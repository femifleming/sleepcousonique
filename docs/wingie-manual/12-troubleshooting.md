## 12. Troubleshooting

| What happens | Why, and what to do |
|---|---|
| esptool can't find a port | a charge-only cable, or the port is held: quit the editor, Meng Qi's pages and serial monitors. Try another cable or USB port |
| esptool can't connect | hold BOOT, tap RESET/EN, let go of BOOT as it starts connecting |
| it plays first light, not my dream | the slot is empty, or both Mode buttons were down at power-up (that clears slot 1) |
| nothing comes out | the dream sends nothing to `out`, `left` or `right`; or a fader the dream uses as volume is down (Volume is only a volume if the dream says so) |
| it howls | a resonator listening to its own speaker, as on the stock Wingie: use headphones, lower the decay, or put a `limiter` before `out` |
| the microphones are silent in the editor | press **listen** (and choose the input in **Sound settings…**); without it `mic` is silence in the simulation |
| the editor says the house is full | the scene needs more than the Wingie's 70 KB: shorter natures, `samplerate=half`, or move a place to another scene (chapter 11) |
| a key also plays a note when I change slot | press the two Mode buttons first, then the key: while both are down, the dream sees no keys |
| it changed to another dream by itself | too heavy: it went back to the last dream that fit (purple flashing). Lighten the scene, or watch **dropped** in the meters |
| it changed slot by itself | the dream has a `slot` place and something pulsed it |
| send says the connected device is an SP-1 | the dream is for the Wingie and the editor found an SP-1, or the other way round: switch the dream (SP-1 / Wingie) or plug in the other instrument |
| send refused: too long | a slot keeps 16 KB of text; long comments and tuning lines count |
| the dream sounds different on the Wingie | the editor hears the computer's microphone, the Wingie its own two; the room is different too. The dream runs on the same engine |
| *SP-1's hands* mistakes | `button1`, `rocker`, `play`, `dots`, `leds` and `fader4` aren't on the Wingie. Use `mode1`, `mode2`, `keys1`, `keys2` and `fader1`–`fader3` |
| *the Wingie's hands* mistakes | `mic`, `keys1` and the rest need `instrument wingie` at the top |
| I want the stock Wingie back | Meng Qi's web flasher (chapter 1). Your dreams stay on the computer |

---

### Thanks

To Meng Qi for the Wingie2 and for opening its firmware (the pins, codec setup and I2S in
Sleepcousonique's Wingie firmware are his, GPLv3); to Peter Blasser for Coco, Quantussy and Rollz;
to Alvin Lucier, Pauline Oliveros and Iannis Xenakis, whose pieces live here as places; and to the
ground beneath your feet.
