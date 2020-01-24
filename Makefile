#
# Copyright (c) 2020, Wuklab, UCSD.
#

# o Do not use make's built-in rules and variables
#   (this increases performance and avoids hard-to-debug behaviour);
# o Look for make include files relative to root of kernel src
MAKEFLAGS += -rR --include-dir=$(CURDIR)

# Do not print "Entering directory ...",
# but we want to display it when entering to the output directory
MAKEFLAGS += --no-print-directory

# Beautify output
ifeq ("$(origin V)", "command line")
  KBUILD_VERBOSE = $(V)
endif
ifndef KBUILD_VERBOSE
  KBUILD_VERBOSE = 0
endif

ifeq ($(KBUILD_VERBOSE), 1)
  Q =
else
  Q = @
endif
export Q KBUILD_VERBOSE

# Current SHELL
CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	        else if [ -x /bin/bash ]; then echo /bin/bash; \
	        else echo sh; fi ; fi)
HOSTCC       = gcc
HOSTCXX      = g++
HOSTCFLAGS   = -Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -fomit-frame-pointer -std=gnu89
HOSTCXXFLAGS = -O2
export CONFIG_SHELL HOSTCC HOSTCXX HOSTCFLAGS HOSTCXXFLAGS

# Configure Board
DEFAULT_BOARD = vcu118

ifeq ("$(origin board)", "command line")
  TARGET_BOARD = $(board)
endif
ifndef TARGET_BOARD
  TARGET_BOARD = $(DEFAULT_BOARD)
  DEFAULT_BOARD_USED = 1
endif

VALID_TARGET_BOARD = vcu108 vcu118

ifeq ($(findstring $(TARGET_BOARD), $(VALID_TARGET_BOARD)), )
  $(error Target board [$(TARGET_BOARD)] not found. Supported boards: $(VALID_TARGET_BOARD))
endif

export TARGET_BOARD DEFAULT_BOARD_USED

GENERATED_IP = generated_ip/

PHONY :=
all:
	$(Q)mkdir -p $(GENERATED_IP)
	$(Q)make -C host
	$(Q)make -C monitor

#
# This cleans up everything.
# Compiling takes time.
# Use with caution.
#
PHONY += clean
clean:
	rm -rf $(GENERATED_IP)
	find ./ -name "generated_ip" | xargs rm -rf
	find ./ -name "generated_hls_project" | xargs rm -rf
	find ./ -name "generated_vivado_project" | xargs rm -rf
	find ./ -name "*.log" | xargs rm -rf
	find ./ -name "*.jou" | xargs rm -rf
	find ./ -name "*.str" | xargs rm -rf
	find ./ -name ".Xil" | xargs rm -rf
	find ./ -name "awsver.txt" | xargs rm -rf
	find ./ -name "ipshared" | xargs rm -rf
	find ./ -name "generated" | xargs rm -rf
	find ./ -name "generated_project" | xargs rm -rf
	find ./ -name "*.o" | xargs rm -rf
	find ./ -name "*.out" | xargs rm -rf

PHONY += help
help:
	@echo  'Build:'
	@echo  '  make board=     - Build for certain board. If board field is not given,'
	@echo  '                    The default board $(DEFAULT_BOARD) will be used.'
	@echo  ''
	@echo  '  make V=0|1 [targets] 0 => quiet build (default), 1 => verbose build'
	@echo  ''
	@echo  'Supported Boards:'
	@$(if $(VALID_TARGET_BOARD),                \
		$(foreach b, $(VALID_TARGET_BOARD), \
		echo "  $(b)";) \
		echo '')
	@echo  'Cleaning targets:'
	@echo  '  clean           - Remove all generated files'

# Declare the contents of the .PHONY variable as phony.  We keep that
# information in a variable so we can use it in if_changed and friends.
.PHONY: $(PHONY)
