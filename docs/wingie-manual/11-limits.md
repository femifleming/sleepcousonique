## 11. Limits and numbers

### The Wingie

| | |
|---|---|
| processor | ESP32, two cores at 240 MHz: one plays the dream, the other reads the panel and the USB |
| sound | 48 kHz, stereo in and out, 64-frame blocks (two engine blocks of 32: 0.7 ms each) |
| a sample's budget | 5000 cycles |
| memory pool | **70 KB a scene** (measured 71,668 bytes on a Wingie without PSRAM; a Wingie with PSRAM gets 512 KB) |
| places, passages | 96 places and 320 passages a scene, as on the SP-1 |
| scenes | up to 16 a dream |
| slots | 12, of up to 16 KB of text each |
| USB | serial at 115200: the editor's link, and a report every second |
| MIDI | the TRS MIDI input feeds `midi`, `midicc` and `midiclock` |

### Memory is usually the limit

Measured on the user's Wingie: an empty dream uses **2.8 %** of the processor, first light about
**30 %**, four reverbs **38 %**. The processor has room. The memory pool runs out first: a grain,
time or spectral place keeps a tape, and two of them rarely fit one scene. What to do:

- **one big tape a scene**, and the light places around it;
- **shorter natures**: a grain or time place's nature is its tape in tenths of a second;
- **`samplerate=half`** (or `quarter`): the same memory holds twice (four times) the seconds, at
  half the work;
- **another scene**: every scene has its own 70 KB, and the Mode buttons step between them.

When a scene doesn't fit, the check says *the house is full: no room for …* in the editor, and
the scene never reaches the Wingie.

### What things cost

Measured on a Wingie (engine/scq_costs_wingie.h), beside the SP-1's figures. The median place
costs the Wingie <!-- scale --> times the SP-1's cycles, but the Wingie has 3.75 times as many cycles a
sample, so most dreams have more room on the Wingie; places without a measurement are estimated
at the SP-1's figure times <!-- scale -->. The table is read from the engine when this manual is
made, so it follows new measurements.

<!-- costs sine vco reverb reverblite comb sympathetic wingie cave listen cocoquantus relabi rollz dust grains stretch paulstretch psola harmonizer specfreeze shimmer cavern eis tapeloop dub limiter -->

### The report

Once a second the Wingie sends one line over USB, which the editor's meters read (for example):

    SCQ engine avg=31% max=44%  places=12 scene=1 slot=3  load=31.2 peak=44.0 dropped=0 of 1500
    mem=40128/71668 core1=4% heap=122880 inL=-38 inR=-41 outL=-18 outR=-19

| field | |
|---|---|
| `avg`, `max` | the audio core's work over the last second, average and worst block |
| `places`, `scene`, `slot` | what is playing |
| `dropped=n of m` | blocks that were late, of how many; any at all is a stutter |
| `mem=used/total` | the engine's pool |
| `core1` | the control core |
| `heap` | free memory left on the ESP32 |
| `inL` … `outR` | peak levels in dBFS; −99 is silence |

### Too heavy

Four late blocks within half a second count as too heavy. The Wingie goes back to the last
scene or dream that played two seconds without a late block, else to first light (chapter 4).
