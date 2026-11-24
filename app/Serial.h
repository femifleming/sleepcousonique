#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

// Sends a dream to an SP-1 running the Sleepcousonique firmware, over its USB serial port.
//
// Protocol (text, then bytes):
//   host -> SP-1   "SCQ HELLO\n"     SP-1 -> host  "SLEEPCOUSONIQUE <version>\n"
//   host -> SP-1   "SCQ LOAD <bytes> <slot>\n" followed by exactly <bytes> of UTF-8 patch text
//   SP-1 -> host   "OK <errors>\n"  or  "ERR <reason>\n"
// Opening the port also makes the SP-1 print its console banner and replay its stored log, and
// it prints a status line every few seconds: "[ON] batt ..." playing, "[STANDBY] batt ..." when it
// is plugged in but switched off (it lands there after every USB boot, so after flashing too).
// The firmware only answers SCQ commands while it is ON.
namespace serial
{
// The port as the protocol sees it: a real serial port, or (in tests) a pretend SP-1.
struct Wire
{
    virtual ~Wire() = default;
    virtual int  read (char* buf, int n, int timeoutMs) = 0;       // count; 0 nothing came; -1 gone
    virtual int  write (const char* d, int n, int timeoutMs) = 0;  // count taken (0 if none in time); -1 gone
    virtual void flushInput() = 0;
};

std::unique_ptr<Wire> open (const juce::String& port);    // nullptr if it can't be opened (busy, gone)
// Ports that might be an SP-1 (and with wingies, a Wingie: its CH340 USB serial, /dev/cu.usbserial-*,
// /dev/cu.wchusbserial*, Windows VID_1A86&PID_7523). Opening a Wingie's port can reset it (the
// ESP32's auto-reset), so only the editor asks for those; the Array never does.
juce::StringArray candidates (bool wingies = false);
bool mightBeWingie (const juce::String& port);            // a CH340 port (not an SP-1's)

// The two instruments, as the protocol names them
enum Device { noDevice = -1, sp1 = 0, wingie = 1 };
juce::String deviceName (int device);                     // "SP-1", "Wingie"

// For tests: these ports and this way of opening them, instead of the computer's. The
// SCQ_PORTS environment variable (paths separated by ';') also replaces the port list.
using Opener = std::function<std::unique_ptr<Wire> (const juce::String&)>;
void setTestPorts (const juce::StringArray& ports, Opener opener);

// How long each step may take (ms). Tests shorten them.
struct Timing
{
    int logQuiet = 400, logMost = 4000;   // the log replay: over after this much silence / at most
    int helloWait = 1500, helloTries = 3; // 4.5 s of HELLOs: more than the ~2.5 s a Wingie takes to
                                          // start again after its port is opened (the ESP32 resets)
    int writeMost = 8000;                 // a whole dream must be taken within this
    int replyWait = 12000;                // OK / ERR (saving pauses and restarts the audio)
};
extern Timing timing;

// What is on the far end of a port, from what it said when opened and asked SCQ HELLO.
struct Hello
{
    enum What { sleepcousonique, standby, otherFirmware, silent };
    What what = silent;
    juce::String version;     // "1.4.0" (sleepcousonique)
    int device = sp1;         // "SLEEPCOUSONIQUE 1.0.0 WINGIE": a Wingie; nothing after the version: an SP-1
};
Hello greet (Wire&, const std::function<bool()>& stop = {});
// Why a port isn't usable, for a person ("" for sleepcousonique).
juce::String explain (const Hello&, const juce::String& port);
// A dream for one instrument and a device that is the other, for a person ("" if they match,
// or if either is unknown).
juce::String mismatch (int dreamFor, int device, const juce::String& port = {});

// Write a dream ("SCQ LOAD n slot" + its bytes). false if the SP-1 stopped taking it in time.
// `between` is called between chunks with whatever the SP-1 said meanwhile.
bool writeDream (Wire&, const std::string& body, int slot, const std::function<void (const char*, int)>& between = {});

// "" if the SP-1 said OK; else what to tell the person. `sp1Version`: from its HELLO.
juce::String describeReply (const juce::String& reply, const juce::String& sp1Version, int device = sp1);

// One whole send on a port nobody else holds: find an SP-1 (or, for a dream for the Wingie,
// `instrument` 1, a Wingie), greet it, send, wait for the reply. `instrument` -1: whichever
// answers. "" on success, else why not. Blocks for seconds: never call it on the message thread.
juce::String send (const juce::String& text, int slot, juce::String& portUsed, int instrument = -1);

// One command, one reply line: opens the port, writes `line` + newline, returns the first line
// that starts with one of `wanted` (or "" after timeoutMs, or if the port is busy).
juce::String ask (const juce::String& port, const juce::String& line, const juce::StringArray& wanted, int timeoutMs = 600);

// The live link: holds the SP-1's port open while it's plugged in, follows its hands
// ("SCQ HANDS ..." lines, about 30 a second) and sends dreams over the same connection.
// With wingies it looks for a Wingie too (whichever answers first is the one it holds), and
// follows a Wingie's hands ("SCQ WHANDS ...") and its once-a-second report.
class Link : private juce::Thread
{
public:
    explicit Link (bool wingies = false);
    ~Link() override;
    // false (--no-link) before a Link is made: never look for an SP-1 or open its port, for
    // running beside another program that holds it
    static bool enabled;

