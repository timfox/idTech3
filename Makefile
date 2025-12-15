#
# Top-level Makefile shim for CI workflows.
#
# This repository is CMake-based, but some GitHub Actions jobs invoke `make`
# directly (e.g. `make clean ARCH=x86`, `make install ...`).
#
# This file provides those targets and forwards them to CMake.
#

.PHONY: all install debug clean configure

# Common knobs used by CI (defaults match typical local dev)
ARCH ?= x86_64
CC ?= gcc
CXX ?=
INSTALL ?=
STRIP ?=

USE_SDL ?= 1
USE_RENDERER_DLOPEN ?= 1
RENDERER_DEFAULT ?= vulkan
CNAME ?= idtech3
BUILD_SERVER ?= 1

DESTDIR ?=

# Build type is driven by the make target in CI:
# - Release jobs call `make install ...`
# - Debug jobs call `make debug ...`
BUILD_TYPE ?= Release

# Parallelism (Linux: nproc, macOS: sysctl)
JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 1)

# If someone requests monolithic Vulkan (USE_RENDERER_DLOPEN=0) we currently
# force dlopen renderers for reproducible builds (matches CMake warning logic).
ifeq ($(RENDERER_DEFAULT),vulkan)
ifeq ($(USE_RENDERER_DLOPEN),0)
override USE_RENDERER_DLOPEN := 1
endif
endif

BUILD_DIR := build-$(ARCH)-$(RENDERER_DEFAULT)-$(BUILD_TYPE)

configure:
	@cmake -S . -B "$(BUILD_DIR)" -G "Unix Makefiles" \
		-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" \
		-DCNAME="$(CNAME)" \
		-DBUILD_SERVER="$(BUILD_SERVER)" \
		-DUSE_SDL="$(USE_SDL)" \
		-DUSE_RENDERER_DLOPEN="$(USE_RENDERER_DLOPEN)" \
		-DRENDERER_DEFAULT="$(RENDERER_DEFAULT)" \
		-DCMAKE_INSTALL_PREFIX="/usr/local" \
		$(if $(CC),-DCMAKE_C_COMPILER="$(CC)",) \
		$(if $(CXX),-DCMAKE_CXX_COMPILER="$(CXX)",)

all: BUILD_TYPE := Release
all: configure
	@cmake --build "$(BUILD_DIR)" -j$(JOBS)

install: BUILD_TYPE := Release
install: configure
	@cmake --build "$(BUILD_DIR)" -j$(JOBS)
	@DESTDIR="$(DESTDIR)" cmake --install "$(BUILD_DIR)"

debug: BUILD_TYPE := Debug
debug: configure
	@cmake --build "$(BUILD_DIR)" -j$(JOBS)
	@DESTDIR="$(DESTDIR)" cmake --install "$(BUILD_DIR)"

clean:
	@rm -rf "$(BUILD_DIR)"
