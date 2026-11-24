#!/usr/bin/env python3
"""Narration for the video manual: Kokoro, British English, voice "Emma"
(bf_emma), the same narrator as the AERIA OS and Cirrostratus videos.

  ~/monsoon-fw/tools/video/.venv/bin/python tools/wingie-video/tts_kokoro.py jobs.json

jobs.json is a list of {"text": ..., "out": ".../x.wav"}. Existing outputs are
skipped, so re-runs only speak what changed. Output: 24 kHz mono WAV.

Setup (once, if the Cirrostratus venv is not there): python3.12 -m venv tools/wingie-video/.venv &&
              tools/wingie-video/.venv/bin/pip install kokoro soundfile
"""
import json, os, sys

import numpy as np
import soundfile as sf
from kokoro import KPipeline

VOICE = "bf_emma"


def main():
    jobs = [j for j in json.load(open(sys.argv[1])) if not os.path.exists(j["out"])]
    if not jobs:
        return
    pipe = KPipeline(lang_code="b", repo_id="hexgrad/Kokoro-82M")
    for i, j in enumerate(jobs):
        parts = [a.numpy() for _, _, a in pipe(j["text"], voice=VOICE, speed=1.0)]
        audio = np.concatenate(parts) if parts else np.zeros(2400, np.float32)
        tmp = j["out"] + ".tmp.wav"
        sf.write(tmp, audio, 24000)
        os.replace(tmp, j["out"])
        print("tts %d/%d" % (i + 1, len(jobs)), flush=True)


if __name__ == "__main__":
    main()
