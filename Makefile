# RE5CabbyCodes - a 32-bit proxy-DLL mod for Resident Evil 5 on Steam (app 21690, re5dx9.exe).
# Cross-compiled on Linux with mingw-w64 (no MSVC / no Windows needed).
#
# The mod ships as d3d9.dll. re5dx9.exe imports d3d9.dll, Windows looks for it
# in the game's folder first, and Proton runs DXVK's d3d9.dll as a native DLL
# (d3d9=n in its own overrides), so ours loads there too with no launch
# options. Every export but Direct3DCreate9 is a jump thunk into the real
# d3d9.dll (the system's - or d3d9_orig.dll, another d3d9.dll the player had in
# the folder, renamed); Direct3DCreate9 is the mod's own (src/overlay_dx9.cpp).
# The exe carries its steam_api.dll inside itself (Enigma), so there is no
# Steam DLL to stand in for.

CXX      := i686-w64-mingw32-g++

# The project name (used for the distributable), the DLL name (fixed by the
# game, which imports d3d9.dll) and the game executable are deliberately
# separate things.
PROJECT  := RE5CabbyCodes
NAME     := d3d9
ORIG     := d3d9_orig
EXE      := re5dx9.exe

# VERSION holds the single source of truth (the user's). With no VERSION file
# the build reports 0.0.0.
VERSION  := $(shell cat VERSION 2>/dev/null || echo 0.0.0)
BUILD    := build
TARGET   := $(BUILD)/$(NAME).dll
DEF      := d3d9.def

# Per-machine config (game install path), kept out of git. Copy
# config.mk.example to config.mk and set GAME_DIR there. The path contains
# spaces, so it is only ever used quoted inside shell recipes below.
-include config.mk

ifneq (,$(filter version rev,$(MAKECMDGOALS)))
REV := $(strip $(filter-out version rev,$(MAKECMDGOALS)))
$(eval $(REV):;@:)
endif

