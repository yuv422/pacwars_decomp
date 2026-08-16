#!/bin/bash
cd "$(dirname "$0")"
SDL_VIDEODRIVER=dummy /c/DOSBox-X/dosbox-x -conf dosbox/dosbox-clean.conf
