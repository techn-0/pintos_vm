/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

// 준용센세
static struct bitmap *swapBits;

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
	/* TODO: Set up the swap_disk. */
	swap_disk = NULL;
	// 준용 추가
	swap_disk = disk_get(1, 1);
	swapBits = bitmap_create(disk_size(swap_disk));
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	// 준용
	anon_page->swapIdx = BITMAP_ERROR;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	struct anon_page *anon_page = &page->anon;
	// 준용
	for (int i = 0; i < (1 << 12) / DISK_SECTOR_SIZE; i++)
	{
		bitmap_reset(swapBits, anon_page->swapIdx + i);
		disk_read(swap_disk, anon_page->swapIdx + i, kva + (DISK_SECTOR_SIZE * i));
	}
	anon_page->swapIdx = BITMAP_ERROR;
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
	// 준용
	anon_page->swapIdx = bitmap_scan(swapBits, 0, (1 << 12) / DISK_SECTOR_SIZE, false);
	if (anon_page->swapIdx == BITMAP_ERROR)
	{
		return false;
	}
	for (int i = 0; i < (1 << 12) / DISK_SECTOR_SIZE; i++)
	{
		bitmap_mark(swapBits, anon_page->swapIdx + i);
		disk_write(swap_disk, anon_page->swapIdx + i, page->frame->kva + (DISK_SECTOR_SIZE * i));
	}

	pml4_set_accessed(thread_current()->pml4, page->va, false);
	pml4_clear_page(thread_current()->pml4, page->va);
	page->frame = NULL;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page *page)
{
	struct anon_page *anon_page = &page->anon;

	// 준용
	pml4_set_accessed(thread_current()->pml4, page->va, false);
	pml4_clear_page(thread_current()->pml4, page->va);
	if (anon_page->swapIdx != BITMAP_ERROR)
	{
		for (int i = 0; i < (1 << 12) / DISK_SECTOR_SIZE; i++)
		{
			bitmap_reset(swapBits, anon_page->swapIdx + i);
		}
	}

	if (page->frame != NULL)
	{
		palloc_free_page(page->frame->kva);
		free(page->frame);
	}
}
