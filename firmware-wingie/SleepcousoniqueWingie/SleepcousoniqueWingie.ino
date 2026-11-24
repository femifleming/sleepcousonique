// Sleepcousonique on the Meng Qi Wingie2.
//
// The same engine as the SP-1 (engine/, copied into src/engine by tools/build_wingie.sh), built
// for the Wingie: dreams that start with `instrument wingie` play here, with the Wingie's hands
// as places (mic and line: whichever the Line/Mic switch picks, keys1 keys2, key, mode1 mode2, octave1 octave2, source, fader1-3, lights).
// Pins, codec setup and I2S are Meng Qi's (Wingie2 firmware, GPLv3).
//
// Panel:      both Mode buttons held + a key on the bottom keyboard n      -> dream slot n (12 slots)
//             hold left Mode, tap right Mode             -> next scene
//             hold right Mode, tap left Mode             -> previous scene
//             (while both are down the dream sees neither Mode button nor the keys)
//             both Mode buttons held at power-up         -> back to the first dream in slot 1
// USB serial (115200): the SP-1's protocol, and a little more
//   "SCQ HELLO"            -> "SLEEPCOUSONIQUE <version> WINGIE"
//   "SCQ LOAD <bytes> <slot>" + bytes -> "OK <errors>" / "ERR <why>"     slot 0-11
//   "SCQ TRY <bytes>" + bytes        -> plays it without keeping it
//   "SCQ SLOT <n>"         -> plays slot n (1-12)
//   "SCQ MEASURE <n>"      -> "SCQ MEASURED <avg> <max> <cycles a block has> <blocks> <dropped> <mem used> <mem>"
//   "SCQ WATCH 1|0"        -> "SCQ WHANDS f1 f2 f3 keys1 keys2 modes oct1 oct2 source scene slot lampL lampR" ~30/s
//   "SCQ WHO"              -> "SCQ ME <mac> 0 DREAM"
//   every second: "SCQ engine avg=..% max=..%  places=.. scene=.. slot=..  load=a.b peak=c.d dropped=n of m
//                  mem=u/t core1=..% heap=.. inL=.. inR=.. outL=.. outR=.."   (levels in dBFS, -99 = silence)

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <Adafruit_AW9523.h>
#include "driver/i2s.h"
#include "driver/i2c.h"
#include "soc/io_mux_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "xtensa/core-macros.h"
#include "AC101.h"
#include "esp_heap_caps.h"

extern "C" {
#include "src/engine/scq.h"
}
#include "first_light.h"   // the dream a new Wingie starts with (generated from examples/wingie-first-light.dream)

#define SCQ_WINGIE_VERSION "1.5.0"
#define SLOTS 12
#define RATE 48000
#define BLOCK 64                                   // I2S frames per audio block (two engine blocks)
static const uint32_t BLOCK_CYCLES = 240000000u / RATE * BLOCK;   // 320 000

// ------------------------------------------------------------------ hardware (stock pins)
static AC101 ac;
static Adafruit_AW9523 aw0, aw1;
static const int lOctPin[2] = {6, 5}, rOctPin[2] = {13, 12}, modePin[2] = {4, 7}, sourcePin = 14;
static const int intn[2] = {23, 5}, rstn[2] = {22, 19};
static const int potPin[3] = {34, 39, 36};
static const int ledPin[2][2] = {{14, 13}, {21, 15}};
static const uint16_t SOURCES[2] = {0x1040, 0x0408};   // MIC, LINE
#define MIC_BOOST 0xAAC4
#define DAC_VOL 0xA2A2

