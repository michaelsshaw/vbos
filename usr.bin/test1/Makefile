# SPDX-License-Identifier: GPL-2.0-only

# usr.bin/test1/Makefile
#
# test1 is the first test program, to be removed later.
#
# This Makefile also serves as a template for other user programs.

CC := x86_64-elf-gcc
LD := x86_64-elf-ld

OUT := test1.elf
CSRC := test.c
SSRC := start.S
OBJ := $(CSRC:.c=.o) $(SSRC:.S=.o)
DEP := $(OBJ:.o=.d)

CFLAGS := -Wall -ffreestanding -mno-mmx -mno-sse2 -mno-sse -m64 -MMD
LDFLAGS := -nostdlib -static -m elf_x86_64

CURDIR := $(shell pwd | awk -F '/' '{print $$(NF-1),"/",$$NF}' | sed 's/ //g')

$(OUT): $(OBJ)
	@$(LD) $^ $(LDFLAGS) -o $@
	@echo "  USR.LD  $(CURDIR)/$@"

-include $(DEP)

%.o: %.S
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "  USR.CC  $(CURDIR)/$@"

%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@ 
	@echo "  USR.CC  $(CURDIR)/$@"

clean:
	@rm -rf $(OUT) $(OBJ) $(DEP)
