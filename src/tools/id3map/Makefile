#  Makefile - Makefile for Q3MAP4
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301 USA.

# Crucial Environment Stuff
PROG_NAME := Q3MAP4
BUILD_DIR=$(PWD)
USER_HOME=$(HOME)
CC := gcc
PREFIX := /usr/bin
DATE_TIME := $(shell date "+%Y-%m-%d-%H-%M-%S")
ARCHIVE_FILE :=$(USER_HOME)/$(PROG_NAME)-$(DATE_TIME)
Q3MAP4_COMPILER_OPTIONS=-MMD -std=c23 -W -Wall -Wcast-align -Wcast-qual -Wno-unused-parameter -fno-strict-aliasing -O3 -march=native -fPIC -pipe -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_GNU_SOURCE -I/usr/include/libxml2 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/libpng16  -Iinclude -Ilibs
D_OPTIONS=-DPOSIX -DXWINDOWS  -DRADIANT_VERSION="\"1.5.0n\"" -DRADIANT_MAJOR_VERSION="\"5\"" -DRADIANT_MINOR_VERSION="\"0\"" -DRADIANT_ABOUTMSG="\"Custom_build\"" -DQ3MAP_VERSION="\"4.0.0n\"" -DRADIANT_EXECUTABLE="\"x86_64\""
LIBS_COMPILER_OPTIONS=-MMD -W -Wall -Wcast-align -Wcast-qual -Wno-unused-parameter -fno-strict-aliasing -O3 -march=native -fPIC -pipe -Ilibs

# Default behavior is to compile statically, but the optimization level can be selected.
# Compiling with -march=native typically improves performance, but at the cost of portability.
# With -march=native on, your executable will only work on CPUs with the same feature-sets.
# -O2 -g is the most normal option and will produce debugger output.
# Optimization levels without -g will not include debugger symbols, thus resulting in a
# smaller executable.

OPTIMIZERS := -static -O3 -march=native
#OPTIMIZERS := -O2 -g
#OPTIMIZERS := -O3
#OPTIMIZERS := -O4

EXTRA_C_FLAGS := -pipe

# Probably leave this one alone
MACHINE := $(shell cat /proc/cpuinfo | grep -w "model name" | sort | uniq | cut -f 2 -d ":")

# Directories to build things in

INCLUDE :=$(BUILD_DIR)/include
LIBS :=$(BUILD_DIR)/libs
MAIN :=$(BUILD_DIR)/main

INCLUDE_FILES :=$(wildcard $(INCLUDE)/*.c)
LIBS_FILES :=$(wildcard $(LIBS)/*.c)
MAIN_FILES :=$(wildcard $(MAIN)/*.c)

INCLUDE_OBJ_FILES := $(patsubst $(INCLUDE)/%.c,$(OBJ_DIR)/%.o,$(INCLUDE_FILES))
LIBS_OBJ_FILES := $(patsubst $(LIBS)/%.c,$(OBJ_DIR)/%.o,$(LIBS_FILES))
MAIN_OBJ_FILES := $(patsubst $(MAIN)/%.c,$(OBJ_DIR)/%.o,$(MAIN_FILES))

# ITEMS_TO_COMPILE := $(patsubst $(INCLUDE_FILES)
# ITEMS_TO_LINK := 

# Set these to add preprocessor or compiler flags, or use
# environment variables
# CFLAGS :=
# CPPFLAGS :=

# The subdirectories we need to build things in
# SRCDIRS := src

.PHONY: all clean install save_asm asm archive

release::
	@echo "*** Building $(PROG_NAME) via build-id3map.sh on:"
	@echo "*** $(MACHINE)"
	./build-id3map.sh
#	$(CC) $(Q3MAP4_COMPILER_OPTIONS) $(D_OPTIONS) $(MAIN_FILES)
#	$(CC) $(MAIN_OBJ_FILES) $(LIBS_OBJ_FILES) $(INCLUDE_OBJ_FILES) -o $(PROG_NAME)
#	strip $(PROG_NAME)
save_asm::
	@echo "*** Compiling $(PROG_NAME) on:"
	@echo "*** $(MACHINE)"
	@echo "*** keeping assembly files"
	$(CC) $(OPTIMIZERS) --save-temps $(ITEMS_TO_COMPILE)
asm::
	@echo "*** Compiling $(PROG_NAME) on:"
	@echo "*** $(MACHINE)"
	@echo "*** using assembly as source"
	$(CC) *.s -o $(PROG_NAME)_ASM
install::
	mv $(PROG_NAME) $PREFIX/
clean::
	rm $(PROG_NAME) $(ITEMS_TO_REMOVE)
archive::
	@echo "Exporting archive to your home area..."
	@echo "Name format is:"
	@echo "$(PROG_NAME)-YEAR-MONTH-DAY-HOUR-MINUTE-SECOND.tar"
ifneq ("$(wildcard $(ARCHIVE_FILE))","")
	tar -cjf $(ARCHIVE_FILE)-$(shell date "+%s").tar ../$(PROG_NAME)
else
	tar -cjf $(ARCHIVE_FILE).tar ../$(PROG_NAME)
endif
help:
	@echo 'Make targets are:'
	@echo ' release (default) - only generates the executable'
	@echo ' clean - removes intermediate files (.i, .s, .o) and the executable'
	@echo ' install - simply copies the executable to the path defined in PREFIX'
	@echo ' asm - compiles and saves .i .s .o files'
	@echo ' archive - tars up entire current build and source directories, saving'
	@echo '           the tar into your home area with a time-stamped name.'

