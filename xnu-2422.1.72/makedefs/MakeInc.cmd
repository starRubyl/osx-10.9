# -*- mode: makefile;-*-
#
# Copyright (C) 1999-2012 Apple Inc. All rights reserved.
#
# MakeInc.cmd contains command paths for use during
# the build, as well as make fragments and text
# strings that may be evaluated as utility functions.
#

#
# Commands for the build environment
#
##
# Verbosity
##
ifeq ($(RC_XBS),YES)
VERBOSE = YES
else
VERBOSE = NO
endif
ifeq ($(VERBOSE),YES)
_v =
_vstdout =
else
_v = @
_vstdout = > /dev/null
endif

ifeq ($(VERBOSE),YES)
	XCRUN = /usr/bin/xcrun -verbose
else
	XCRUN = /usr/bin/xcrun
endif

SDKROOT ?= /
HOST_SDKROOT ?= macosx
HOST_SPARSE_SDKROOT ?= /

# SDKROOT may be passed as a shorthand like "iphoneos.internal". We
# must resolve these to a full path and override SDKROOT.

ifeq ($(SDKROOT_RESOLVED),)
export SDKROOT_RESOLVED := $(shell $(XCRUN) -sdk $(SDKROOT) -show-sdk-path)
ifeq ($(strip $(SDKROOT)_$(SDKROOT_RESOLVED)),/_)
export SDKROOT_RESOLVED := /
endif
endif
override SDKROOT = $(SDKROOT_RESOLVED)

ifeq ($(HOST_SDKROOT_RESOLVED),)
export HOST_SDKROOT_RESOLVED := $(shell $(XCRUN) -sdk $(HOST_SDKROOT) -show-sdk-path)
endif
override HOST_SDKROOT = $(HOST_SDKROOT_RESOLVED)

ifeq ($(PLATFORM),)
	export PLATFORMPATH := $(shell $(XCRUN) -sdk $(SDKROOT) -show-sdk-platform-path)
	export PLATFORM := $(shell echo $(PLATFORMPATH) | sed 's,^.*/\([^/]*\)\.platform$$,\1,')
	ifeq ($(PLATFORM),)
		export PLATFORM := MacOSX
	endif
endif

ifeq ($(SDKVERSION),)
     export SDKVERSION := $(shell $(XCRUN) -sdk $(SDKROOT) -show-sdk-version)
endif

ifeq ($(PLATFORM),iPhoneOS)
	ifeq ($(HOST_SPARSE_SDKROOT),/)
		export HOST_SPARSE_SDKROOT := $(shell $(XCRUN) -sdk iphonehost.internal -show-sdk-path)
	endif
endif

# CC/CXX get defined by make(1) by default, so we can't check them
# against the empty string to see if they haven't been set
ifeq ($(origin CC),default)
	export CC := $(shell $(XCRUN) -sdk $(SDKROOT) -find clang)
endif
ifeq ($(origin CXX),default)
	export CXX := $(shell $(XCRUN) -sdk $(SDKROOT) -find clang++)
endif
ifeq ($(MIG),)
	export MIG := $(shell $(XCRUN) -sdk $(SDKROOT) -find mig)
endif
ifeq ($(MIGCOM),)
	export MIGCOM := $(shell $(XCRUN) -sdk $(SDKROOT) -find migcom)
endif
ifeq ($(MIGCC),)
	export MIGCC := $(CC)
endif
ifeq ($(STRIP),)
	export STRIP := $(shell $(XCRUN) -sdk $(SDKROOT) -find strip)
endif
ifeq ($(LIPO),)
	export LIPO := $(shell $(XCRUN) -sdk $(SDKROOT) -find lipo)
endif
ifeq ($(LIBTOOL),)
	export LIBTOOL := $(shell $(XCRUN) -sdk $(SDKROOT) -find libtool)
endif
ifeq ($(NM),)
	export NM := $(shell $(XCRUN) -sdk $(SDKROOT) -find nm)
