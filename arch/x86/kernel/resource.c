// SPDX-License-Identifier: GPL-2.0
#include <linux/efi.h>
#include <linux/ioport.h>
#include <linux/printk.h>
#include <asm/e820/api.h>
#include <asm/pci_x86.h>

static void resource_clip(struct resource *res, resource_size_t start,
			  resource_size_t end)
{
	resource_size_t low = 0, high = 0;

	if (res->end < start || res->start > end)
		return;		/* no conflict */

	if (res->start < start)
		low = start - res->start;

	if (res->end > end)
		high = res->end - end;

	/* Keep the area above or below the conflict, whichever is larger */
	if (low > high)
		res->end = start - 1;
	else
		res->start = end + 1;
}

static void remove_e820_regions(struct resource *avail)
{
	int i;
	struct e820_entry *entry;
	u64 e820_start, e820_end;
	struct resource orig = *avail;

	if (!pci_use_e820)
		return;

	for (i = 0; i < e820_table->nr_entries; i++) {
		entry = &e820_table->entries[i];
		e820_start = entry->addr;
		e820_end = entry->addr + entry->size - 1;

		resource_clip(avail, e820_start, e820_end);
		if (orig.start != avail->start || orig.end != avail->end) {
			pr_info("clipped %pR to %pR for e820 entry [mem %#010Lx-%#010Lx]\n",
				 &orig, avail, e820_start, e820_end);
			orig = *avail;
		}
	}
}

#ifdef CONFIG_EFI
static bool efi_mmio_mem(const struct resource *avail)
{
	resource_size_t start, end;
	efi_memory_desc_t desc;

	if (!efi_enabled(EFI_MEMMAP) ||
	    efi_mem_desc_lookup(avail->start, &desc))
		return false;

	start = desc.phys_addr;
	end = desc.phys_addr + (desc.num_pages << EFI_PAGE_SHIFT) - 1;

	/*
	 * No need to clip the resource if it is fully contained in an
	 * EFI memory mapped region.
	 */
	if (avail->start >= start && avail->end <= end &&
	    desc.type == EFI_MEMORY_MAPPED_IO)
		return true;

	return false;
}
#else
static inline bool efi_mmio_mem(const struct resource *avail)
{
	return false;
}
#endif

void arch_remove_reservations(struct resource *avail)
{
	/*
	 * Trim out BIOS area (high 2MB) and E820 regions. We do not remove
	 * the low 1MB unconditionally, as this area is needed for some ISA
	 * cards requiring a memory range, e.g. the i82365 PCMCIA controller.
	 */
	if (avail->flags & IORESOURCE_MEM) {
		resource_clip(avail, BIOS_ROM_BASE, BIOS_ROM_END);

		if (!efi_mmio_mem(avail))
			remove_e820_regions(avail);
	}
}