static void acWriteReg(uint8_t reg, uint16_t val) {
  uint8_t buf[2] = {uint8_t(val >> 8), uint8_t(val & 0xff)};
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (AC101_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
  i2c_master_write(cmd, buf, 2, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin((i2c_port_t)I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
}

// ------------------------------------------------------------------ the engine and its text
static SemaphoreHandle_t engineLock;
static char *textBuf[2];                // the playing dream and the one arriving (the engine reads it in place); heap
static int playing = 0;                 // which textBuf the engine reads
static int playingLen = 0;
static int slot = 0;
static uint32_t nights = 0;
static volatile bool engineReady = false;

// MIDI bytes from the TRS jack, handed to the audio task (which owns the engine)
static uint8_t midiRing[256];
static volatile uint8_t midiHead = 0, midiTail = 0;

// meters (audio task writes, loop reads)
static volatile uint32_t engSum = 0, engN = 0, engMax = 0, engDropped = 0;
static volatile uint32_t lateTotal = 0;
static bool fallbackOn = true;               // SCQ FALLBACK 0: the benchmark measures a heavy dream as it is        // late blocks since boot (never reset): the overload watch
static volatile float peakIn[2] = {0, 0}, peakOut[2] = {0, 0};
static volatile uint32_t mLeft = 0, mSum = 0, mN = 0, mMax = 0, mDropped = 0;
static volatile bool mDone = false;
static volatile uint32_t core1Busy = 0;   // cycles the control task worked this second

// ------------------------------------------------------------------ slots (LittleFS)
static void slotPath(int s, char *p) { snprintf(p, 24, "/scq/slot%d.dream", s); }
static int slotRead(int s, char *buf) {
  char p[24]; slotPath(s, p);
  File f = LittleFS.open(p, "r");
  if (!f) return -1;
  int n = f.read((uint8_t *)buf, SCQ_MAX_TEXT - 1);
  f.close();
  if (n < 0) n = 0;
  buf[n] = 0;
  return n;
}
static bool slotWrite(int s, const char *text, int len) {
  char p[24]; slotPath(s, p);
  File f = LittleFS.open(p, "w");
  if (!f) return false;
  int n = f.write((const uint8_t *)text, len);
  f.close();
  return n == len;
}
static void stateWrite() {
  File f = LittleFS.open("/scq/state", "w");
  if (f) { f.printf("%d %u\n", slot, (unsigned)nights); f.close(); }
}

// Load text (already in textBuf[buf]) into the engine. Returns the number of errors; on errors
// the dream that was playing comes back.
static int loadBuffer(int buf, int len) {
  xSemaphoreTake(engineLock, portMAX_DELAY);
  int e = scq_load(textBuf[buf], len, nights);
  if (e == 0) { playing = buf; playingLen = len; }
  else {
    String msgs = scq_messages();
    if (playingLen > 0) scq_load(textBuf[playing], playingLen, nights);
    xSemaphoreGive(engineLock);
    Serial.print("SCQ mistakes:\n"); Serial.print(msgs);
    return e;
  }
  xSemaphoreGive(engineLock);
  return 0;
}

static void playSlot(int s) {
  int other = 1 - playing;
  int n = slotRead(s, textBuf[other]);
  if (n <= 0) {   // empty slot: the first light
    n = (int)strlen(FIRST_LIGHT);
    memcpy(textBuf[other], FIRST_LIGHT, n + 1);
    Serial.printf("SCQ slot %d is empty: playing the first light\n", s + 1);
  }
  int e = loadBuffer(other, n);
  slot = s;
  Serial.printf("SCQ slot %d: %d bytes, %d mistakes\n", s + 1, n, e);
}

// ------------------------------------------------------------------ audio task (core 0)
static void i2sInit() {
  i2s_pin_config_t pins = {};
  pins.bck_io_num = 27; pins.ws_io_num = 26; pins.data_out_num = 25; pins.data_in_num = 35;
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  cfg.sample_rate = RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL3;
  cfg.dma_buf_count = 3;
  cfg.dma_buf_len = BLOCK;
  cfg.use_apll = true;
  esp_err_t e1 = i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  esp_err_t e2 = i2s_set_pin(I2S_NUM_0, &pins);
  if (e1 != ESP_OK || e2 != ESP_OK) Serial.printf("I2S: install %d, pins %d\n", (int)e1, (int)e2);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
  REG_WRITE(PIN_CTRL, 0xFFFFFFF0);
}

static void audioTask(void *) {
  static int32_t rx[BLOCK * 2], tx[BLOCK * 2];
  static float inL[BLOCK], inR[BLOCK], outL[BLOCK], outR[BLOCK];
  const float DIV = 4.6566129e-10f, MUL = 2147483647.0f;
  float pi0 = 0, pi1 = 0, po0 = 0, po1 = 0;
  uint32_t secBlocks = 0;
  for (;;) {
    size_t n = 0;
    i2s_read(I2S_NUM_0, rx, sizeof rx, &n, portMAX_DELAY);
    for (int i = 0; i < BLOCK; i++) {
      inL[i] = rx[2 * i] * DIV; inR[i] = rx[2 * i + 1] * DIV;
      float a = fabsf(inL[i]), b = fabsf(inR[i]);
      if (a > pi0) pi0 = a; if (b > pi1) pi1 = b;
    }
    uint32_t c0 = XTHAL_GET_CCOUNT();
    bool ran = false;
    if (engineReady && xSemaphoreTake(engineLock, 0) == pdTRUE) {
      while (midiTail != midiHead) {   // MIDI bytes that arrived since the last block
        uint8_t b = midiRing[midiTail];
        scq_midi(&b, 1);
        midiTail = (uint8_t)(midiTail + 1);
      }
      scq_input(inL, inR);
      scq_render(outL, outR, BLOCK);
      xSemaphoreGive(engineLock);
      ran = true;
    }
    if (!ran) for (int i = 0; i < BLOCK; i++) outL[i] = outR[i] = 0.f;   // loading: a moment of silence
    uint32_t used = XTHAL_GET_CCOUNT() - c0;
    for (int i = 0; i < BLOCK; i++) {
      float l = outL[i] > 1.f ? 1.f : (outL[i] < -1.f ? -1.f : outL[i]);
      float r = outR[i] > 1.f ? 1.f : (outR[i] < -1.f ? -1.f : outR[i]);
      if (fabsf(l) > po0) po0 = fabsf(l); if (fabsf(r) > po1) po1 = fabsf(r);
      tx[2 * i] = (int32_t)(l * MUL); tx[2 * i + 1] = (int32_t)(r * MUL);
    }
    if (ran) {
      engSum += used; engN++;
      if (used > engMax) engMax = used;
      if (used > BLOCK_CYCLES) { engDropped++; lateTotal++; }   // late: the DMA ran dry, a glitch was heard
      if (mLeft) {
        mSum += used; mN++; if (used > mMax) mMax = used; if (used > BLOCK_CYCLES) mDropped++;
        if (--mLeft == 0) mDone = true;
      }
    }
    if (++secBlocks >= RATE / BLOCK) {   // once a second: levels for the meters
      secBlocks = 0;
      peakIn[0] = pi0; peakIn[1] = pi1; peakOut[0] = po0; peakOut[1] = po1;
      pi0 = pi1 = po0 = po1 = 0;
    }
    size_t w = 0;
    i2s_write(I2S_NUM_0, tx, sizeof tx, &w, portMAX_DELAY);
  }
}

// ------------------------------------------------------------------ panel (control task, core 1)
static volatile bool keyIrq[2] = {false, false};
static void IRAM_ATTR irq0() { keyIrq[0] = true; }
static void IRAM_ATTR irq1() { keyIrq[1] = true; }
static bool keyState[2][12], modeState[2];
static int octState[2] = {0, 0};
static bool srcState = false;
static volatile int panelFader[3] = {0, 0, 0}, panelOct[2] = {0, 0}, panelSource = 0;
static volatile uint16_t panelKeys[2] = {0, 0};
static volatile uint8_t panelModes = 0;
static volatile int wantSlot = -1, wantScene = 0;   // panel combos, done by the loop task
static int volumeLevel = 0;

static void readKeysAw0() {
  for (int i = 0; i < 16; i++) {
    bool v = !aw0.digitalRead(i);
    if (i < 8) keyState[0][7 - i] = v;
    else if (i < 12) keyState[0][11 - i + 8] = v;
    else keyState[1][i - 12] = v;
  }
}
static void readKeysAw1() {
  for (int i = 0; i < 4; i++) keyState[1][3 - i + 4] = !aw1.digitalRead(i);
  for (int i = 8; i < 12; i++) keyState[1][11 - i + 8] = !aw1.digitalRead(i);
  modeState[0] = !aw1.digitalRead(modePin[0]);
  modeState[1] = !aw1.digitalRead(modePin[1]);
  octState[0] = -(int)!aw1.digitalRead(lOctPin[0]) + (int)!aw1.digitalRead(lOctPin[1]);
  octState[1] = -(int)!aw1.digitalRead(rOctPin[0]) + (int)!aw1.digitalRead(rOctPin[1]);
  srcState = !aw1.digitalRead(sourcePin);
}

// lamp 0..1 -> the LED's colour code (stock: red 0, purple 1, yellow 2, white 3)
static uint8_t lampCode(float v) { return v < 0.25f ? 0 : v < 0.5f ? 1 : v < 0.75f ? 2 : 3; }
static void setLed(int s, uint8_t code) { for (int b = 0; b < 2; b++) digitalWrite(ledPin[s][b], !((code >> b) & 1)); }

static uint32_t flashUntil = 0;
static uint8_t flashCode = 3;
static uint32_t flashMs = 120;   // blink half-period (mistakes flicker faster)

static void controlTask(void *) {
  Wire.begin(17, 18);
  for (int i = 0; i < 2; i++) { pinMode(rstn[i], OUTPUT); digitalWrite(rstn[i], HIGH); pinMode(intn[i], INPUT); }
  delay(5);
  if (!aw0.begin(0x58)) Serial.println("AW9523 0: not found");
  if (!aw1.begin(0x59)) Serial.println("AW9523 1: not found");
  aw0.reset(); aw1.reset();
  for (int i = 0; i < 16; i++) { aw0.pinMode(i, INPUT); aw0.enableInterrupt(i, true); aw1.pinMode(i, INPUT); aw1.enableInterrupt(i, true); }
  for (int i = 0; i < 16; i += 8) { aw0.digitalRead(i); aw1.digitalRead(i); }
  attachInterrupt(digitalPinToInterrupt(intn[0]), irq0, FALLING);
  attachInterrupt(digitalPinToInterrupt(intn[1]), irq1, FALLING);
  for (int s = 0; s < 2; s++) for (int b = 0; b < 2; b++) pinMode(ledPin[s][b], OUTPUT);
  readKeysAw0(); readKeysAw1();

  uint32_t lastRefresh = 0, lastVol = 0, sourceAt = 0;
  bool sourceMuted = false;
  bool both = false, comboUsed = false;
  bool prevMode[2] = {false, false};
  bool lineIn = !aw1.digitalRead(sourcePin);
  srcState = lineIn;
  acWriteReg(ADC_SRC, SOURCES[lineIn]);          // the Line/Mic switch as it is at power-up (the codec starts on the mics)
  panelSource = lineIn;
  scq_control(SCQ_CTL_SOURCE, lineIn ? 1.f : 0.f);
  for (;;) {
    uint32_t c0 = XTHAL_GET_CCOUNT();
    uint32_t now = millis();
    if (volumeLevel < 63 && now - lastVol > 25 && engineReady) { lastVol = now; ac.SetVolumeHeadphone(++volumeLevel); }
    if (keyIrq[0]) { keyIrq[0] = false; readKeysAw0(); }
    if (keyIrq[1]) { keyIrq[1] = false; readKeysAw1(); }
    if (now - lastRefresh > 500) { lastRefresh = now; readKeysAw0(); readKeysAw1(); }

    for (int i = 0; i < 3; i++) {
      int v = analogRead(potPin[i]);
      panelFader[i] = v;
      scq_control(SCQ_CTL_LEAN1 + i, v / 4095.f);
    }
    int o1 = octState[0], o2 = octState[1];
    panelOct[0] = o1; panelOct[1] = o2;
    scq_control(SCQ_CTL_OCTAVE1, (float)o1); scq_control(SCQ_CTL_OCTAVE2, (float)o2);

    bool src = srcState;
    if (src != lineIn && !sourceMuted) {
      lineIn = src; ac.SetVolumeHeadphone(0); sourceMuted = true; sourceAt = now;
      acWriteReg(ADC_SRC, SOURCES[lineIn]);
      panelSource = lineIn; scq_control(SCQ_CTL_SOURCE, lineIn ? 1.f : 0.f);
    }
    if (sourceMuted && now - sourceAt > 55) { sourceMuted = false; ac.SetVolumeHeadphone(volumeLevel); }

    // Mode combos: both down -> slot by a key on the bottom keyboard; one held + tap the other -> scene
    bool m0 = modeState[0], m1 = modeState[1];
    if (m0 && m1) {
      if (!both) { both = true; comboUsed = false; }
      for (int k = 0; k < 12; k++) if (keyState[0][k]) { wantSlot = k; comboUsed = true; }
    }
    if (!m0 && !m1) both = false;
    // a tap of one Mode while the other is held
    if (prevMode[1] && !m1 && m0 && !comboUsed) { wantScene = +1; comboUsed = true; }
    if (prevMode[0] && !m0 && m1 && !comboUsed) { wantScene = -1; comboUsed = true; }
    prevMode[0] = m0; prevMode[1] = m1;

    uint16_t k1 = 0, k2 = 0;
    for (int k = 0; k < 12; k++) { if (keyState[0][k]) k1 |= 1 << k; if (keyState[1][k]) k2 |= 1 << k; }
    panelKeys[0] = k1; panelKeys[1] = k2;
    panelModes = (m0 ? 1 : 0) | (m1 ? 2 : 0);
    bool hide = both;   // while both are down the dream sees neither Mode nor the keys
    for (int k = 0; k < 12; k++) {
      scq_control(SCQ_CTL_KEYS1 + k, (!hide && keyState[0][k]) ? 1.f : 0.f);
      scq_control(SCQ_CTL_KEYS2 + k, (!hide && keyState[1][k]) ? 1.f : 0.f);
    }
    // the Mode buttons as the dream sees them: a combo freezes them (the one held first stays
    // held, the second never shows), and a button let go after a combo is let go only once it
    // has been down 0.65 s, so the dream never sees a quick tap
    {
      static bool shown[2] = {false, false}, combo[2] = {false, false};
      static uint32_t downAt[2] = {0, 0};
      static bool physPrev[2] = {false, false};
      bool phys[2] = {m0, m1};
      for (int i = 0; i < 2; i++) {
        if (phys[i] && !physPrev[i]) downAt[i] = now;
        if (both) combo[i] = true;
        if (!both && !combo[i]) shown[i] = phys[i];                         // ordinary use
        else if (both) { /* frozen: keep what the dream saw */ }
        else if (!phys[i] && shown[i]) { if (now - downAt[i] >= 650) shown[i] = false; }   // let go after a combo
        if (!phys[i] && !shown[i]) combo[i] = false;
        physPrev[i] = phys[i];
      }
      scq_control(SCQ_CTL_MODE1, shown[0] ? 1.f : 0.f);
      scq_control(SCQ_CTL_MODE2, shown[1] ? 1.f : 0.f);
    }

    // LEDs: the dream's lights, or a flash after a slot / scene change / mistakes
    if ((int32_t)(flashUntil - now) > 0) { bool on = (now / flashMs) & 1; setLed(0, on ? flashCode : 0); setLed(1, on ? flashCode : 0); }
    else { setLed(0, lampCode(scq_lamp(0))); setLed(1, lampCode(scq_lamp(1))); }

    core1Busy += XTHAL_GET_CCOUNT() - c0;
    vTaskDelay(2 / portTICK_PERIOD_MS);
  }
}

// ------------------------------------------------------------------ serial
static char line[96];
static int lineN = 0;
static int want = -1, got = 0, wantSlotLoad = -1, rxBuf = 1;
static bool watching = false;

static void handleLine() {
  line[lineN] = 0;
  if (!strncmp(line, "SCQ HELLO", 9)) {
    Serial.print("SLEEPCOUSONIQUE " SCQ_WINGIE_VERSION " WINGIE\n");
  } else if (!strncmp(line, "SCQ LOAD ", 9) || !strncmp(line, "SCQ TRY ", 8)) {
    bool tryOnly = line[4] == 'T';
    char *end;
    long n = strtol(line + (tryOnly ? 8 : 9), &end, 10);
    long s = tryOnly ? -1 : strtol(end, NULL, 10);
    if (n <= 0 || n >= SCQ_MAX_TEXT) Serial.print("ERR too big\n");
    else { want = (int)n; got = 0; rxBuf = 1 - playing; wantSlotLoad = tryOnly ? -1 : ((s >= 0 && s < SLOTS) ? (int)s : slot); }
  } else if (!strncmp(line, "SCQ SLOT ", 9)) {
    int s = atoi(line + 9) - 1;
    if (s >= 0 && s < SLOTS) { playSlot(s); stateWrite(); }
    Serial.print("OK 0\n");
  } else if (!strncmp(line, "SCQ WATCH ", 10)) {
    watching = line[10] == '1';
  } else if (!strncmp(line, "SCQ MEASURE", 11)) {
    long b = strtol(line + 11, NULL, 10);
    mSum = 0; mN = 0; mMax = 0; mDropped = 0; mDone = false;
    mLeft = (uint32_t)(b > 0 && b <= 100000 ? b : 1000);
  } else if (!strncmp(line, "SCQ WHO", 7)) {
    Serial.printf("SCQ ME %s 0 DREAM\n", WiFi.macAddress().c_str());
  } else if (!strncmp(line, "SCQ FALLBACK ", 13)) {
    fallbackOn = line[13] != '0';
    Serial.print("OK 0\n");
  } else if (!strncmp(line, "SCQ SHOW", 8)) {
    flashUntil = millis() + 3000; flashCode = 3; flashMs = 120;
    Serial.print("OK 0\n");
  } else if (!strncmp(line, "SCQ MODE", 8) || !strncmp(line, "SCQ UNIT", 8)) {
    Serial.print("OK DREAM\n");   // the Wingie has no interface mode or array unit
  }
  lineN = 0;
}

static void serialPoll() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c < 0) break;
    if (want >= 0) {
      textBuf[rxBuf][got++] = (char)c;
      if (got < want) continue;
      textBuf[rxBuf][got] = 0;
      want = -1;
      int e = loadBuffer(rxBuf, got);
      if (e) { Serial.printf("ERR %d mistakes\n", e); flashUntil = millis() + 900; flashCode = 3; flashMs = 50; continue; }   // mistakes: a fast white flicker
      if (wantSlotLoad < 0) { Serial.print("OK 0\n"); continue; }
      slot = wantSlotLoad;
      bool ok = slotWrite(slot, textBuf[playing], got);
      stateWrite();
      if (ok) Serial.print("OK 0\n"); else Serial.print("ERR could not save (playing it anyway)\n");
      Serial.printf("SCQ received %d bytes into slot %d\n", got, slot + 1);
      flashUntil = millis() + 500; flashCode = 3; flashMs = 120;
      continue;
    }
    if (c == '\n' || c == '\r') { if (lineN) handleLine(); }
    else if (lineN < (int)sizeof line - 1) line[lineN++] = (char)c;
  }
}

