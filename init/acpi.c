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

void rsdt_print(struct rsdt *rsdt)
{
	int num_sdt = (rsdt->h.length - sizeof(rsdt->h)) / 4;
	printf(LOG_INFO "Number of RSDT entries: %d\n", num_sdt);

	for (int i = 0; i < num_sdt; i++) {
		/* 2 steps silences warning */
		uint64_t sdt_addr = (uint64_t)rsdt->sdt[i] | hhdm_start;
		struct acpi_sdt_header *h = (struct acpi_sdt_header *)sdt_addr;

		if (!(memcmp(h->signature, "APIC", 4))) {
			__madt = (struct madt *)h;
		}

		char buf[8] = { 0 };
		memcpy(buf, h->signature, 4);
		printf(LOG_INFO "RSDT entry: %s %d %x \n", buf, h->revision, h->checksum);
	}
}

void acpi_init()
{
	rsdp = (struct rsdp *)rsdp_req.response->address;

	/* 2 steps silences warning */
	uint64_t rsdt_addr = (uint64_t)rsdp->rsdt_addr | hhdm_start;
	struct rsdt *rsdt = (struct rsdt *)rsdt_addr;
	rsdt_print(rsdt);

	printf(LOG_INFO "ACPI version: %d\n", rsdp->revision);
	printf(LOG_SUCCESS "ACPI initialized\n");
}
