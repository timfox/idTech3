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
CFLAGS := -std=c11 -fPIC -fvisibility=hidden -Wall -Wextra -O2
LDFLAGS := -shared -lm

# Directories
SRCDIR := .
GAMEDIR := game
CGAMEDIR := cgame
UIDIR := ui
OUTDIR := ../vm

# Engine include paths (adjust to match your setup)
ENGINE_ROOT := ../..
INCLUDES := -I$(ENGINE_ROOT)/src/game \
            -I$(ENGINE_ROOT)/src/cgame \
            -I$(ENGINE_ROOT)/src/ui \
            -I$(ENGINE_ROOT)/src/qcommon \
            -I$(ENGINE_ROOT)/src/renderercommon

# Source files - OpenArena source (all .c files in each directory)
GAME_SRCS := $(wildcard $(GAMEDIR)/*.c)
CGAME_SRCS := $(wildcard $(CGAMEDIR)/*.c)
UI_SRCS := $(wildcard $(UIDIR)/*.c)

# Object files
GAME_OBJS := $(GAME_SRCS:.c=.o)
CGAME_OBJS := $(CGAME_SRCS:.c=.o)
UI_OBJS := $(UI_SRCS:.c=.o)

# Output files (must match engine naming: namearch.so for ui, name.arch.so for others)
GAME_LIB := $(OUTDIR)/game.$(ARCH_SUFFIX).so
CGAME_LIB := $(OUTDIR)/cgame.$(ARCH_SUFFIX).so
UI_LIB := $(OUTDIR)/ui$(ARCH_SUFFIX).so

.PHONY: all clean game cgame ui

all: game cgame ui

game: $(GAME_LIB)

cgame: $(CGAME_LIB)

ui: $(UI_LIB)

$(GAME_LIB): $(GAME_OBJS)
	@mkdir -p $(OUTDIR)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Built: $@"

$(CGAME_LIB): $(CGAME_OBJS)
	@mkdir -p $(OUTDIR)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Built: $@"

$(UI_LIB): $(UI_OBJS) $(GAME_LIB)
	@mkdir -p $(OUTDIR)
	$(CC) $(LDFLAGS) -L$(OUTDIR) -Wl,-rpath,'$$ORIGIN:$$ORIGIN/..' -o $@ $(UI_OBJS) $(GAME_LIB)
	@echo "Built: $@"

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

