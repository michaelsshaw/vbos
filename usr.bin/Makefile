# SPDX-License-Identifier: GPL-2.0-only

# usr.bin/Makefile
#
# Compile all user programs.

UPRG := test1
UPRGCLEAN := $(addsuffix clean,$(UPRG))
UPRGBUILD := $(addsuffix build,$(UPRG))


.PHONY: all
all: $(UPRGBUILD)

%build: %
	@$(MAKE) --no-print-directory -C $< 

clean: $(UPRGCLEAN)
%clean: %
	@$(MAKE) -C $< clean
	@echo "  CLEAN   usr.bin/$<"
