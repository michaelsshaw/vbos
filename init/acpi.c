#include <kernel/acpi.h>
#include <limine/limine.h>

#include <stdio.h>

struct limine_rsdp_request rsdp_req = { .id = LIMINE_RSDP_REQUEST,
					.revision = 0 };

static struct rsdp *rsdp;

void acpi_init()
{
	rsdp = (struct rsdp *)rsdp_req.response->address;

	printf("ACPI initialized\n");
}
