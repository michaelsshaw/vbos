

CC := x86_64-elf-gcc
LD := x86_64-elf-ld

OUT := ftest.elf
CSRC :=
SSRC := ftest.S
OBJ := $(CSRC:.c=.o) $(SSRC:.S=.o)
DEP := $(OBJ:.o=.d)

CFLAGS := -Wall -ffreestanding -mno-mmx -mno-sse2 -mno-sse -m64 -MMD -g
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

install: $(OUT)
	@cp $(OUT) ../../bin
	@echo "  INSTALL $(CURDIR)/$(OUT)"
