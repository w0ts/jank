# jank shortcuts.
#   make simulator [ARGS="--xiao"]   the live player / emulator page (beat/live/player.py, 127.0.0.1:8791)
#   make flash [IMAGE=path.bin]       install an image on the running stick: the JANK drive if mounted, else MIDI (ota.py)
#   make build                        build the XIAO firmware (scripts/build.sh xiao)
# FW names the firmware checkout, BEAT the beat folder; both default to this checkout (main).

FW     ?= .
BEAT   ?= beat
PY     ?= $(BEAT)/.venv/bin/python
ARGS   ?= --xiao
# default image: the newest build in the scratch folder (scripts/build.sh writes build-<variant>/jank.bin there)
IMAGE  ?= $(shell ls -t worktrees/jank-scratch/build-*/jank.bin 2>/dev/null | head -1)

.PHONY: simulator flash build

simulator:
	cd $(BEAT) && .venv/bin/python -m live.player $(ARGS)

flash:
	@[ -n "$(IMAGE)" ] || { echo "no image found: make flash IMAGE=path.bin"; exit 2; }
	FW_DIR=$(abspath $(FW)) PY=$(abspath $(PY)) bash scripts/flash.sh $(abspath $(IMAGE))

build:
	cd $(FW) && bash scripts/build.sh xiao
