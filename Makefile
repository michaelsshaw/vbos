MAKEFLAGS += -rR
KERNEL := kernel.elf

CC := x86_64-elf-gcc
LD := x86_64-elf-ld

CFLAGS := -g -O0 -pipe -Wall

CFLAGS +=		   		\
	-std=gnu99			\
	-ffreestanding	   		\
	-fno-stack-protector 		\
	-fno-stack-check	 	\
	-fno-lto			\
	-fno-pie			\
	-fno-pic			\
	-fno-builtin			\
	-DKDEBUG			\
	-m64				\
	-march=x86-64			\
	-mabi=sysv		   	\
	-mno-80387		   	\
	-mno-mmx			\
	-mno-sse			\
	-mno-sse2			\
	-mno-red-zone			\
	-mcmodel=kernel 		\
	-Iinclude 			\
	-MMD 		

LDFLAGS :=		 		\
	-nostdlib			\
	-static				\
	-m elf_x86_64		   	\
	-z max-page-size=0x1000 	\
	-T linker.ld			\
	-no-pie				\

KDIRS := dev/ fs/ lib/ mem/ sched/ init/

CFILES := $(shell find $(KDIRS) -type f -name '*.c')
ASFILES := $(shell find $(KDIRS) -type f -name '*.S')
OBJ := $(CFILES:.c=.o) $(ASFILES:.S=.o)
HEADER_DEPS := $(CFILES:.c=.d) $(ASFILES:.S=.d)

.PHONY: all
all: $(KERNEL)

.PHONY: _usr
_usr:
	@cd usr.bin && ./make.sh install

$(KERNEL): $(OBJ) _usr linker.ld
	@echo "  LD      $@"
	@$(LD) $(OBJ) $(LDFLAGS) -o $@

-include $(HEADER_DEPS)

%.o: %.c 
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S 
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@echo "  CLEAN   kernel"
	@rm -rf $(KERNEL) $(OBJ) $(HEADER_DEPS) bin
	@echo "  CLEAN   usr.bin"
	@cd usr.bin && ./make.sh clean

.PHONY: distclean
distclean: clean
	rm -f 

.PHONY += run
run:
	make -C .. run

.PHONY += debug
debug:
	make -C .. run-debug
