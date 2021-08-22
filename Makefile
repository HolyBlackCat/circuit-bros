# This file is an universal makefile for simple C/C++ executable projects.
#
#
# ---- COMPILING
#
# -- CONFIGURATION
#
# This makefile doesn't contain any project-specific details.
# Project configuration (i.e. sources and compiler settings) is stored in `project_config.mk`.
#
# Machine-specific configuration (i.e. what compiler to use) is stored in `local_config.mk`.
# If `local_config.mk` is missing, you will be asked to create it.
# Make sure it contains valid compiler paths before building.
#
# -- WINDOWS SUPPORT
#
# Windows builds are supported, but not with the MSVC compiler.
# You need to use GCC or Clang. (Which you can get from MSYS2.)
# On windows you also need the MSYS2 shell (or something similar).
# The native Windows shell (with `mingw32-make` instead of `make`) will not work.
#
# -- USAGE TL;DR
#
# Inspect `local_config.mk` and adjust any compiler and tool paths if necessary.
#
# Then run `make mode=release -j4`. If this project doesn't support 'release'
# build mode, you'll be given a list of the available modes to choose from.
#
# Some projects might expect you to provide a set of prebuilt dependencies, consult the project documentation.
#
# -- USAGE
#
# This makefile uses a concept of 'build modes'.
# A build mode is essentially a named set of compiler settings, such as 'debug', 'release', and so on.
# See `project_config.mk` for the list of available modes for this project (look for `$(call new_mode, ...)` mode declarations).
#
# Object files and precompiled headers for each mode are stored separately, which allows switching
# between modes quickly. But this doesn't apply to the resulting executable, so it will always be
# relinked when you build after changing the mode.
#
# A list of public `make` targets is provided below.
#
# Targets marked with `[M]` are affected by the selected build mode. Those targets can be
# invoked with `mode=<mode_name>` to selected a specific mode.
# The last used mode is remembered (saved to `.current_mode.mk`) and is used by default until changed later.
# If you haven't selected a mode yet, failure to specify `mode=...` is a error.
# Targets NOT marked with `[M]` are not affected by current build mode and don't accept the `mode=...` flag.
#
# [M] * `make`                - Same as `make build`.
# [M] * `make build`          - Builds the project using current build mode.
# [M] * `make use`            - Switches to a different build mode without doing anything else. `mode=...` has to be specified.
# [M] * `make clean_mode`     - Deletes object files and precompiled headers for the current build mode. The executable is not deleted.
# [M] * `make clean_pch`      - Deletes precompiled headers for the current build mode. The executable is not deleted.
#     * `make clean`          - Deletes all object files, precompiled headers, and the executable.
#     * `make commands`       - Generate `compile_commands.json`. Mode-specific flags are ignored.
#                               You can add `EXCLUDE_FILES=...` and `EXCLUDE_DIRS=...` to exclude some of the sources.
#                               If `LINKER_MODE` is set to `CXX` (normally in `project_config.mk`), then `.h` files will be treated as C++ sources.
#     * `make clean_commands` - Delete `compile_commands.json`.
#     * `make clean_everything` - Does `make clean clean_deps clean_commands`, effectively cleaning the repository.
#                               Also removes the object directory, which at this point
#                               should contain only the file that stores the name of the last used mode.
#
# Next, there is a 'dependency management' feature that projects can enable.
# If a project uses dependency management, it relies on a pack of prebuilt libraries that have to be downloaded separately.
# When you build the project for the first time, at the very beginning of the build sequence, the expected location of those dependencies
# will be checked, and some information about them will be collected.
# Then, at the end of the build sequence, shared libraries used by the application (the standard ones, and the ones coming from the library
# pack, as determined by LDD) will be copied to the build directory.
#
# If you enable dependency management, several additional targets become availabe:
#
#     * `make find_deps`      - Checks if the prebuilt dependencies are in the correct location. Runs `pkg-config` on them and saves the results.
#                               This runs automatically at the beginning of the build sequence, but you can use this target to check dependencies
#                               without building anything.
#     * `make clean_shared_libs` - Erases shared libraries that were previously copied from a pack of prebuilt dependencies into the build
#                               directory, and the ones that constitute the standard library and were copied from system directories.
#                               If they're missing, they will be copied back in place at the end of the build sequence.
#     * `make clean_deps`     - Same as `clean_shared_libs`, but also erases the files generated by `find_deps`.
#
#
# End of documentation.


# --- DEFAULT TARGET ---
build:


# --- CHECK IF MAKE WAS RESTARTED ---
ifneq ($(MAKE_RESTARTS),)
override make_was_restarted := yes
$(info --------)
else
override make_was_restarted :=
endif


# --- DEFINITIONS ---

# Some constants.
override space := $(strip) $(strip)
override comma := ,
override dollar := $
override open_par := (
override close_par := )
override define lf :=
$(strip)
$(strip)
endef

# If a target depends on `__force_this_target`, it will be always considered outdated.
.PHONY: __force_this_target
__force_this_target:

# Used to create local variables in a safer way. E.g. `$(call var,x := 42)`.
override var = $(eval override $(subst $,$$$$,$1))

