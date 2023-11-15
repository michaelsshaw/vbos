/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ELF_H_
#define _ELF_H_

#include <kernel/common.h>

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

#endif /* _ELF_H_ */
