/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ELF_H_
#define _ELF_H_

#include <kernel/common.h>
#include <kernel/proc.h>

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2

#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_ABIVERSION 8
#define EI_PAD 9

#define EI_CLASS_64 2
#define EI_OSABI_SYSV 0

#define EM_X86_64 62

#define EI_VERSION_CURRENT 1

#define ELFCLASS64 2
#define ELFDATA2LSB 1 /* Little endian, two's complement */

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5

#define R_X86_64_NONE 0
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_RELATIVE 8

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_NOBITS 8
#define SHT_REL 9

#define SHF_WRITE 1
#define SHF_ALLOC 2

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2

#define SHN_UNDEF 0
#define SHN_ABS 0xFFF1
#define SHN_COMMON 0xFFF2

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

#define ELF64_R_TYPE(i) ((i)&0xffffffff)
#define ELF64_R_SYM(i) ((i) >> 32)

#define ELF_RELOC_ERR -1

struct elf64_header {
	uint8_t e_ident[16];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
} PACKED;

struct elf64_program_header {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
} PACKED;

struct elf64_section_header {
	uint32_t sh_name;
	uint32_t sh_type;
	uint64_t sh_flags;
	uint64_t sh_addr;
	uint64_t sh_offset;
	uint64_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint64_t sh_addralign;
	uint64_t sh_entsize;
} PACKED;

struct elf64_symbol {
	uint32_t st_name;
	uint8_t st_info;
	uint8_t st_other;
	uint16_t st_shndx;
	uint64_t st_value;
	uint64_t st_size;
} PACKED;

struct elf64_rel {
	uint64_t r_offset;
	uint64_t r_info;
} PACKED;

struct elf64_rela {
	uint64_t r_offset;
	uint64_t r_info;
	int64_t r_addend;
} PACKED;

pid_t elf_load_proc(char *fname);

#endif /* _ELF_H_ */