# A recursive wildcard function.
# Source: https://stackoverflow.com/a/18258352/2752075
# Recursively searches a directory for all files matching a pattern.
# The first parameter is a directory, the second is a pattern.
# Example usage: SOURCES = $(call rwildcard, src, *.c *.cpp)
# This implementation differs from the original. It was changed to correctly handle directory names without trailing `/`, and several directories at once.
override rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# Same as `$(shell ...)`, but triggers a error on failure.
ifeq ($(filter --trace,$(MAKEFLAGS)),)
override safe_shell = $(shell $1)$(if $(filter-out 0,$(.SHELLSTATUS)),$(error Unable to execute `$1`, status $(.SHELLSTATUS)))
else
override safe_shell = $(info Shell command: $1)$(shell $1)$(if $(filter-out 0,$(.SHELLSTATUS)),$(error Unable to execute `$1`, status $(.SHELLSTATUS)))
endif

# Same as `safe_shell`, but discards the output and expands to a single space.
override safe_shell_exec = $(call space,$(call safe_shell,$1))

# Trim space on the left of $1.
override trim_left = $(call var,__trim_left_var := $1)$(__trim_left_var)$(eval override undefine __trim_left_var)

# Checks if $2 contains any word from list $1 as a substring.
override find_any_as_substr = $(word 1,$(foreach x,$1,$(findstring $x,$2)))

# Compairs two strings for equality
override strings_equal = $(and $(if $(findstring $1,$2),y),$(if $(findstring $2,$1),y))

# Checks if list $2 contains element $1
override find_elem_in = $(if $(filter 1,$(words $1)),$(word 1,$(foreach x,$2,$(if $(call strings_equal,$1,$x),y))))


# --- DETECT ENVIRONMENT ---

# Host OS and target OS.Z
ifeq ($(OS),Windows_NT)
HOST_OS ?= windows
TARGET_OS ?= windows
else
HOST_OS ?= linux
TARGET_OS ?= linux
endif

ifeq ($(HOST_OS),windows)
override host_extension_exe := .exe
override host_native_path = $(subst /,\,$1)
else
override host_extension_exe :=
override host_native_path = $1
endif

ifeq ($(TARGET_OS),windows)
override extension_exe := .exe
override pattern_dll := *.dll
override directory_dll := bin
override is_canonical_dll_name = y
else
override extension_exe :=
override pattern_dll := *.so*
override directory_dll := lib
# This should match `foo.so.1`, but not `foo.so` or `foo.so.1.2`
override is_canonical_dll_name = $(filter 2,$(words $(subst ., ,$(word 2,$(subst .so, so,$(subst $(space),<,$1))))))
endif

# Make sure we're not using a Windows shell.
ifeq ($(shell echo "foo"),"foo")
$(error `mingw32-make` is not supported! Use MSYS2 and its `make` instead.)
endif

# Shell functions.
override quote = '$(subst ','"'"',$1)'
override echo = echo $(call quote,$1)
override pause := read -s -n 1 -p 'Press any key to continue . . .' && echo


# --- IMPORT LOCAL CONFIG ---

# You can override default values specified below in `local_config.mk`.

POST_BUILD_COMMANDS :=

# Note that error messages below are triggered lazily, only when the corresponding variables are used.
C_COMPILER = $(error No C compiler specified.\
	$(lf)Define `C_COMPILER` in `local_config.mk` or directly when invoking `make`)

CXX_COMPILER = $(error No C++ compiler specified.\
	$(lf)Define `CXX_COMPILER` in `local_config.mk` or directly when invoking `make`)