SRCS := $(wildcard src/*.cpp)

# Dear ImGui (vendored, MIT) supplies the overlay panel: the core plus the
# Win32 and DX9 backends - re5dx9.exe draws through Direct3D 9.
IMGUI_DIR  := contrib/imgui
IMGUI_SRCS := $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp \
              $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp \
              $(IMGUI_DIR)/backends/imgui_impl_dx9.cpp \
              $(IMGUI_DIR)/backends/imgui_impl_win32.cpp

OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS)) \
        $(patsubst $(IMGUI_DIR)/%.cpp,$(BUILD)/imgui/%.o,$(IMGUI_SRCS))

# -MMD -MP: emit .d header-dependency files so editing a header recompiles every
# .cpp that includes it.
CXXFLAGS := -m32 -std=c++20 -O2 -Wall -Wextra -Wno-cast-function-type \
            -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD \
            -I$(IMGUI_DIR) \
            -ffunction-sections -fdata-sections \
            -MMD -MP

# -static*: fold the C/C++ runtime in so the DLL has no external mingw runtime
# deps. Exports come from the .def, so no --exclude-all-symbols here.
LDFLAGS  := -m32 -shared \
            -static -static-libgcc -static-libstdc++ \
            -Wl,--gc-sections

# Only DLLs every Windows (and Wine) has are imported. dinput8 is the game's:
# the mod only ever calls the objects the game made with it. d3d9 itself is
# loaded at run time (the real one, by its full path).
LDLIBS   := -luser32 -lshell32 -lole32 -lgdi32 -ldwmapi -lwinmm

.PHONY: all clean install uninstall dist package rev version
all: $(TARGET)

$(TARGET): $(OBJS) $(DEF) | $(BUILD)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(DEF) $(LDLIBS)
	@echo ">> Built $@"

MOD_OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS))

# Rebuild the mod's own objects when the version changes (the vendored ImGui
# objects do not use it, so they are left alone). No VERSION file: nothing to track.
$(MOD_OBJS): $(wildcard VERSION)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -DRE5CC_VERSION='"$(VERSION)"' -c $< -o $@

$(BUILD)/imgui/%.o: $(IMGUI_DIR)/%.cpp | $(BUILD)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

$(BUILD):
	mkdir -p $(BUILD) $(BUILD)/imgui/backends

# `make version` prints the version; `make version X.Y.Z` sets it (`rev` is an
# older spelling of the same thing). Make has no argument syntax, so the new
# version arrives as a second goal; the block near the top defines a do-nothing
# target for it so make does not fail trying to build "1.2.3".
version rev:
	@v='$(REV)'; \
	if [ -z "$$v" ] && [ "$@" = version ]; then echo "$(VERSION)"; exit 0; fi; \
	if ! echo "$$v" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$$'; then \
	  echo "usage: make $@ X.Y.Z        (for example: make $@ 1.2.0)"; \
	  echo "current version: $(VERSION)"; \
	  exit 1; \
	fi; \
	printf '%s\n' "$$v" > VERSION; \
	echo ">> version $(VERSION) -> $$v"; \
	echo ">> run 'make package' to build $(PROJECT)_v$$v.zip"

clean:
	rm -rf $(BUILD) $(DIST)

# --- Deploy -----------------------------------------------------------------
# The game ships no d3d9.dll of its own (the system's is the real one). A
# d3d9.dll already in the folder that is not ours - another graphics mod - is
# moved aside to d3d9_orig.dll first, once, and the mod passes the game's calls
# on to it. Never the other way round: if d3d9_orig.dll is missing and the live
# DLL is ours, there is nothing to preserve.
#
# Atomic install: write to a temp name in the SAME directory then rename(2) over
# the target, so a running game keeps its old intact mapping.
install: $(TARGET)
	@set -e; \
	if [ -z "$(GAME_DIR)" ]; then \
	  echo "ERROR: GAME_DIR is not set."; \
	  echo "  Copy config.mk.example to config.mk and set GAME_DIR,"; \
	  echo "  or run: make install GAME_DIR=\"/path/to/Resident Evil 5\""; \
	  exit 1; \
	fi; \
	if [ ! -f "$(GAME_DIR)/$(EXE)" ]; then \
	  echo "ERROR: no '$(EXE)' in '$(GAME_DIR)' - is that really the game's folder?"; exit 1; \
	fi; \
	if pgrep -x $(EXE) >/dev/null 2>&1; then \
	  echo "ERROR: $(EXE) is running - close the game first."; exit 1; \
	fi; \
	live="$(GAME_DIR)/$(NAME).dll"; \
	other=$$(ls "$(GAME_DIR)" | grep -ix '$(NAME)\.dll' | head -1); \
	if [ -n "$$other" ] && ! grep -aq "$(PROJECT)" "$(GAME_DIR)/$$other" 2>/dev/null; then \
	  if [ -f "$(GAME_DIR)/$(ORIG).dll" ]; then \
	    echo "ERROR: $(GAME_DIR)/$$other is not the mod and $(ORIG).dll already exists - sort them out first."; \
	    exit 1; \
	  fi; \
	  mv -f "$(GAME_DIR)/$$other" "$(GAME_DIR)/$(ORIG).dll"; \
	  echo ">> another $$other was in the folder - kept as $(ORIG).dll (the mod passes the game's calls on to it)"; \
	fi; \
	t="$$live.re5cc-new.$$$$"; cp -f $(TARGET) "$$t"; mv -f "$$t" "$$live"; \
	echo ">> installed (atomic) $$live"; \
	for stray in $$(ls "$(GAME_DIR)" | grep -ix '$(NAME)\.dll' | grep -vx '$(NAME).dll'); do \
	  if grep -aq "$(PROJECT)" "$(GAME_DIR)/$$stray" 2>/dev/null; then \
	    rm -f "$(GAME_DIR)/$$stray"; \
	    echo ">> removed stale copy $$stray - it shadowed this install"; \
	  else \
	    echo ">> WARNING: $$stray exists and is not ours - it may shadow the mod"; \
	  fi; \
	done

# Remove the mod (and put a d3d9.dll it had moved aside back).
uninstall:
	@set -e; \
	if [ -z "$(GAME_DIR)" ]; then echo "ERROR: GAME_DIR is not set."; exit 1; fi; \
	for stray in $$(ls "$(GAME_DIR)" | grep -ix '$(NAME)\.dll'); do \
	  if grep -aq "$(PROJECT)" "$(GAME_DIR)/$$stray" 2>/dev/null; then \
	    rm -f "$(GAME_DIR)/$$stray"; \
	    echo ">> removed $$stray"; \
	  fi; \
	done; \
	if [ -f "$(GAME_DIR)/$(ORIG).dll" ]; then \
	  mv -f "$(GAME_DIR)/$(ORIG).dll" "$(GAME_DIR)/$(NAME).dll"; \
	  echo ">> put the other $(NAME).dll back"; \
	fi; \
	rm -f "$(GAME_DIR)/$(PROJECT).log"; \
	echo ">> the mod is removed"

# --- Distributable ----------------------------------------------------------
# `make dist` assembles dist/RE5CabbyCodes/ - the mod DLL plus the docs and an
# INSTALL.txt.
DIST    ?= dist
PAYLOAD  = $(DIST)/$(PROJECT)

dist: $(TARGET)
	@rm -rf "$(PAYLOAD)" && mkdir -p "$(PAYLOAD)"
	@cp $(TARGET) "$(PAYLOAD)/$(NAME).dll"
	@cp README.md LICENSE "$(PAYLOAD)/"
	@printf '%s\n' \
	  "$(PROJECT) $(VERSION) - pause-menu cheat panel for Resident Evil 5" \
	  "" \
	  "  For the current Steam version of Resident Evil 5 (build 11465250)." \
	  "" \
	  "INSTALL" \
	  "  Copy  $(NAME).dll  into your Resident Evil 5 folder (the one containing" \
	  "  '$(EXE)'). If a $(NAME).dll is already there (another mod), rename that one" \
	  "  to $(ORIG).dll first - the mod passes the game's graphics on to it." \
	  "" \
	  "  That is all, on Windows and on Linux / Steam Deck (Proton): no launch" \
	  "  options." \
	  "" \
	  "UNINSTALL" \
	  "  Delete $(NAME).dll (and rename $(ORIG).dll back, if you had one)." \
	  "" \
	  "USE" \
	  "  Pause the game while playing and a panel appears beside the pause menu." \
	  "  F7 hides/shows it. Settings and the log live in $(PROJECT).ini / .log" \
	  "  beside the DLL." \
	  > "$(PAYLOAD)/INSTALL.txt"
	@echo ">> assembled $(PAYLOAD)"

# <project>_v<version>.zip, e.g. RE5CabbyCodes_v1.0.0.zip
PKG ?= $(DIST)/$(PROJECT)_v$(VERSION).zip
package: dist
	@command -v zip >/dev/null || { echo "ERROR: zip required"; exit 1; }
	@rm -f "$(PKG)"
	@cd "$(PAYLOAD)" && zip -r -q "$(abspath $(PKG))" .
	@echo ">> packaged -> $(PKG) ($$(du -h "$(PKG)" | cut -f1))"
