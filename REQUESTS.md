# Requests

What was asked for, in the order it was asked (2026-11-13).

1. Something to code for the SP-1 stem player, now open and being hacked, drawing on the VSTs we've made. (Links given for code reference: Wakes, SP-1-dev, sp1-tape-looper, solderless, feldd.) Not the effects already out there.
2. "what else" / "more" (several rounds of ideas).
3. "do you know shbobo fish?"
4. "what if you made a program like fish that i could use to make custom patches and flash them onto the sp-1, like a modular environment, and i could map the faders and buttons to things?" (pblasser/shbobo)
5. Fish 2.0 for reference; ideas "kind of like nord modular too, where i can make the modular patch in a browser, map things and then flash it over usb."
6. "dont make it just like fish, make up your own language, have it be inspired by my dream journals ... and by nabra"
7. "have also be inspired by buchla and serge and nord modular"
8. Name: **Sleepcousonique**. "do it"
9. "make the editor a desktop app not a web app. allow it also to have a plaintext style of writing so i can just type it and paste it in the visual editor sometimes, if i dont want to use the visual editor"
10. "make a manual for it at the end"
11. "i dont need stems on the sp-1 i want it to be like fish and shbobo shnth where in the modular environment i can make mappings for what the knobs and buttons do, and have audio out ... patching modular in the program and then taking that patch with me on the device to perform standalone." (Stems removed everywhere; the hands panel now lists every control's mappings.)
12. "i dont want any audio files to be a part of it, i just want it to have the patch from the modular environment, no playing of audio tracks or prerecorded stuff ... only the patching and flashing to make it a standalone instrument." (The .wav export in scqtool removed; firmware confirmed to build without storage, filesystem, USB drive, USB audio or MIDI.)
13. "i need to be able to still put it into boot mode and flash new .bin to it if anything's wrong so dont get rid of anything that will prevent that" (Checked: the firmware never writes the bootloader or its settings page; no wipe option; recovery section added to the manual.)
14. "add way more sources shapes spaces and currents, pull from all times of modular synthesis environments, make it extremely in depth, the modules I could use." (62 new places, 102 in all, each with its lineage; examples/a-history.dream; tools/test_places.sh tests every place alone.)
15. "make sure theres a standard way to turn it on and off and check the battery life, and make sure it shows the battery life when charging still, make sure it can still send audio over bluetooth as well." (On/off and the battery tap kept from Wakes; now charges from a computer while on, and shows the charge bar on the play row while plugged in. Bluetooth audio out not possible yet: needs Infineon's non-public radio SDK; the BT chip is never touched.)
16. "make a button combo for changing the scene, i want to keep lift up and down as things i can patch in the modular world." (•• + FFWD / RWD changes scene on the SP-1 and in the editor; the rocker never reaches lift during the combo; examples no longer use lift for scenes.)
17. "can you make me a pdf guide on how you built it, how you decided the language it gets to be coded in, and how you wrote the code? i want to get into building apps like this, specifically the language part" (docs/guide/Building-Sleepcousonique.pdf + tinydream, a 260-line patch language in C with exercises.)
18. "can you check with my sp connected, i dont hear anything when i flashed it" / "make sure the send to sp also works" / "do your tests, update firmware and app". (Read the SP-1's console: the audio stream died after the first save and never recovered. Fixes: flash writes pause and restart the audio; an audio watchdog; a 4 KB flow-controlled USB receive buffer (1 KB dropped the end of patches); the engine's own load on the console; pitch and timing maths moved out of per-sample loops; the app now sends to the playing slot, waits out the SP-1's log replay, and has a --send test mode. tools/sp1bench measures dreams on a connected SP-1.)