static int dB(float peak) { if (peak < 1e-5f) return -99; int d = (int)lroundf(20.f * log10f(peak)); return d < -99 ? -99 : d; }

// ------------------------------------------------------------------ setup / loop
static uint32_t poolBytes = 0;
// both tasks' stacks are static: on the heap they would split the block the pool needs
static StackType_t audioStack[8192], controlStack[8192];
static StaticTask_t audioTcb, controlTcb;
static size_t largestAtBoot = 0;
void setup() {
  i2sInit();   // first: its DMA buffers need internal memory before the pool takes the big block
  engineLock = xSemaphoreCreateMutex();
  // the audio task too (it plays silence until the engine is ready): its stack must not lose
  // the race for memory against the pool
  bool audioOk = xTaskCreateStaticPinnedToCore(audioTask, "audio", 8192, nullptr, 24, audioStack, &audioTcb, 0) != nullptr;
  // the two dream buffers (16 KB each), then the pool from the biggest block left
  textBuf[0] = (char *)malloc(SCQ_MAX_TEXT + 1);
  textBuf[1] = (char *)malloc(SCQ_MAX_TEXT + 1);
  // the engine's pool, while the heap is still in one piece: the biggest block there is,
  // up to the SP-1's 124 KB (PSRAM, if this Wingie has any, holds much more)
  {
    largestAtBoot = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    void *pool = nullptr;
    size_t want = 124 * 1024;
    if (psramFound()) { want = 512 * 1024; pool = heap_caps_malloc(want, MALLOC_CAP_SPIRAM); }
    if (!pool) {
      want = 124 * 1024;
      if (want > largestAtBoot - 6144) want = (largestAtBoot - 6144) & ~3u;   // room for the tasks and LittleFS
      pool = heap_caps_malloc(want, MALLOC_CAP_8BIT);
    }
    scq_pool(pool, (uint32_t)want);
    poolBytes = want;
  }
  Serial.setRxBufferSize(4096);                   // dreams stream in while the loop task reads them
  Serial.begin(115200);
  Serial2.begin(31250, SERIAL_8N1, 16, -1);       // TRS MIDI in (RX only: TX2 would clash with I2C)
  WiFi.mode(WIFI_MODE_NULL);
  btStop();
  if (!audioOk) Serial.println("audio task: could not start");
  if (!LittleFS.begin(true)) Serial.println("LittleFS: mount failed");
  LittleFS.mkdir("/scq");
  File f = LittleFS.open("/scq/state", "r");
  if (f) { int s = 0; unsigned nn = 0; if (sscanf(f.readString().c_str(), "%d %u", &s, &nn) >= 1) { slot = s % SLOTS; nights = nn; } f.close(); }
  nights++;

  // codec (stock sequence)
  esp_err_t err = ESP_FAIL;
  for (int a = 0; a < 5 && err != ESP_OK; a++) { err = ac.begin(); if (err != ESP_OK) delay(100); }
  if (err != ESP_OK) Serial.println("AC101: init failed, continuing");
  acWriteReg(ADC_SRCBST_CTRL, MIC_BOOST);
  acWriteReg(OMIXER_BST1_CTRL, 0x56DB);
  ac.SetVolumeHeadphone(0);
  ac.SetVolumeSpeaker(0);
  acWriteReg(OMIXER_SR, 0x0102);
  acWriteReg(DAC_VOL_CTRL, DAC_VOL);
  acWriteReg(ADC_SRC, SOURCES[0]);

  xTaskCreateStaticPinnedToCore(controlTask, "control", 8192, nullptr, 1, controlStack, &controlTcb, 1);
  delay(50);
  // both Mode buttons at power-up: back to the first dream in slot 1
  if (modeState[0] && modeState[1]) { char p[24]; slotPath(0, p); LittleFS.remove(p); slot = 0; Serial.println("SCQ slot 1 back to the first light"); }
  playSlot(slot);
  stateWrite();
  engineReady = true;

  Serial.printf("Sleepcousonique %s on the Wingie: slot %d, night %u\n", SCQ_WINGIE_VERSION, slot + 1, (unsigned)nights);
  Serial.printf("SCQ pool %u bytes (largest free block at boot %u, PSRAM %u)\n", (unsigned)poolBytes, (unsigned)largestAtBoot, (unsigned)ESP.getPsramSize());
}