endif
ifeq ($(UNIFDEF),)
	export UNIFDEF := $(shell $(XCRUN) -sdk $(SDKROOT) -find unifdef)
endif
ifeq ($(DSYMUTIL),)
	export DSYMUTIL := $(shell $(XCRUN) -sdk $(SDKROOT) -find dsymutil)
endif
ifeq ($(CTFCONVERT),)
	export CTFCONVERT := $(shell $(XCRUN) -sdk $(SDKROOT) -find ctfconvert)
endif
ifeq ($(CTFMERGE),)
	export CTFMERGE :=  $(shell $(XCRUN) -sdk $(SDKROOT) -find ctfmerge)
endif
ifeq ($(CTFINSERT),)
	export CTFINSERT := $(shell $(XCRUN) -sdk $(SDKROOT) -find ctf_insert)
endif
ifeq ($(NMEDIT),)
	export NMEDIT := $(shell $(XCRUN) -sdk $(SDKROOT) -find nmedit)
endif

# Platform-specific tools
ifeq (iPhoneOS,$(PLATFORM))
ifeq ($(EMBEDDED_DEVICE_MAP),)
	export EMBEDDED_DEVICE_MAP := $(shell $(XCRUN) -sdk $(SDKROOT) -find embedded_device_map)
endif
EDM_DBPATH = $(PLATFORMPATH)/usr/local/standalone/firmware/device_map.db
endif

# Scripts or tools we build ourselves
SEG_HACK = $(OBJROOT)/SETUP/setsegname/setsegname
KEXT_CREATE_SYMBOL_SET = $(OBJROOT)/SETUP/kextsymboltool/kextsymboltool
DECOMMENT = $(OBJROOT)/SETUP/decomment/decomment
NEWVERS = $(SRCROOT)/config/newvers.pl
INSTALL = $(OBJROOT)/SETUP/installfile/installfile

# Standard BSD tools
RM = /bin/rm -f
RMDIR = /bin/rmdir
CP = /bin/cp
MV = /bin/mv
LN = /bin/ln -fs
CAT = /bin/cat
MKDIR = /bin/mkdir -p
FIND = /usr/bin/find
XARGS = /usr/bin/xargs
TAR = /usr/bin/gnutar
BASENAME = /usr/bin/basename
DIRNAME = /usr/bin/dirname
TR = /usr/bin/tr
TOUCH = /usr/bin/touch
AWK = /usr/bin/awk
SED = /usr/bin/sed
ECHO = /bin/echo
PLUTIL = /usr/bin/plutil

#
# Command to generate host binaries. Intentionally not
# $(CC), which controls the target compiler
#
ifeq ($(HOST_OS_VERSION),)
	export HOST_OS_VERSION	:= $(shell sw_vers -productVersion)
endif
ifeq ($(HOST_CC),)
	export HOST_CC		:= $(shell $(XCRUN) -sdk $(HOST_SDKROOT) -find clang)
endif
ifeq ($(HOST_FLEX),)
	export HOST_FLEX	:= $(shell $(XCRUN) -sdk $(HOST_SDKROOT) -find flex)
endif
ifeq ($(HOST_BISON),)
	export HOST_BISON	:= $(shell $(XCRUN) -sdk $(HOST_SDKROOT) -find bison)
endif
ifeq ($(HOST_GM4),)
	export HOST_GM4		:= $(shell $(XCRUN) -sdk $(HOST_SDKROOT) -find gm4)
endif
ifeq ($(HOST_CODESIGN),)
	export HOST_CODESIGN	:= /usr/bin/codesign
endif
ifeq ($(HOST_CODESIGN_ALLOCATE),)
	export HOST_CODESIGN_ALLOCATE	:= $(shell $(XCRUN) -sdk $(HOST_SDKROOT) -find codesign_allocate)
endif

#
# The following variables are functions invoked with "call", and thus
# behave similarly to externally compiled commands
#

