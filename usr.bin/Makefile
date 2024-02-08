# SPDX-License-Identifier: GPL-2.0-only

# usr.bin/Makefile
#
# Compile all user programs.

UPRG := test1 ftest
UPRGCLEAN := $(addsuffix clean,$(UPRG))
UPRGBUILD := $(addsuffix build,$(UPRG))
UPRGINSTALL := $(addsuffix install,$(UPRG))

all: $(UPRGBUILD) $(UPRGINSTALL)

%build: %
	@$(MAKE) --no-print-directory -C $< &

clean: $(UPRGCLEAN)
%clean: %
	@$(MAKE) -C $< clean
	@echo "  CLEAN   usr.bin/$<"

%install: %
	@mkdir -p ../bin
	@$(MAKE) --no-print-directory -C $< install
