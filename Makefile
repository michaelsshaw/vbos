# Nuke built-in rules and variables.
override MAKEFLAGS += -rR
override KERNEL := kernel.elf

override CC := x86_64-elf-gcc

# Convenience macro to reliably declare overridable command variables.
define DEFAULT_VAR =
	ifeq ($(origin $1),default)
		override $(1) := $(2)
	endif
	ifeq ($(origin $1),undefined)
		override $(1) := $(2)
	endif
endef

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
$(eval $(call DEFAULT_VAR,CC,cc))

# Same thing for "ld" (the linker).
$(eval $(call DEFAULT_VAR,LD,ld))

# User controllable CFLAGS.
CFLAGS ?= -g -O2 -pipe -Wall -Wextra

# User controllable linker flags. We set none by default.
LDFLAGS ?=

# Internal C flags that should not be changed by the user.
override CFLAGS +=	   		\
	-std=c11			\
	-ffreestanding	   		\
	-fno-stack-protector 		\
	-fno-stack-check	 	\
	-fno-lto			\
	-fno-pie			\
	-fno-pic			\
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

# Internal linker flags that should not be changed by the user.
override LDFLAGS +=		 	\
	-nostdlib			\
	-static				\
	-m elf_x86_64		   	\
	-z max-page-size=0x1000 	\
	-T linker.ld

# Check if the linker supports -no-pie and enable it if it does.
ifeq ($(shell $(LD) --help 2>&1 | grep 'no-pie' >/dev/null 2>&1; echo $$?),0)
	override LDFLAGS += -no-pie
endif

# Use find to glob all *.c, *.S, and *.asm files in the directory and extract the object names.
override CFILES := $(shell find . -type f -name '*.c')
override ASFILES := $(shell find . -type f -name '*.S')
override OBJ := $(CFILES:.c=.o) $(ASFILES:.S=.o) $(NASMFILES:.asm=.o)
override HEADER_DEPS := $(CFILES:.c=.d) $(ASFILES:.S=.d)

# Default target.
.PHONY: all
all: $(KERNEL)

# Link rules for the final kernel executable.
$(KERNEL): $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) -o $@

# Include header dependencies.
-include $(HEADER_DEPS)

# Compilation rules for *.c files.
%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

# Compilation rules for *.S files.
%.o: %.S 
	$(CC) $(CFLAGS) -c $< -o $@

# Remove object files and the final executable.
.PHONY: clean
clean:
	rm -rf $(KERNEL) $(OBJ) $(HEADER_DEPS)

.PHONY: distclean
distclean: clean
	rm -f 

.PHONY += run
run:
	make -C .. run-hdd

.PHONY += whichcc
whichcc:
	echo "$(CC)"