# $(1) is an expanded kernel config from a TARGET_CONFIGS_UC tuple
# $(2) is an expanded arch config from a TARGET_CONFIGS_UC tuple
# $(3) is an expanded machine config from a TARGET_CONFIGS_UC tuple
_function_create_build_configs_join = $(strip $(1))^$(strip $(2))^$(strip $(3))

# $(1) is an un-expanded kernel config from a TARGET_CONFIGS_UC tuple
# $(2) is an un-expanded arch config from a TARGET_CONFIGS_UC tuple
# $(3) is an un-expanded machine config from a TARGET_CONFIGS_UC tuple
_function_create_build_configs_do_expand =          $(call _function_create_build_configs_join, \
							   $(if $(filter DEFAULT,$(1)), \
							   	$(DEFAULT_KERNEL_CONFIG), \
								$(1) \
							    ), \
							   $(if $(filter DEFAULT,$(2)), \
							   	$(DEFAULT_ARCH_CONFIG), \
								$(2) \
							    ), \
							   $(if $(filter DEFAULT,$(3)), \
							   	$(if $(filter DEFAULT,$(2)), \
							   	     $(DEFAULT_$(DEFAULT_ARCH_CONFIG)_MACHINE_CONFIG), \
								     $(DEFAULT_$(strip $(2))_MACHINE_CONFIG) \
							    	), \
								$(3) \
							    ) \
						     )

# $(1) is an un-expanded TARGET_CONFIGS_UC list, which must be consumed
#      3 elements at a time
function_create_build_configs = $(sort \
					$(strip \
				       	 	 $(call _function_create_build_configs_do_expand, \
						 	$(word 1,$(1)), \
						 	$(word 2,$(1)), \
						 	$(word 3,$(1)), \
						  ) \
						 $(if $(word 4,$(1)), \
						      $(call function_create_build_configs, \
							     $(wordlist 4,$(words $(1)),$(1)) \
						       ), \
						       \
						  ) \
					  ) \
				   )

# $(1) is a fully-expanded kernel config
# $(2) is a fully-expanded arch config
# $(3) is a fully-expanded machine config. "NONE" is not represented in the objdir path
function_convert_target_config_uc_to_objdir = $(if $(filter NONE,$(3)),$(strip $(1))_$(strip $(2)),$(strip $(1))_$(strip $(2))_$(strip $(3)))

# $(1) is a fully-expanded build config (like "RELEASE^X86_64^NONE")
function_convert_build_config_to_objdir = $(call function_convert_target_config_uc_to_objdir, \
					  	 $(word 1,$(subst ^, ,$(1))), \
						 $(word 2,$(subst ^, ,$(1))), \
						 $(word 3,$(subst ^, ,$(1))) \
					   )

# $(1) is a fully-expanded build config (like "RELEASE^X86_64^NONE")
function_extract_kernel_config_from_build_config  = $(word 1,$(subst ^, ,$(1)))
function_extract_arch_config_from_build_config    = $(word 2,$(subst ^, ,$(1)))
function_extract_machine_config_from_build_config = $(word 3,$(subst ^, ,$(1)))

# $(1) is an input word
# $(2) is a list of colon-separate potential substitutions like "FOO:BAR BAZ:QUX"
# $(3) is a fallback if no substitutions were made
function_substitute_word_with_replacement = $(strip $(if $(2),								\
							 $(if $(filter $(word 1,$(subst :, ,$(word 1,$(2)))),$(1)),	\
							      $(word 2,$(subst :, ,$(word 1,$(2)))),		\
							      $(call function_substitute_word_with_replacement,$(1),$(wordlist 2,$(words $(2)),$(2)),$(3))), \
							 $(3)								\
						     )									\
					     )

# You can't assign a variable to an empty space without these
# shenanigans
empty :=
space := $(empty) $(empty)

# Arithmetic
# $(1) is the number to increment
NUM16 = x x x x x x x x x x x x x x x x 
increment = $(words x $(wordlist 1,$(1),$(NUM16)))

# vim: set ft=make: