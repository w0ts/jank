# jank shortcuts.
#   make simulator [ARGS="--xiao"]   the live player / emulator page (beat/live/player.py, 127.0.0.1:8791)
#   make flash [IMAGE=path.bin]       build, then install the new image on the running stick: the JANK drive if mounted, else MIDI
#                                     (ota.py). With IMAGE=path.bin: install that file, no build.
#   make page                         expand page/src/jank.html (+ ui/*) into page/jank.html, the file the firmware embeds
#   make build                        build the XIAO firmware (scripts/build.sh xiao)
# FW names the firmware checkout, BEAT the beat folder; both default to this checkout (main).

FW     ?= .
BEAT   ?= beat
PY     ?= $(BEAT)/.venv/bin/python
ARGS   ?= --xiao
# make flash builds first and installs that fresh image (scripts/build.sh writes build-xiao/jank.bin in the scratch folder);
# with IMAGE=path.bin it installs that file as it is, no build.
SCRATCH ?= $(or $(JANK_SCRATCH),$(FW)/worktrees/jank-scratch)
ifeq ($(origin IMAGE),undefined)
IMAGE      := $(SCRATCH)/build-xiao/jank.bin
FLASH_DEPS := build
endif

.PHONY: simulator flash build page

simulator:
	cd $(BEAT) && .venv/bin/python -m live.player $(ARGS)

flash: $(FLASH_DEPS)
	@[ -f "$(IMAGE)" ] || { echo "no image: $(IMAGE) (make flash IMAGE=path.bin)"; exit 2; }
	FW_DIR=$(abspath $(FW)) PY=$(abspath $(PY)) bash scripts/flash.sh $(abspath $(IMAGE))

build:
	cd $(FW) && bash scripts/build.sh xiao

page: page/jank.html

page/jank.html: page/src/jank.html $(wildcard ui/*) scripts/inline_ui.py
	python3 scripts/inline_ui.py page/src/jank.html -o page/jank.html