    struct Hands
    {
        float fader[4] {};
        juce::uint32 buttons = 0;     // bits: doors 0-3, PLAY 4, dots 5, FFWD 6, RWD 7, VOL+ 8, VOL- 9
        int scene = 1, slot = 1;
        float lamp[8] {};
        juce::uint32 count = 0;       // goes up with every report
    };
    struct WHands                     // a Wingie's hands ("SCQ WHANDS ...")
    {
        float fader[3] {};            // Mix, Decay, Volume, 0..1
        juce::uint32 keys[2] {};      // bits 0-11: C..B held, left and right
        juce::uint32 mode = 0;        // bit 0 left Mode, bit 1 right
        int octave[2] {};             // -1 / 0 / 1
        int source = 0;               // 0 microphones, 1 line in
        int scene = 1, slot = 1;
        float lamp[2] {};             // 0..1
        juce::uint32 count = 0;       // goes up with every report
    };
    struct Measured                   // the device's own report ("SCQ engine avg=.. max=..") every 5 s (a Wingie: every second)
    {
        int avg = -1, peak = -1;      // percent of its processor
        float load = -1, loadPeak = -1;   // the same, finer (load= peak=), when it says them
        bool over = false;            // it dropped blocks: the dream is too heavy
        int dropped = -1, blocks = -1;    // blocks it couldn't finish in time, of how many
        int memUsed = -1, memTotal = -1;  // the engine's pool
        // a Wingie's extras (-1 / -100 when the report doesn't have them)
        int core1 = -1;               // its control core, percent
        int heap = -1;                // free heap, bytes
        int inL = -100, inR = -100, outL = -100, outR = -100;   // peak levels over the last second, dBFS (-99 silence)
        juce::uint32 when = 0;        // ms counter of the report
        juce::uint32 count = 0;       // goes up with every report
    };
    // the report's words, for tests and the --hands demo (the link fills `into` from them)
    static bool parseReport (const juce::String& line, Measured& into);
    static bool parseWHands (const juce::String& line, WHands& into);
    bool connected() const { return isConnected.load(); }
    int device() const { return isConnected.load() ? connectedDevice.load() : (int) noDevice; }   // what is connected now
    Measured measured() const;
    // the last thing the device told a person (a dream too heavy and what it went back to); `count`
    // goes up with each, so the editor shows each once
    juce::String lastEvent (juce::uint32& count) const;
    juce::String port() const;
    juce::String version() const;     // the device's firmware, from its HELLO
    // why there is no SP-1 connected, for a person: "" while connected (or still looking)
    juce::String note() const;
    Hands hands() const;
    WHands whands() const;

    // "" on success, else why not. Waits up to waitForSP1Ms for the link to find an SP-1 if
    // it hasn't yet, then up to ~14 s for the reply: never call it on the message thread.
    // instrument (0 SP-1, 1 Wingie): the device the dream is for; a device that is the other one
    // is not sent it, and the answer says so. -1: whichever is connected.
    juce::String send (const juce::String& text, int slot, int waitForSP1Ms = 0, int instrument = -1);

private:
    void run() override;
    bool connect();
    void handle (const juce::String& line);
    void heard (const char* buf, int n);
    void drop();
    void finishSend (const juce::String& result);

    const bool lookForWingies;
    std::unique_ptr<Wire> wire;     // only the link thread uses it
    std::atomic<bool> isConnected { false };
    std::atomic<int> connectedDevice { sp1 };
    juce::String portName, sp1Version, why;
    juce::StringArray notOurs;      // CH340 ports that answered as something else: not opened again (each opening resets it) until replugged
    mutable juce::CriticalSection lock;
    Hands latest;
    WHands latestW;
    Measured meas;
    juce::String event;
    juce::uint32 eventCount = 0;
    juce::String partial;

    // a send handed to the link thread
    juce::CriticalSection sendLock, oneSend;   // oneSend: a second send waits for the first
    juce::String pendingText, sendResult;
    int pendingSlot = -1;
    bool sendRequested = false, awaitingReply = false;
    juce::uint32 replyDeadline = 0, sendTicket = 0, doneTicket = 0;
    juce::WaitableEvent sendDone;
};
}
