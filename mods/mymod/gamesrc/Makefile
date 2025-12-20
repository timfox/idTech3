# Makefile for building native game modules
# This allows you to use modern C features instead of QVM's limited C compiler

# Detect architecture
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
    ARCH_SUFFIX := x86_64
else ifeq ($(UNAME_M),i386)
    ARCH_SUFFIX := i386
else ifeq ($(UNAME_M),i686)
    ARCH_SUFFIX := i386
else ifeq ($(UNAME_M),aarch64)
    ARCH_SUFFIX := arm64
else ifeq ($(UNAME_M),arm64)
    ARCH_SUFFIX := arm64
else ifeq ($(UNAME_M),armv7l)
    ARCH_SUFFIX := arm
else
    ARCH_SUFFIX := unknown
endif

# Compiler settings
CC := gcc
CFLAGS := -std=c23 -fPIC -fvisibility=hidden -Wall -Wextra -O2
LDFLAGS := -shared -lm

# Directories
SRCDIR := .
GAMEDIR := game
CGAMEDIR := cgame
UIDIR := ui
OUTDIR := ../vm

# Runtime install / cache locations
MOD_ROOT := $(abspath ..)
RUNTIME_VM := $(abspath ../../release/blacksun/vm)
HOME_VM := $(HOME)/.q3a/blacksun/vm
HOME_FONTS := $(HOME)/.q3a/blacksun/fonts
HOME_PK3CACHE := $(HOME)/.q3a/blacksun/pk3cache.dat
MOD_PK3CACHE := $(MOD_ROOT)/pk3cache.dat

# Engine include paths (adjust to match your setup)
ENGINE_ROOT := ../..
INCLUDES := -I$(ENGINE_ROOT)/src/game \
            -I$(ENGINE_ROOT)/src/cgame \
            -I$(ENGINE_ROOT)/src/ui \
            -I$(ENGINE_ROOT)/src/common \
            -I$(ENGINE_ROOT)/src/renderercommon

# Source files - OpenArena source (all .c files in each directory)
GAME_SRCS := $(wildcard $(GAMEDIR)/*.c)
CGAME_SRCS := $(filter-out $(CGAMEDIR)/cg_newdraw.c,$(wildcard $(CGAMEDIR)/*.c))
UI_SRCS := $(wildcard $(UIDIR)/*.c)

# Object files
GAME_OBJS := $(GAME_SRCS:.c=.o)
CGAME_OBJS := $(CGAME_SRCS:.c=.o)
UI_OBJS := $(UI_SRCS:.c=.o)

# Output files (must match engine naming: name.arch.so format)
GAME_LIB := $(OUTDIR)/game.$(ARCH_SUFFIX).so
CGAME_LIB := $(OUTDIR)/cgame.$(ARCH_SUFFIX).so
UI_LIB := $(OUTDIR)/ui.$(ARCH_SUFFIX).so

.PHONY: all clean game cgame ui

all: game cgame ui

game: $(GAME_LIB)

cgame: $(CGAME_LIB)

ui: $(UI_LIB)

$(GAME_LIB): $(GAME_OBJS)
	@mkdir -p $(OUTDIR)
	$(CC) $(LDFLAGS) -Wl,-soname,game.$(ARCH_SUFFIX).so -o $@ $^
	@echo "Built: $@"
	@$(MAKE) postinstall

$(CGAME_LIB): $(CGAME_OBJS)
	@mkdir -p $(OUTDIR)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Built: $@"
	@$(MAKE) postinstall

$(UI_LIB): $(UI_OBJS) $(GAME_LIB)
	@mkdir -p $(OUTDIR)
	$(CC) $(LDFLAGS) -Wl,-rpath,'$$ORIGIN' -o $@ $(UI_OBJS) -Wl,--as-needed $(GAME_LIB)
	@echo "Built: $@"
	@echo "Note: If ui.x86_64.so fails to load due to '../vm/game.x86_64.so' dependency,"
	@echo "      install patchelf and run: patchelf --replace-needed ../vm/game.$(ARCH_SUFFIX).so game.$(ARCH_SUFFIX).so $(UI_LIB)"
	@$(MAKE) postinstall

# After building, copy freshly built modules to runtime locations and clear stale caches
.PHONY: postinstall
postinstall:
	@rm -f $(HOME_VM)/cgame.* $(HOME_VM)/ui.* $(HOME_VM)/game.* 2>/dev/null || true
	@rm -f $(HOME_FONTS)/fontImage_* 2>/dev/null || true
	@rm -f $(HOME_PK3CACHE) $(MOD_PK3CACHE) 2>/dev/null || true
	@mkdir -p $(RUNTIME_VM)
	@mkdir -p $(HOME_VM)
	@cp -f $(OUTDIR)/game.$(ARCH_SUFFIX).so $(RUNTIME_VM)/ 2>/dev/null || true
	@cp -f $(OUTDIR)/cgame.$(ARCH_SUFFIX).so $(RUNTIME_VM)/ 2>/dev/null || true
	@cp -f $(OUTDIR)/ui.$(ARCH_SUFFIX).so $(RUNTIME_VM)/ 2>/dev/null || true
	@cp -f $(OUTDIR)/game.$(ARCH_SUFFIX).so $(HOME_VM)/ 2>/dev/null || true
	@cp -f $(OUTDIR)/cgame.$(ARCH_SUFFIX).so $(HOME_VM)/ 2>/dev/null || true
	@cp -f $(OUTDIR)/ui.$(ARCH_SUFFIX).so $(HOME_VM)/ 2>/dev/null || true
	@echo "Postinstall: copied VMs to runtime and cleared font/pk3 caches."

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(GAME_OBJS) $(CGAME_OBJS) $(UI_OBJS)
	rm -f $(GAME_LIB) $(CGAME_LIB) $(UI_LIB)

help:
	@echo "Native Game Module Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build all modules (game, cgame, ui)"
	@echo "  game    - Build server-side game module"
	@echo "  cgame   - Build client-side game module"
	@echo "  ui      - Build UI module"
	@echo "  clean   - Remove build artifacts"
	@echo ""
	@echo "Architecture detected: $(ARCH_SUFFIX)"
	@echo "Output directory: $(OUTDIR)"

