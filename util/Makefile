#### PROJECT SETTINGS ####

ifeq ($(ROOT),)
	ROOT = $(shell pwd)/../../../
endif

# The version of the library
include $(ROOT)/Makefile.in

# The name of the executable to be created
LIB_NAME  := lib$(PROJECT_NAME)_util
SLIB_NAME := $(LIB_NAME).a
DLIB_NAME := $(LIB_NAME).so.$(PROJECT_VERSION)

# Compiler used
CXX ?= g++
# Archive used
AR ?= ar
# Extension of source files used in the project
SRC_EXT = cc
ASM_EXT = S
# Path to the source directory, relative to the makefile
SRC_PATH = ./

# Space-separated pkg-config libraries used by this project
LIBS := $(DEP_LIBS) -levent

# General compiler flags
COMPILE_FLAGS = -Wall -Werror -g -fPIC

# Additional release-specific flags
RCOMPILE_FLAGS = -DNDEBUG -O3
# Additional debug-specific flags
DCOMPILE_FLAGS =

# Add additional include paths
INCLUDES = $(DEP_CPPFLAGS) -I$(ROOT)/src

RTFLAGS := $(DEP_RPATHS)

# General linker settings
LINK_FLAGS = -pthread $(DEP_LDFLAGS)

# Additional release-specific linker settings
RLINK_FLAGS = 
# Additional debug-specific linker settings
DLINK_FLAGS = 

#### END PROJECT SETTINGS ####

# Generally should not need to edit below this line

# Shell used in this makefile
# bash is used for 'echo -en'
SHELL = /bin/bash
# Clear built-in rules
.SUFFIXES:
# Programs for installation
INSTALL = install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = $(INSTALL) -m 644

# Verbose option, to output compile and link commands
export V := false
export CMD_PREFIX := @
ifeq ($(V),true)
	CMD_PREFIX := 
endif

# Combine compiler and linker flags
release: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(RCOMPILE_FLAGS)
release: export LDFLAGS := $(LDFLAGS) $(LINK_FLAGS) $(RLINK_FLAGS) $(LIBS) $(RTFLAGS)
debug: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(DCOMPILE_FLAGS)
debug: export LDFLAGS := $(LDFLAGS) $(LINK_FLAGS) $(DLINK_FLAGS) $(LIBS) $(RTFLAGS)

# Build and output paths
release: export BUILD_PATH := build/release
release: export BIN_PATH := bin/release
debug: export BUILD_PATH := build/debug
debug: export BIN_PATH := bin/debug
install: export BIN_PATH := bin/release

# Find all source files in the source directory, sorted by most
# recently modified
CSOURCES = $(shell find $(SRC_PATH)/ -name '*.$(SRC_EXT)' -printf '%T@\t%p\n' \
					| sort -k 1nr | cut -f2-)

SSOURCES = $(shell find $(SRC_PATH)/ -name '*.$(ASM_EXT)' -printf '%T@\t%p\n' \
					| sort -k 1nr | cut -f2-)
# fallback in case the above fails
rwildcard = $(foreach d, $(wildcard $1*), $(call rwildcard,$d/,$2) \
						$(filter $(subst *,%,$2), $d))
ifeq ($(CSOURCES),)
	CSOURCES := $(call rwildcard, $(SRC_PATH)/, *.$(SRC_EXT))
endif

ifeq ($(SSOURCES),)
	SSOURCES := $(call rwildcard, $(SRC_PATH)/, *.$(ASM_EXT))
endif

# Set the object file names, with the source directory stripped
# from the path, and the build path prepended in its place
COBJ = $(CSOURCES:$(SRC_PATH)/%.$(SRC_EXT)=$(BUILD_PATH)/$(SRC_EXT)/%.o)
SOBJ = $(SSOURCES:$(SRC_PATH)/%.$(ASM_EXT)=$(BUILD_PATH)/$(ASM_EXT)/%.o)
OBJECTS = $(SOBJ) $(COBJ) 
# Set the dependency files that will be used to add header dependencies
DEPS = $(OBJECTS:.o=.d)

# Standard, non-optimized release build
.PHONY: release
release: dirs
	@echo "Building release: $(LIB_NAME)"
	@$(MAKE) all --no-print-directory

# Debug build for gdb debugging
.PHONY: debug
debug: dirs
	@echo "Building debug: $(LIB_NAME)"
	@$(MAKE) all --no-print-directory

# Create the directories used in the build
.PHONY: dirs
dirs:
	@mkdir -p $(dir $(OBJECTS))
	@mkdir -p $(BIN_PATH)

# Installs to the set path
.PHONY: install
install:
	@echo "Installing to $(LIB_PREFIX)"
	@$(RM) -fr $(LIB_PREFIX)/$(LIB_NAME).*
	@$(INSTALL_PROGRAM) $(BIN_PATH)/$(DLIB_NAME) $(LIB_PREFIX) \
		&& (cd $(LIB_PREFIX); ln -s $(DLIB_NAME) $(LIB_NAME).so)
	@echo "Installed success"

# Uninstalls the program
.PHONY: uninstall
uninstall:
	@echo "Removing $(LIB_PREFIX)/$(LIB_NAME)"
	@$(RM) $(LIB_PREFIX)/$(LIB_NAME).*

# Removes all build files
.PHONY: clean
clean:
	@echo "Deleting: $(LIB_NAME)"
	@$(RM) $(LIB_NAME).*
	@$(RM) -r build
	@$(RM) -r bin

# Main rule, checks the executable and symlinks to the output
all: $(BIN_PATH)/$(LIB_NAME)
	@$(RM) $(LIB_NAME).*
	@ln -s $(BIN_PATH)/$(SLIB_NAME) $(LIB_NAME).a
	@ln -s $(BIN_PATH)/$(DLIB_NAME) $(LIB_NAME).so
	@ln -s $(BIN_PATH)/$(DLIB_NAME) $(LIB_NAME).so.$(PROJECT_VERSION)

# Link the executable
$(BIN_PATH)/$(LIB_NAME): $(OBJECTS)
	@echo "Linking: $(LIB_NAME)"
	$(CMD_PREFIX)$(CC) -shared $(OBJECTS) $(LDFLAGS) -o $@.so.$(PROJECT_VERSION)
	$(CMD_PREFIX)$(AR) -cvq $@.a $(OBJECTS)

# Add dependency files, if they exist
-include $(DEPS)

# Source file rules
# After the first compilation they will be joined with the rules from the
# dependency files to provide header dependencies
$(BUILD_PATH)/$(SRC_EXT)/%.o: $(SRC_PATH)/%.$(SRC_EXT)
	@echo "Compiling: $< -> $@"
	$(CMD_PREFIX)$(CC) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

$(BUILD_PATH)/$(ASM_EXT)/%.o: $(SRC_PATH)/%.$(ASM_EXT)
	@echo "Compiling: $< -> $@"
	$(CMD_PREFIX)$(CC) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@
