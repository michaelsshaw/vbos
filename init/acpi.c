#include <kernel/acpi.h>
#include <limine/limine.h>

#include <stdio.h>
#include <string.h>

struct limine_rsdp_request rsdp_req = { .id = LIMINE_RSDP_REQUEST,
					.revision = 0 };

static struct rsdp *rsdp = NULL;
struct madt *__madt = NULL;

static const char *madt_entry_types[] = {
	"LAPIC",	    "IOAPIC",	  "IOAPIC_IRQ_SRC_OVRD",
	"IOAPIC_NMI_SRC",   "LAPIC_NMI", "LAPIC_ADDR_OVRD",
	"INVALID",	    "INVALID",	  "INVALID",
	"CPU_LOCAL_X2-APIC"
};

void rsdt_print(struct rsdt *rsdt)
{
	int num_sdt = (rsdt->h.length - sizeof(rsdt->h)) / 4;
	printf("Number of RSDT entries: %d\n", num_sdt);

	for (int i = 0; i < num_sdt; i++) {
		/* 2 steps silences warning */
		uint64_t sdt_addr = (uint64_t)rsdt->sdt[i];
		struct acpi_sdt_header *h = (struct acpi_sdt_header *)sdt_addr;

		if (!(memcmp(h->signature, "APIC", 4))) {
			__madt = (struct madt *)h;
		}

		char buf[9];
		memcpy(buf, h->signature, 4);
		printf("[ACPI] %s %d %x \n", buf, h->revision, h->checksum);
	}
}

int madt_parse_next_entry(int offset)
{
	if ((unsigned)offset >= (__madt->h.length - 1))
		return 0;

	static int n = 0; /* Entry number */

	uint8_t *madt = (uint8_t *)__madt;
	int type = madt[offset];
	int len = madt[offset + 1];

	printf("MADT Entry %d: %s\n", n, madt_entry_types[type]);

	n++;
	return offset + len;
}

void acpi_init()
{
	rsdp = (struct rsdp *)rsdp_req.response->address;

	/* 2 steps silences warning */
	uint64_t rsdt_addr = (uint64_t)rsdp->rsdt_addr;
	struct rsdt *rsdt = (struct rsdt *)rsdt_addr;
	rsdt_print(rsdt);

	if (__madt == NULL) {
		printf("Failed to locate MADT\n");
	} else {
		/* First variable entry in MADT */
		int offset = 0x2C;
		int length = __madt->h.length;
		printf("MADT Length: %x\n", length);

		while ((offset = madt_parse_next_entry(offset)))
			;
	}

	printf("ACPI version: %d\n", rsdp->revision);
	printf("ACPI initialized\n");
}
