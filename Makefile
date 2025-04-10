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
	-mno-mmx			\
	-mno-sse			\
	-mno-sse2			\
	-mno-sse3			\
	-mno-ssse3			\
	-mno-sse4			\
	-mno-sse4a			\
	-mno-sse4.1			\
	-mno-sse4.2			\
	-mno-avx			\
	-mno-avx2			\
	-mno-avx512f			\
	-mno-avx512cd			\
	-mno-avx512vl			\
	-mno-avx512bw			\
	-mno-avx512dq			\
	-mno-avx512ifma			\
	-mno-avx512vbmi			\
	-mno-sha			\
	-mno-aes			\
	-mno-pclmul			\
	-mno-clflushopt			\
	-mno-clwb			\
	-mno-fsgsbase			\
	-mno-ptwrite			\
	-mno-rdrnd			\
	-mno-f16c			\
	-mno-fma			\
	-mno-pconfig			\
	-mno-wbnoinvd			\
	-mno-fma4			\
	-mno-prfchw			\
	-mno-rdpid			\
	-mno-rdseed			\
	-mno-sgx			\
	-mno-xop			\
	-mno-lwp			\
	-mno-3dnow			\
	-mno-3dnowa			\
	-mno-popcnt			\
	-mno-abm			\
	-mno-adx			\
	-mno-bmi			\
	-mno-bmi2			\
	-mno-lzcnt			\
	-mno-fxsr			\
	-mno-xsave			\
	-mno-xsaveopt			\
	-mno-xsavec			\
	-mno-xsaves			\
	-mno-rtm			\
	-mno-hle			\
	-mno-tbm			\
	-mno-mwaitx			\
	-mno-clzero			\
	-mno-pku			\
	-mno-avx512vbmi2		\
	-mno-avx512bf16			\
	-mno-avx512fp16			\
	-mno-gfni			\
	-mno-vaes			\
	-mno-waitpkg			\
	-mno-vpclmulqdq			\
	-mno-avx512bitalg		\
	-mno-movdiri			\
	-mno-movdir64b			\
	-mno-enqcmd			\
	-mno-uintr			\
	-mno-tsxldtrk			\
	-mno-avx512vpopcntdq		\
	-mno-avx512vp2intersect		\
	-mno-avx512vnni			\
	-mno-avxvnni			\
	-mno-cldemote			\
	-mno-serialize			\
	-mno-amx-tile			\
	-mno-amx-int8			\
	-mno-amx-bf16			\
	-mno-hreset			\
	-mno-kl				\
	-mno-widekl			\
	-mno-avxifma			\
	-mno-avxvnniint8		\
	-mno-avxneconvert		\
	-mno-cmpccxadd			\
	-mno-amx-fp16			\
	-mno-prefetchi			\
	-mno-raoint			\
	-mno-amx-complex		\
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
