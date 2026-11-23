# jank shortcuts.
#   make simulator [ARGS="--xiao"]   the live player / emulator page (beat/live/player.py, 127.0.0.1:8791)
#   make flash [IMAGE=path.bin]       build, then install the new image on the running device: the JANK drive if mounted, else MIDI
#                                     (ota.py). With IMAGE=path.bin: install that file, no build.
#   make page                         expand page/src/*.html (+ ui/*) into page/: jank.html (the Device page, the file the firmware
#                                     embeds), index.html (About), install.html, build.html (Build: lists the photos still to come)
#   make publish-page                 publish the COMMITTED pages of main to https://w0ts.github.io/jank/ (runs the Pages workflow)
#   make build                        build the release firmware: XIAO or jank mini 3, told apart at the start (scripts/build.sh auto)
# FW names the firmware checkout, BEAT the beat folder; both default to this checkout (main).

FW     ?= .
BEAT   ?= beat
PY     ?= $(BEAT)/.venv/bin/python
ARGS   ?= --xiao
# make flash builds first and installs that fresh image (scripts/build.sh writes build-auto/jank.bin in the scratch folder);
# with IMAGE=path.bin it installs that file as it is, no build.
SCRATCH ?= $(or $(JANK_SCRATCH),$(FW)/worktrees/jank-scratch)
ifeq ($(origin IMAGE),undefined)
IMAGE      := $(SCRATCH)/build-auto/jank.bin
FLASH_DEPS := build
endif

.PHONY: simulator flash build page publish-page

simulator:
	cd $(BEAT) && .venv/bin/python -m live.player $(ARGS)

flash: $(FLASH_DEPS)
	@[ -f "$(IMAGE)" ] || { echo "no image: $(IMAGE) (make flash IMAGE=path.bin)"; exit 2; }
	FW_DIR=$(abspath $(FW)) PY=$(abspath $(PY)) bash scripts/flash.sh $(abspath $(IMAGE))

build:
	cd $(FW) && bash scripts/build.sh auto

page: page/jank.html page/index.html page/install.html page/build.html

page/jank.html: page/src/jank.html $(wildcard ui/*) scripts/inline_ui.py
	python3 scripts/inline_ui.py page/src/jank.html -o page/jank.html

page/index.html: page/src/index.html ui/common.css scripts/inline_ui.py
	python3 scripts/inline_ui.py page/src/index.html -o page/index.html

page/install.html: page/src/install.html ui/common.css scripts/inline_ui.py
	python3 scripts/inline_ui.py page/src/install.html -o page/install.html

page/build.html: page/src/build.html $(wildcard page/src/buildparts/*) ui/common.css scripts/inline_ui.py scripts/photo_todo.py
	python3 scripts/inline_ui.py page/src/build.html -o page/build.html
	python3 scripts/photo_todo.py page/build.html

# the committed pages to https://w0ts.github.io/jank/: the Pages workflow (scripts/site_tree.py) on main; it also runs by
# itself when page/ changes on main and after a release
publish-page:
	gh workflow run pages.yml --ref main