C_LINKER = $(error No C linker specified.\
	$(lf)Define `C_LINKER` in `local_config.mk` or directly when invoking `make`.\
	$(lf)Normally it should be equal to `C_COMPILER`.\
	$(lf)\
	$(lf)If you're using Clang, consider using LLD linker to improve linking times. See comments in the makefile for details)

CXX_LINKER = $(error No C++ linker specified.\
	$(lf)Define `CXX_LINKER` in `local_config.mk` or directly when invoking `make`.\
	$(lf)Normally it should be equal to `CXX_COMPILER`.\
	$(lf)\
	$(lf)If you're using Clang, consider using LLD linker to improve linking times. See comments in the makefile for details)

# Using LLD linker with Clang
#
# To use LLD linker, append `-fuse-ld=lld` to `*_LINKER` variables.
# On windows, some old LLD versions used to generate `<exec_name>.lib` file alongside the resulting binary.
# If that happens to you, add following to `local_config.mk` to automatically delete that file:
#
#     ifeq ($(TARGET_OS),windows)
#     POST_BUILD_COMMANDS = @rm -f '$(OUTPUT_FILE).lib' || true
#     endif
#

PKGCONFIG := pkg-config

ifeq ($(TARGET_OS),windows)
WINDRES := windres
LDD := ntldd -R
PATCHELF = $(error Attempt to use `patchelf`, but we're not on Linux)
else
WINDRES = $(error Attempt to use `windres`, but we're not on Windows)
LDD := ldd
PATCHELF := patchelf
endif

# Following variables have no effect if dependency management is disabled:
# [
# Will look for dependencies in this directory.
LIBRARY_PACK_DIR := ../_dependencies
ifeq ($(TARGET_OS),windows)
# If one of those strings is present in a shared library name, that library is considered to be a part of the standard library.
STD_LIB_NAME_PATTERNS := libgcc libstdc++ libc++ winpthread
# If one of those strings is present in a path to a library that matches `STD_LIB_NAME_PATTERNS`, a error will be emitted.
BANNED_LIBRARY_PATH_PATTERNS := :/Windows
else
# Some testing was performed (building on Ubuntu 18.04, running on 20.04), and it was determined that copying any other libraries crashes the application.
# Except `libatomic`, but I don't think we care about it.
STD_LIB_NAME_PATTERNS := libgcc libstdc++
BANNED_LIBRARY_PATH_PATTERNS :=
endif
# ]


# Include the config
-include local_config.mk


# --- CONFIG FUNCTIONS ---
override mode_list :=
override last_mode :=

# Function: make new mode.
# Example: `$(call new_mode,debug)`.
override define new_mode =
$(eval
override mode_list += $1
override last_mode := $1
.PHONY: __mode_$(strip $1)
__mode_$(strip $1): __generic_build
)
endef

# Function: set mode-specific flags. It only affects the last defined mode.
# Example: `$(mode_flags) CXXFLAGS += -g`.
override mode_flags = __mode_$(last_mode): override$(space)

# Internal function: check if mode exists.
# Note that we shouldn't use `findstring` here, since it can match a part of mode name.
override mode_exists = $(call find_elem_in,$1,$(mode_list))


# --- DEFAULT VARIABLE VALUES ---
SOURCES :=
SOURCE_DIRS := .
OUTPUT_FILE := program
OUTPUT_FILE_EXT = $(OUTPUT_FILE)$(extension_exe)# Output filename with extension. Normally you don't need to touch this, override `OUTPUT_FILE` instead. Note that we can't use `:=` here.
OBJECT_DIR := obj
CFLAGS := -std=c17 -Wall -Wextra -pedantic-errors -g
CXXFLAGS := -std=c++20 -Wall -Wextra -pedantic-errors -g
WINDRES_FLAGS := -O res
LDFLAGS :=
LINKER_MODE := CXX# C or CXX
ALLOW_PCH := 1# 0 or 1

PRECOMPILED_HEADERS :=
# Controls precompiled headers. Must be a `|`-separated list.
# Each entry is written as `patterns>header`, where `header` is a header file name (probably ending in `.h` or `.hpp`)
# and `patterns` is a space-separated list of file names or name patterns (using `*` as a wildcard character).
# Files that match a pattern will use this precompiled header.
# Example: `PRECOMPILED_HEADERS = src/game/*.cpp src/states/*.cpp > src/pch.hpp | lib/*.c > lib/common.h`.
# If `ALLOW_PCH` == 0, then headers are not precompiled, but are still included using compiler flags.

FILE_SPECIFIC_FLAGS :=
# Applies additional flags to specific files. Must be a `|`-separated list.
# Each entry is written as `patterns>flags`, where `patterns` is a space-separated list of file names or
# name patterns (using `*` as a wildcard character) and `flags` is a space-separated list of flags.
# Example: `FILE_SPECIFIC_FLAGS = lib/*.cpp > -03 -ffast-math | src/game/main.cpp > -O0`.

LIBRARY_PACK_NAME :=
# If non-empty, enables automatic dependency (library) management.
# It should be set to a name of a library pack.

# Following variables have no effect if dependency management is disabled:
# [
USED_PACKAGES :=
# Used pkg-config packages from the library pack.
USED_EXTERNAL_PACKAGES :=
# Used external pkg-config packages.
ALWAYS_COPIED_SHARED_LIB_PATTERS :=
# A list of patterns. If a shared library name matches this pattern (contains any word from this list as a substring),
# it will be copied from the dependency pack to the location of the binary unconditionally.
# Otherwise a library is only copied if the linker flags obtained from pkg-config (`-l...`) mention a similar name.
# ]


# --- ADDITIONAL MAKEFILE LOCATIONS ---
override current_mode_file := $(OBJECT_DIR)/current_mode.mk
override lib_pack_info_file := $(OBJECT_DIR)/library_pack_info.mk


# --- INCLUDE LIBRARY PACK INFO FILE ---
override deps_library_pack_name :=
override deps_packages :=
override deps_external_packages :=
override deps_always_copied_shared_lib_patterns :=
override deps_compiler_flags :=
override deps_linker_flags :=
-include $(lib_pack_info_file)


# --- INCLUDE PROJECT CONFIG ---
-include project_config.mk
override dependency_management_enabled := $(if $(strip $(LIBRARY_PACK_NAME)),yes)


# --- SELECT BUILD MODE ---

# Prevent env variables from overriding `target`.
mode :=
override mode := $(strip $(mode))
override mode_list := $(strip $(mode_list))

# Try loading saved mode name
override current_mode :=
-include $(current_mode_file)
override current_mode := $(strip $(current_mode))
$(if $(current_mode),$(if $(call mode_exists,$(current_mode)),,$(error Invalid mode name specified in `$(current_mode_file)`. Delete that file (or do `make clean`) and try again)))

# Internal: Make sure a valid mode is selected. Save current mode to the config file if necessary.
# Note that mingw32-make doesn't seem to have `else if[n]eq` conditionals, so we have to use the `__else` hack.
.PHONY: __check_mode
# If no mode is selected.
ifeq ($(or $(mode),$(current_mode)),)
__check_mode:
	$(error No build mode selected.\
		$(lf)Add `mode=...` to the flags. Selected mode will be remembered and used by default until changed.\
		$(lf)You can also do `make use mode=...` to change mode without doing anything else.\
		$(lf)Supported modes are: $(mode_list))
else ifeq ($(mode),)
# If using a saved mode.
__check_mode:
	$(info [Mode] $(current_mode))
else
# If mode was specified with a flag.
__check_mode:
	$(if $(call mode_exists,$(mode)),,$(error Invalid build mode specified.\
		$(lf)Expected one of: $(mode_list)))
	$(info [Mode] $(strip $(old_mode) -> $(mode)))
ifneq ($(mode),$(current_mode)) # Specified mode differs from the saved one, file update is necessary.
	$(file >$(current_mode_file),override current_mode := $(mode))
	@true
endif

ifneq ($(mode),$(current_mode)) # Specified mode differs from the saved one.
override mode_changed := 1
endif

override old_mode := $(current_mode)
override current_mode := $(mode)
endif

# Internal: Make sure no mode is specified in the flags. (Use this for targets that don't need to know build mode.)
.PHONY:
__no_mode_needed:
	$(if $(mode),$(error This operation doesn't require a build mode))


# --- COMBINE FLAGS ---
override common_object_dir := $(OBJECT_DIR)
override OBJECT_DIR := $(OBJECT_DIR)/$(current_mode)

CFLAGS_EXTRA :=
CXXFLAGS_EXTRA :=
LDFLAGS_EXTRA :=
FILE_SPECIFIC_FLAGS_EXTRA :=
override CFLAGS += $(CFLAGS_EXTRA)
override CXXFLAGS += $(CXXFLAGS_EXTRA)
override LDFLAGS += $(LDFLAGS_EXTRA)
override FILE_SPECIFIC_FLAGS += $(FILE_SPECIFIC_FLAGS_EXTRA)


# --- LOCATE FILES ---

# Source files.
override source_file_extensions := *.c *.cpp
ifeq ($(TARGET_OS),windows)
override source_file_extensions += *.rc
endif

override SOURCES += $(call rwildcard,$(SOURCE_DIRS),$(source_file_extensions))# Note the `+=`.

# Object files.
override objects := $(SOURCES:%=$(OBJECT_DIR)/%.o)

# Dependency lists
override dep_files := $(patsubst %.o,%.d,$(filter %.c.o %.cpp.o,$(objects)))


# --- HANDLE PRECOMPILED HEADERS ---
# A raw list of all precompiled header entries. You can iterate over it, then use following functions to extract data from each element.
override pch_all_entries = $(subst |, ,$(subst $(space),<,$(PRECOMPILED_HEADERS)))
# Extract all file patterns from a precompiled header entry.
override pch_entry_file_patterns = $(subst *,%,$(subst <, ,$(word 1,$(subst >, ,$1))))
# Extract header name from a precompiled header entry.
override pch_entry_header = $(strip $(subst <, ,$(word 2,$(subst >, ,$1))))
# List of all precompiled header files.
override compiled_headers := $(foreach x,$(pch_all_entries),$(OBJECT_DIR)/$(call pch_entry_header,$x).gch)
# Add precompiled headers as dependencies for corresponding source files. The rest is handled automatically.
$(foreach x,$(pch_all_entries),$(foreach y,$(filter $(call pch_entry_file_patterns,$x),$(SOURCES)),$(eval $(OBJECT_DIR)/$y.o: $(OBJECT_DIR)/$(call pch_entry_header,$x).gch)))
ifeq ($(strip $(ALLOW_PCH)),1)
# Register dependency files for precompiled headers.
override dep_files += $(foreach x,$(pch_all_entries),$(OBJECT_DIR)/$(call pch_entry_header,$x).d)
# This function processes compiler flags for files. If this file uses a PCH, all `-include` flags are removed and the PCH is included instead.
override add_pch_to_flags = $(if $2,$(filter-out -include|%,$(subst -include ,-include|,$(strip $1))) -include $(patsubst %.gch,%,$2),$1)
else
# This function processes compiler flags for files when PCHs are disabled. If this file uses a PCH, non-precompiled version of that header is included.
override add_pch_to_flags = $1 $(if $2,-include $(patsubst $(OBJECT_DIR)/%.gch,%,$2))
endif


# --- HANDLE FILE-SPECIFIC FLAGS ---
override file_local_flags :=
$(foreach x,$(subst |, ,$(subst $(space),<,$(FILE_SPECIFIC_FLAGS))),$(foreach y,$(filter $(subst *,%,$(subst <, ,$(word 1,$(subst >, ,$x)))),$(SOURCES)),\
																	  $(eval $(OBJECT_DIR)/$y.o: override file_local_flags += $(strip $(subst <, ,$(word 2,$(subst >, ,$x)))))))


# --- RULES FOR CREATING FOLDERS ---
override files_requiring_folders := $(OUTPUT_FILE_EXT) $(objects) $(compiled_headers) $(current_mode_file) $(lib_pack_info_file)
$(foreach x,$(files_requiring_folders),$(eval $x: | $(dir $x)))
$(sort $(dir $(files_requiring_folders))):
	@mkdir -p $(call quote,$@)


# --- TARGETS ---

# Public: set mode without doing anything else.
.PHONY: use
use: __check_mode
	$(if $(mode),,$(error No build mode specified.\
		$(lf)Add `mode=...` to the flags to change mode))

# Public: build stuff.
.PHONY: build
build: __check_mode __mode_$(current_mode)

# Public: erase files for all modes and the executable.
.PHONY: clean
clean: __no_mode_needed
	@$(call echo,[Cleaning] All build artifacts)
	@true $(foreach x,$(mode_list),; rm -rf $(call quote,$(common_object_dir)/$x))
	@rm -f $(call quote,$(OUTPUT_FILE_EXT))
	@$(call echo,[Done])

# Public: erase files for the current build mode, including precompiled headers but not including the executable.
.PHONY: clean_mode
clean_mode: __check_mode
	@$(call echo,[Cleaning] Mode '$(current_mode)')
	@rm -rf $(call quote,$(OBJECT_DIR))
	@$(call echo,[Done])

# Public: clean precompiled headers for the current build mode.
.PHONY: clean_pch
clean_pch: __check_mode
	@$(call echo,[Cleaning] PCH for mode '$(current_mode)') $(foreach x,$(compiled_headers),; rm -f $(call quote,$x))
	@$(call echo,[Done])

# Public: clean absolutely everything.
.PHONY: clean_everything
clean_everything: clean_commands
	@$(call echo,[Cleaning] All build artifacts)
	@rm -rf $(call quote,$(common_object_dir))
	@rm -f $(call quote,$(OUTPUT_FILE_EXT))
	@$(call echo,[Done])

ifneq ($(dependency_management_enabled),)
clean_everything: clean_deps
endif


# Internal: Generic build. This is used by `__mode_*` targets.
.PHONY: __generic_build
__generic_build: $(OUTPUT_FILE_EXT)

# Internal: Actually build the executable.
# Note that object files come before linker flags.
$(OUTPUT_FILE_EXT): $(objects)
	@$(call echo,[Linking] $(OUTPUT_FILE_EXT))
	@$($(LINKER_MODE)_LINKER) $(objects) $(LDFLAGS) -o $@
	@$(if $(POST_BUILD_COMMANDS),$(call echo,[Finishing]))
	$(POST_BUILD_COMMANDS)
	@$(call echo,[Done])

# Make sure the executable is rebuilt correctly if build mode changes.
ifneq ($(strip $(mode_changed)),)
$(OUTPUT_FILE_EXT): __force_this_target
else
$(OUTPUT_FILE_EXT): $(current_mode_file)
endif

# Internal targets that build the source files.
# Note that flags come before the files. Note that file-specific flags aren't passed to `add_pch_to_flags`, to prevent removal of `-include` from them.
# * C sources
$(OBJECT_DIR)/%.c.o: %.c
	@$(call echo,[C] $<)
	@$(strip $(C_COMPILER) -MMD -MP $(call add_pch_to_flags,$(CFLAGS),$(filter %.gch,$^)) $(file_local_flags) $< -c -o $@)
# * C++ sources
$(OBJECT_DIR)/%.cpp.o: %.cpp
	@$(call echo,[C++] $<)
	@$(strip $(CXX_COMPILER) -MMD -MP $(call add_pch_to_flags,$(CXXFLAGS),$(filter %.gch,$^)) $(file_local_flags) $< -c -o $@)
ifeq ($(strip $(ALLOW_PCH)),1)
# * C precompiled headers
$(OBJECT_DIR)/%.h.gch: %.h
	@$(call echo,[C header] $<)
	@$(strip $(C_COMPILER) -MMD -MP $(CFLAGS) $(file_local_flags) $< -c -o $@)
# * C++ precompiled headers
$(OBJECT_DIR)/%.hpp.gch: %.hpp
	@$(call echo,[C++ header] $<)
	@$(strip $(CXX_COMPILER) -MMD -MP $(CXXFLAGS) $(file_local_flags) $< -c -o $@)
else
# * C precompiled headers (skip)
$(OBJECT_DIR)/%.h.gch: %.h
	@touch $(call quote,$@)
# * C++ precompiled headers (skip)
$(OBJECT_DIR)/%.hpp.gch: %.hpp
	@touch $(call quote,$@)
endif
# * Windows resources
$(OBJECT_DIR)/%.rc.o: %.rc
	@$(call echo,[Resource] $<)
	@$(WINDRES) $(WINDRES_FLAGS) -i $< -o $@

# Helpers for generating compile_commands.json
# Note that we avoid using `:=` on some variables here because they aren't used often.
EXCLUDE_FILES :=# Override those when invoking `make commands`.
EXCLUDE_DIRS  :=# ^

override assume_h_files_are_cpp := $(filter CXX,$(LINKER_MODE))

ifneq ($(filter windows,$(HOST_OS)),)
# If we're on windows and use linux-style shell, we need to fix the path. `/c/foo/bar` -> `c:/foo/bar`
override current_dir = $(subst <, ,$(subst $(space),/,$(strip $(join $(subst /, ,$(subst $(space),<,$(CURDIR))),:))))
else
override current_dir = $(CURDIR)
endif
override EXCLUDE_FILES += $(foreach d,$(EXCLUDE_DIRS),$(call rwildcard,$d,*.c *.cpp *.h *.hpp))
override include_files = $(filter-out $(EXCLUDE_FILES),$(SOURCES) $(foreach d,$(SOURCE_DIRS),$(call rwildcard,$d,*.h *.hpp)))
override get_file_headers = $(foreach x,$(pch_all_entries),$(if $(filter $(call pch_entry_file_patterns,$x),$1),-include $(call pch_entry_header,$x)))
override get_file_local_flags = $(foreach x,$(subst |, ,$(subst $(space),<,$(FILE_SPECIFIC_FLAGS))),$(if $(filter $(subst *,%,$(subst <, ,$(word 1,$(subst >, ,$x)))),$1),$(subst <, ,$(word 2,$(subst >, ,$x)))))
override file_command = $(file >>compile_commands.json,{"directory": "$(current_dir)", "file": "$(current_dir)/$3", "command": "$(strip $1 $2 $(call get_file_headers,$3) $3)"},)
override all_commands = $(foreach f,$(filter %.c $(if $(assume_h_files_are_cpp),,%.h),$(include_files)),$(call file_command,$(C_COMPILER),$(CFLAGS) $(call get_file_local_flags,$f),$f)) \
						$(foreach f,$(filter %.cpp %.hpp $(if $(assume_h_files_are_cpp),%.h),$(include_files)),$(call file_command,$(CXX_COMPILER),$(CXXFLAGS) $(call get_file_local_flags,$f),$f))

# Public: generate `compile_commands.json`.
# Any target-specific flags are ignored.
.PHONY: commands
commands: __no_mode_needed
	@$(call echo,[Generating] compile_commands.json)
	$(file >compile_commands.json,[)
	$(all_commands)
	$(file >>compile_commands.json,])
	@$(call echo,[Done])

# Public: remove compile_commands.json.
.PHONY: clean_commands
clean_commands: __no_mode_needed
	@$(call echo,[Cleaning] Commands)
	@rm -f 'compile_commands.json'
	@$(call echo,[Done])


# --- OPTIONAL LIBRARY MANAGEMENT ---

# A list of shared libraries currently sitting in the build directory.
override shared_libraries := $(wildcard $(dir $(OUTPUT_FILE_EXT))*$(pattern_dll))
# A command to erase all `shared_libraries`.
override erase_shared_libraries = $(if $(shared_libraries),@true $(foreach x,$(shared_libraries),; rm -f $(call quote,$x)))

# Public: erase shared libraries that were copied into the build directory.
.PHONY: clean_shared_libs
clean_shared_libs: __no_mode_needed
	@$(call echo,[Cleaning] Shared libraries)
	$(erase_shared_libraries)
	@$(call echo,[Done])

# Public: same as `clean_shared_libs`, but also erase cached library information.
.PHONY: clean_deps
clean_deps: __no_mode_needed
	@$(call echo,[Cleaning] Dependencies)
	$(erase_shared_libraries)
	@rm -f $(call quote,$(lib_pack_info_file))
	@$(call echo,[Done])

# Public: update dependency information.
# This calls `pkg-config` on a directory that should contain prebuilt dependencies, and saves the flags that it reports.
# Yes, this is effectively an empty target. As long as it's mentioned in `targets_requiring_deps_info`, it does the job.
.PHONY: find_deps
find_deps: __no_mode_needed
	$(if $(make_was_restarted),@$(call echo,[Done]))


# A list of targets that need the dependency information.
override targets_requiring_deps_info := build commands commands_fixed find_deps


ifneq ($(dependency_management_enabled),)
ifneq ($(strip $(if $(MAKECMDGOALS),$(filter $(targets_requiring_deps_info),$(MAKECMDGOALS)),yes)),)# Don't update dependencies unless necessary.

# A path to the library pack.
override library_pack_path = $(LIBRARY_PACK_DIR)/$(LIBRARY_PACK_NAME)

# A pattern for a dependency archive.
override library_pack_archive_pattern = ./$(LIBRARY_PACK_NAME)_prebuilt_$(TARGET_OS)*.tar.gz
override library_pack_archive_pattern_display = $(library_pack_archive_pattern)

# Same as $(PKGCONFIG), but with some commands to set proper library paths.
# Using `=` here instead of `:=`, because this variable isn't required very often.
override pkgconfig_with_path = env $(call quote,PKG_CONFIG_PATH=) $(call quote,PKG_CONFIG_LIBDIR=$(library_pack_path)/lib/pkgconfig) $(PKGCONFIG)

# `run_pkgconfig` - runs pkg-config with $1 (--cflags or --libs) as flags, and sanitizes the results.
override banned_pkgconfig_flag_patterns := -rpath --enable-new-dtags
override run_pkgconfig_low = $(if $(strip $2),$(foreach x,$(call safe_shell,$1 $2),$(if $(call find_any_as_substr,$(banned_pkgconfig_flag_patterns),$x),,$x)))
override run_pkgconfig = $(strip $(call run_pkgconfig_low,$(pkgconfig_with_path) $1 --define-prefix,$(USED_PACKAGES)) $(call run_pkgconfig_low,$(PKGCONFIG) $1,$(USED_EXTERNAL_PACKAGES)))

# Extra library pack flags.
override extra_pkgconfig_compiler_flags :=
override extra_pkgconfig_linker_flags :=
ifeq ($(TARGET_OS),linux)
override extra_pkgconfig_linker_flags := -Wl,-rpath='$$ORIGIN'
endif


# Check if we should update the dependencies.
ifneq ($(strip $(LIBRARY_PACK_NAME)),$(strip $(deps_library_pack_name)))
$(lib_pack_info_file): __force_this_target
endif
ifneq ($(strip $(USED_PACKAGES)),$(strip $(deps_packages)))
$(lib_pack_info_file): __force_this_target
endif
ifneq ($(strip $(USED_EXTERNAL_PACKAGES)),$(strip $(deps_external_packages)))
$(lib_pack_info_file): __force_this_target
endif
ifneq ($(strip $(ALWAYS_COPIED_SHARED_LIB_PATTERS)),$(strip $(deps_always_copied_shared_lib_patterns)))
$(lib_pack_info_file): __force_this_target
endif

# Make sure everything is rebuilt if dependency information is updated.
$(OUTPUT_FILE_EXT) $(objects) $(compiled_headers): | $(lib_pack_info_file)


# Generate the library pack info.
# Note that `deps_packages` is the last variable. If `pkg-config` fails, the file won't contain this variable, which will cause it to be regenerated.
$(lib_pack_info_file):
	$(info [Deps] Updating dependency information...)
	$(if $(wildcard $(library_pack_path)),,\
		$(call var,_local_archive_path := $(wildcard $(library_pack_archive_pattern)))\
		$(if $(filter 1,$(words $(_local_archive_path))),\
			$(info [Deps] Unpacking `$(_local_archive_path)`...)\
				$(call safe_shell_exec,mkdir -p $(call quote,$(library_pack_path)) && tar -C $(LIBRARY_PACK_DIR) -xf $(_local_archive_path))\
				$(info [Deps] Done. This file is no longer needed, you can remove it.),\
			$(error Prebuilt dependencies not found in `$(library_pack_path)`.\
				$(lf)I can install them for you; I need `$(library_pack_archive_pattern_display)` in the current directory)))
	$(erase_shared_libraries)
	$(call safe_shell_exec,$(call echo,override deps_library_pack_name := $(LIBRARY_PACK_NAME)) >$@)
	$(call safe_shell_exec,$(call echo,override deps_packages := $(USED_PACKAGES)) >>$@)
	$(call safe_shell_exec,$(call echo,override deps_external_packages := $(USED_EXTERNAL_PACKAGES)) >>$@)
	$(call safe_shell_exec,$(call echo,override deps_always_copied_shared_lib_patterns := $(ALWAYS_COPIED_SHARED_LIB_PATTERS)) >>$@)
	$(call safe_shell_exec,$(call echo,override deps_compiler_flags := $(subst $$,$$$$,$(call run_pkgconfig,--cflags) $(extra_pkgconfig_compiler_flags))) >>$@)
	$(call safe_shell_exec,$(call echo,override deps_linker_flags := $(subst $$,$$$$,$(call run_pkgconfig,--libs) $(extra_pkgconfig_linker_flags))) >>$@)
	$(eval include $(lib_pack_info_file))
	$(info [Deps] Packages used:)
	$(info [Deps]   $(deps_packages))
	$(info [Deps] External packages used:)
	$(info [Deps]   $(deps_external_packages))
	$(info [Deps] Compiler flags:)
	$(info [Deps]   $(deps_compiler_flags))
	$(info [Deps] Linker flags:)
	$(info [Deps]   $(deps_linker_flags))
	$(info [Deps] Suggesting a clean rebuild!)


# Copy shared libraries (right after building the executable)
ifeq ($(shared_libraries),)

override deps_entry_dir = $(subst >, ,$(word 1,$(subst <, ,$1)))
override deps_entry_name = $(subst >, ,$(word 2,$(subst <, ,$1)))
override deps_entry_summary = $(call deps_entry_dir,$1) >> $(call deps_entry_name,$1)

# A list of shared libraries found in the library pack.
override available_libs = $(strip $(foreach x,$(notdir $(wildcard $(library_pack_path)/$(directory_dll)/*$(pattern_dll))),$(if $(call is_canonical_dll_name,$x),$x)))
# A list of patterns for dynamic libraries that we might need, guessed based on the linker flags.
ifeq ($(TARGET_OS),windows)
# `[lib]$name[-???].dll`.
override needed_lib_patterns = $(foreach x,$(patsubst -l%,%,$(filter -l%,$(deps_linker_flags))),$x.dll lib$x.dll $x-%.dll lib$x-%.dll)
else
# `lib$name[-???].so[???]`. The `-???` part is rare, but apparently SDL2 uses it.
override needed_lib_patterns = $(foreach x,$(patsubst -l%,%,$(filter -l%,$(deps_linker_flags))),lib$x.so% lib$x-%)
endif
# A list of libraries from the pack that the program needs, guessed based on the linker flags.
# Equal to `available_libs`, with libs not mentioned in the linker flags (or `$(deps_always_copied_shared_lib_patterns)`) removed.
override needed_available_libs = $(strip $(foreach x,$(available_libs),$(if $(filter $(needed_lib_patterns),$x)$(call find_any_as_substr,$(deps_always_copied_shared_lib_patterns),$x),$x)))

ifeq ($(TARGET_OS),linux)
override ldd_with_path = export LD_LIBRARY_PATH=$(call quote,$(library_pack_path)/$(directory_dll):)"$$LD_LIBRARY_PATH" && $(LDD)
else
override ldd_with_path = export PATH=$(call quote,$(library_pack_path)/$(directory_dll):)"$$PATH" && $(LDD)
endif

# On Windows does nothing. On Linux, uses `patchelf` to add `$ORIGIN` to RPATH of library $1, and also calls `chmod -x` on it.
ifeq ($(TARGET_OS),linux)
override finalize = \
	$(call safe_shell_exec,$(PATCHELF) --set-rpath '$$ORIGIN' $(call quote,$1))\
	$(call safe_shell_exec,chmod -x $(call quote,$1))
# Alternative implementation, which appends `$ORIGIN` to rpath without removing existing entries.
#	$(call safe_shell_exec,$(PATCHELF) --set-rpath \
#		$(call quote,$(subst <, ,$(subst $(space),:,$(strip $$ORIGIN \
#			$(foreach y,$(subst :, ,$(subst $(space),<,$(call safe_shell,$(PATCHELF) --print-rpath $(call quote,$1)))),$(if $(call find_any_as_substr,$(dollar)ORIGIN $(dollar)(ORIGIN),$y),,$y))\
#		)))) $(call quote,$1)\
#	)
else
override finalize =
endif

.PHONY: __shared_libs
__generic_build: | __shared_libs
__shared_libs: | $(OUTPUT_FILE_EXT)
	$(info [Deps] Following prebuilt libraries will be copied to `$(dir $(OUTPUT_FILE_EXT))`:)
	$(foreach x,$(needed_available_libs),$(info [Deps] - $(library_pack_path)/$(directory_dll)/$x))
	$(foreach x,$(needed_available_libs),$(call safe_shell_exec,cp -f $(call quote,$(library_pack_path)/$(directory_dll)/$x) $(call quote,$(dir $(OUTPUT_FILE_EXT))))\
		$(call finalize,$(dir $(OUTPUT_FILE_EXT))$x))
	$(info [Deps] Copying completed)
	$(info [Deps] Determining all dependencies for `$(OUTPUT_FILE_EXT)`...)
	$(call var,_local_dep_list := $(call safe_shell,$(ldd_with_path) -- $(OUTPUT_FILE_EXT)))
	$(if $(strip $(_local_dep_list)),,$(error Got empty output from LDD))
	$(call var,_local_dep_list := $(subst $(space)=>$(space),<,$(subst $(space)$(open_par),<,$(subst $(close_par) ,|,$(_local_dep_list)))))
	$(call var,_local_dep_list := $(subst |, ,$(subst $(space),>,$(subst | ,|,$(foreach x,$(_local_dep_list),$(call trim_left,$x))))))
	$(call var,_local_dep_list := $(sort $(foreach x,$(_local_dep_list),$(if $(filter 2,$(words $(subst <, ,$x))),./,$(subst \,/,$(dir $(word 2,$(subst <, ,$x)))))<$(word 1,$(subst <, ,$x)))))
	$(foreach x,$(_local_dep_list),$(info [Deps] - $(call deps_entry_summary,$x)))
	$(call var,_local_libs := $(strip $(foreach x,$(_local_dep_list),\
		$(if \
			$(and \
				$(strip $(foreach y,$(STD_LIB_NAME_PATTERNS),$(findstring $y,$(call deps_entry_name,$x)))),\
				$(filter-out ./,$(call deps_entry_dir,$x))\
			)\
		,\
			$x\
		)\
	)))
	$(foreach x,$(_local_libs),$(if $(call find_any_as_substr,$(BANNED_LIBRARY_PATH_PATTERNS),$(call deps_entry_dir,$x)),\
		$(error Didn't expect to find `$(call deps_entry_name,$x)` in `$(call deps_entry_dir,$x)`)))
	$(info [Deps] Out of those, following libraries will be copied to `$(dir $(OUTPUT_FILE_EXT))`:)
	$(foreach x,$(_local_libs),$(info [Deps] - $(call deps_entry_summary,$x)))
	$(call var,_local_libs := $(foreach x,$(_local_libs),$(if $(call find_elem_in,$(call deps_entry_name,$x),$(needed_available_libs)),\
		$(info [Deps] Skipping prebuilt library: $(call deps_entry_name,$x)),$x)))
	$(foreach x,$(_local_libs),$(call safe_shell_exec,cp -f $(call quote,$(call deps_entry_dir,$x)$(call deps_entry_name,$x)) $(call quote,$(dir $(OUTPUT_FILE_EXT))))\
		$(call finalize,$(dir $(OUTPUT_FILE_EXT))$(call deps_entry_name,$x)))
	$(info [Deps] Copying completed)
	@true
endif

endif
endif


# --- INCLUDE DEPENDENCIES ---
-include $(dep_files)
