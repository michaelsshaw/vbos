/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/mem.h>
#include <kernel/acpi.h>
#include <dev/apic.h>

#include <limine/limine.h>

#include <stdio.h>
#include <string.h>

struct limine_rsdp_request rsdp_req = { .id = LIMINE_RSDP_REQUEST, .revision = 0 };

static struct rsdp *rsdp = NULL;
struct madt *__madt = NULL;
struct mcfg *__mcfg = NULL;

static void rsdt_parse(struct rsdt *rsdt)
{
	int num_sdt = (rsdt->h.length - sizeof(rsdt->h)) >> 2;

#ifdef KDEBUG
	kprintf(LOG_DEBUG "Number of RSDT entries: %d\n", num_sdt);
#endif /* KDEBUG */

	for (int i = 0; i < num_sdt; i++) {
		/* 2 steps silences warning */
		uintptr_t sdt_addr = (uintptr_t)(rsdt->sdt[i] | hhdm_start);
		struct acpi_sdt_header *h = (struct acpi_sdt_header *)sdt_addr;

		if (!(memcmp(h->signature, "APIC", 4))) {
			__madt = (struct madt *)h;
		}

		if (!(memcmp(h->signature, "MCFG", 4))) {
			__mcfg = (struct mcfg *)h;
		}

#ifdef KDEBUG
		char buf[8] = { 0 };
		memcpy(buf, h->signature, 4);
		kprintf(LOG_DEBUG "RSDT entry: %s %d %x \n", buf, h->revision, h->checksum);
#endif /* KDEBUG */
	}
}

void acpi_init()
{
	rsdp_req.response->address = (void *)((uintptr_t)rsdp_req.response->address & ~(hhdm_start));
	rsdp = kmap_device(rsdp_req.response->address, 0x1000);

	/* 2 steps silences warning */
	uintptr_t rsdt_addr = (uintptr_t)(rsdp->rsdt_addr & ~(hhdm_start));
	struct rsdt *rsdt = kmap_device((void *)rsdt_addr, 0x4000);
	rsdt_parse(rsdt);

#ifdef KDEBUG
	kprintf(LOG_DEBUG "ACPI version: %d\n", rsdp->revision);
#endif /* KDEBUG */
	kprintf(LOG_SUCCESS "ACPI initialized\n");
}
