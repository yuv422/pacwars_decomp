#!/bin/bash
cd "$(dirname "$0")"
#SDL_VIDEODRIVER=dummy 
dosbox-x -conf dosbox/dosbox-build.conf
cat GAME/BUILD.LOG