void loop() {
  static uint32_t lastWatch = 0, lastReport = 0, lastCore1 = 0;
  // MIDI in -> the audio task
  while (Serial2.available()) {
    uint8_t b = (uint8_t)Serial2.read();
    uint8_t h = midiHead, nh = (uint8_t)(h + 1);
    if (nh != midiTail) { midiRing[h] = b; midiHead = nh; }
  }
  serialPoll();
  // scene changes asked for by the dream are compiled here, off the audio task
  if (xSemaphoreTake(engineLock, 0) == pdTRUE) { scq_service(); xSemaphoreGive(engineLock); }
  // ---- the last dream that fit: a dream (and scene) that has played 2 s without a late block
  {
    static uint32_t lastLate = 0, calmSince = 0, lateWindowStart = 0, lateInWindow = 0;
    static int goodBuf = -1, goodLen = 0, goodScene = 0;
    static const char *goodText = nullptr;
    static int watchBuf = -1, watchScene = -1;
    static uint32_t lastSaid = 0;
    uint32_t t = millis(), late = lateTotal;
    // calm is counted from when this dream and scene started: a dream that has just arrived
    // hasn't fitted yet
    static uint32_t startedAt = 0;
    if (playing != watchBuf || scq_scene() != watchScene || want >= 0) { watchBuf = playing; watchScene = scq_scene(); calmSince = t; startedAt = t; lateInWindow = 0; }
    // the first 0.6 s of a dream or scene don't count: tapes and tables are being laid out
    if (late != lastLate) {
      if (t - lateWindowStart > 500) { lateWindowStart = t; lateInWindow = 0; }
      lateInWindow += late - lastLate;
      lastLate = late;
      calmSince = t;
      if (lateInWindow >= 4 && fallbackOn && t - startedAt > 600) {   // too heavy: four glitches in half a second
        lateInWindow = 0;
        int cur = scq_scene();
        if (goodBuf == playing && goodText == textBuf[playing] && goodScene != cur && goodScene < scq_scene_count()) {
          xSemaphoreTake(engineLock, portMAX_DELAY); scq_goto_scene(goodScene); xSemaphoreGive(engineLock);
          if (t - lastSaid > 5000) { lastSaid = t; Serial.printf("SCQ too heavy for the Wingie: back to scene %d, the last that fit\n", goodScene + 1); }
        } else if (goodBuf >= 0 && goodBuf != playing && goodLen > 0) {
          loadBuffer(goodBuf, goodLen);
          if (goodScene > 0) { xSemaphoreTake(engineLock, portMAX_DELAY); scq_goto_scene(goodScene); xSemaphoreGive(engineLock); }
          if (t - lastSaid > 5000) { lastSaid = t; Serial.print("SCQ too heavy for the Wingie: back to the last dream that fit\n"); }
        } else {
          // nothing kept that fitted: the first light always does
          int other = 1 - playing;
          int n = (int)strlen(FIRST_LIGHT);
          memcpy(textBuf[other], FIRST_LIGHT, n + 1);
          loadBuffer(other, n);
          if (t - lastSaid > 5000) { lastSaid = t; Serial.print("SCQ too heavy for the Wingie: back to the first light\n"); }
        }
        flashUntil = t + 900; flashCode = 1; flashMs = 120;
      }
    } else if (t - calmSince > 2000) {
      goodBuf = playing; goodLen = playingLen; goodText = textBuf[playing]; goodScene = scq_scene();
    }
    // a new dream arriving into the other buffer overwrites what was kept there
    if (want >= 0 && goodBuf == rxBuf) goodBuf = -1;
  }
  // ---- a dream asking for another slot (the slot place)
  {
    int s = -1, st = 0;
    xSemaphoreTake(engineLock, portMAX_DELAY);
    int asked = scq_wanted_slot(&s, &st);
    xSemaphoreGive(engineLock);
    if (asked) {
      int to = s >= 0 ? s : (slot + st + SLOTS) % SLOTS;
      if (to >= 0 && to < SLOTS && to != slot) { playSlot(to); stateWrite(); flashUntil = millis() + 400; flashCode = 2; flashMs = 120; }
    }
  }
  int ws = wantSlot;
  if (ws >= 0) { wantSlot = -1; if (ws != slot) { playSlot(ws); stateWrite(); } flashUntil = millis() + 400; flashCode = 2; flashMs = 120; }
  int sc = wantScene;
  if (sc) {
    wantScene = 0;
    xSemaphoreTake(engineLock, portMAX_DELAY);
    int n = scq_scene_count(), cur = scq_scene();
    if (n > 1) scq_goto_scene((cur + sc + n) % n);
    xSemaphoreGive(engineLock);
  }
  uint32_t now = millis();
  if (watching && now - lastWatch >= 33) {
    lastWatch = now;
    Serial.printf("SCQ WHANDS %d %d %d %03x %03x %u %d %d %d %d %d %02x %02x\n",
                  panelFader[0], panelFader[1], panelFader[2], panelKeys[0], panelKeys[1], panelModes,
                  panelOct[0], panelOct[1], panelSource, scq_scene() + 1, slot + 1,
                  (unsigned)(scq_lamp(0) * 255.f), (unsigned)(scq_lamp(1) * 255.f));
  }
  if (mDone) {
    mDone = false;
    uint32_t used = 0, total = 0; scq_memory(&used, &total);
    Serial.printf("SCQ MEASURED %u %u %u %u %u %u %u\n", (unsigned)(mN ? mSum / mN : 0), (unsigned)mMax,
                  (unsigned)BLOCK_CYCLES, (unsigned)mN, (unsigned)mDropped, (unsigned)used, (unsigned)total);
  }
  if (now - lastReport >= 1000) {
    uint32_t span = now - lastReport;
    lastReport = now;
    uint32_t n = engN, sum = engSum, mx = engMax, dr = engDropped;
    engN = 0; engSum = 0; engMax = 0; engDropped = 0;
    uint32_t c1 = core1Busy; core1Busy = 0;
    unsigned a10 = n ? (unsigned)((uint64_t)sum * 1000u / ((uint64_t)n * BLOCK_CYCLES)) : 0u;
    unsigned m10 = (unsigned)((uint64_t)mx * 1000u / BLOCK_CYCLES);
    unsigned c1p = (unsigned)((uint64_t)c1 * 100u / ((uint64_t)span * 240000u + 1));
    (void)lastCore1;
    uint32_t used = 0, total = 0; scq_memory(&used, &total);
    Serial.printf("SCQ engine avg=%u%% max=%u%%  places=%d scene=%d slot=%d  load=%u.%u peak=%u.%u dropped=%u of %u mem=%u/%u core1=%u%% heap=%u inL=%d inR=%d outL=%d outR=%d%s\n",
                  a10 / 10u, m10 / 10u, scq_node_count(), scq_scene() + 1, slot + 1, a10 / 10u, a10 % 10u, m10 / 10u, m10 % 10u,
                  (unsigned)dr, (unsigned)n, (unsigned)used, (unsigned)total, c1p, (unsigned)ESP.getFreeHeap(),
                  dB(peakIn[0]), dB(peakIn[1]), dB(peakOut[0]), dB(peakOut[1]), dr ? "  OVER BUDGET: blocks were late" : "");
  }
  delay(1);
}
