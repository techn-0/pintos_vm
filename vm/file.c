/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
// 휘건 추가
#include "threads/vaddr.h"
#include "userprog/process.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

// 휘건 추가
// lazy_load_arg 구조체 정의
// struct lazy_load_arg
// {
// 	struct file *file;
// 	off_t ofs;
// 	size_t read_bytes;
// 	size_t zero_bytes;
// };
static bool
lazy_load_segment(struct page *page, void *aux);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void)
{
	// 휘건 추가
	// todo: file-backed page와 관련된 모든 설정을 수행할 수 있습니다.
}

bool mapFileToPage(struct page *page, struct fileReader *fr)
{
	file_seek(fr->target, fr->offset);
	if (file_read(fr->target, page->frame->kva, fr->pageReadBytes) != fr->pageReadBytes)
	{
		palloc_free_page(page->frame->kva);
		return false;
	}
	memset(page->frame->kva + fr->pageReadBytes, 0, fr->pageZeroBytes);
	pml4_set_dirty(thread_current()->pml4, page->va, false);
	return true;
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	// 먼저 page->operations에 file-backed pages에 대한 핸들러를 설정합니다.
	// page->operations = &file_ops;

	// struct file_page *file_page = &page->file;

	// // todo: page struct의 일부 정보(such as 메모리가 백업되는 파일과 관련된 정보)를 업데이트할 수도 있습니다.
	// struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)page->uninit.aux;
	// file_page->file = lazy_load_arg->file;
	// file_page->ofs = lazy_load_arg->ofs;
	// file_page->read_bytes = lazy_load_arg->read_bytes;
	// file_page->zero_bytes = lazy_load_arg->zero_bytes;
	// return true;
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	file_page->fr = page->uninit.aux;
	return mapFileToPage(page, file_page->fr);
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page = &page->file;
	return mapFileToPage(page, file_page->fr);
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
	//	여기 쓸 것
	unmapFileFromPage(page);
	pml4_set_accessed(thread_current()->pml4, page->va, false);
	pml4_clear_page(thread_current()->pml4, page->va);
	page->frame = NULL;
	return true;
}

void unmapFileFromPage(struct page *page)
{
	struct fileReader *fr = page->file.fr;
	if (pml4_is_dirty(thread_current()->pml4, page->va))
	{
		file_seek(fr->target, fr->offset);
		file_write(fr->target, page->frame->kva, fr->pageReadBytes);
	}
	pml4_set_dirty(thread_current()->pml4, page->va, false);
}
/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	// struct file_page *file_page UNUSED = &page->file;
	// // 휘건 추가
	// if (pml4_is_dirty(thread_current()->pml4, page->va))
	// {
	// 	file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
	// 	pml4_set_dirty(thread_current()->pml4, page->va, 0);
	// }
	// pml4_clear_page(thread_current()->pml4, page->va);
	struct file_page *file_page UNUSED = &page->file;

	if (page->frame != NULL)
	{
		unmapFileFromPage(page);
		palloc_free_page(page->frame->kva);
		free(page->frame);
	}

	pml4_set_accessed(thread_current()->pml4, page->va, false);
	pml4_clear_page(thread_current()->pml4, page->va);
}

/* Do the mmap */

// 휘건 추가
static bool
lazy_load_mmap(struct page *page, void *aux)
{
	struct lazy_load_arg *l = aux;

	file_seek(l->file, l->ofs);

	if (file_read(l->file, page->frame->kva, l->read_bytes) == NULL)
	{
		palloc_free_page(page->frame->kva);
		return false;
	}

	memset(page->frame->kva + l->read_bytes, 0, l->zero_bytes);
	pml4_set_dirty(thread_current()->pml4, page->va, true);
	return true;
}

// void *
// do_mmap(void *addr, size_t length, int writable,
// 		struct file *file, off_t offset)
// {
// 	// printf("집에보내줘");
// 	// 휘건 추가
// 	struct file *f = file_reopen(file);
// 	void *start_addr = addr; // 매핑 성공 시 파일이 매핑된 가상 주소 반환하는 데 사용
// 	int total_page_count = length <= PGSIZE ? 1 : length % PGSIZE ? length / PGSIZE + 1
// 																  : length / PGSIZE; // 이 매핑을 위해 사용한 총 페이지 수

// 	size_t read_bytes = file_length(f) < length ? file_length(f) : length;
// 	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

// 	// ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
// 	// ASSERT(pg_ofs(addr) == 0);	  // upage가 페이지 정렬되어 있는지 확인
// 	// ASSERT(offset % PGSIZE == 0); // ofs가 페이지 정렬되어 있는지 확인

// 	while (read_bytes > 0 || zero_bytes > 0)
// 	{

// 		/* 이 페이지를 채우는 방법을 계산
// 		파일에서 PAGE_READ_BYTES 바이트를 읽고
// 		최종 PAGE_ZERO_BYTES 바이트를 0으로 채움 */
// 		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
// 		size_t page_zero_bytes = PGSIZE - page_read_bytes;

// 		struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)malloc(sizeof(struct lazy_load_arg));
// 		lazy_load_arg->file = f;
// 		lazy_load_arg->ofs = offset;
// 		lazy_load_arg->read_bytes = page_read_bytes;
// 		lazy_load_arg->zero_bytes = page_zero_bytes;

// 		// vm_alloc_page_with_initializer를 호출하여 대기 중인 객체를 생성
// 		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
// 											writable, lazy_load_mmap, lazy_load_arg))
// 			return NULL;

// 		struct page *p = spt_find_page(&thread_current()->spt, start_addr);
// 		p->mapped_page_count = total_page_count;

// 		/* Advance. */
// 		// 읽은 바이트와 0으로 채운 바이트를 추적하고 가상 주소를 증가
// 		read_bytes -= page_read_bytes;
// 		zero_bytes -= page_zero_bytes;
// 		addr += PGSIZE;
// 		offset += page_read_bytes;
// 	}

// 	return start_addr;
// }

void *
do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset)
{
	void *firstPage = addr;
	struct file *target = file_reopen(file);

	//-------
	if (!target)
	{
		// printf("Failed to reopen the file\n");
		return NULL;
	}

	size_t readLength = file_length(target) < length ? file_length(target) : length;
	size_t zeroBytes = PGSIZE - (readLength % PGSIZE);

	//----------------
	// printf("do_mmap: addr=%p, length=%zu, writable=%d, offset=%jd\n", addr, length, writable, (intmax_t)offset);

	while (readLength > 0 || zeroBytes > 0)
	{
		size_t page_read_bytes = readLength < PGSIZE ? readLength : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		//---------------------
		// printf("Allocating page: addr=%p, page_read_bytes=%zu, page_zero_bytes=%zu\n",
			//    addr, page_read_bytes, page_zero_bytes);

		//	준용 추가
		struct fileReader *aux = NULL;
		aux = malloc(sizeof(struct fileReader));
		aux->target = target;
		aux->pageReadBytes = page_read_bytes;
		aux->pageZeroBytes = page_zero_bytes;
		aux->offset = offset;
		aux->mappedCnt = (addr == firstPage) ? (readLength + zeroBytes) / PGSIZE : 0;

		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
											writable, NULL, aux))
		{
			// ------------------------------
			// printf("vm_alloc_page_with_initializer failed at addr=%p\n", addr);

			return NULL;
		}

		/* Advance. */
		readLength -= page_read_bytes;
		zeroBytes -= page_zero_bytes;
		addr += PGSIZE;
		//	준용 추가
		offset += page_read_bytes;
		//-----------------------------------
		// printf("Updated readLength=%zu, zeroBytes=%zu, new addr=%p\n", readLength, zeroBytes, addr);
	}
	//-------------------------------
	// printf("do_mmap completed successfully, returning addr=%p\n", firstPage);
	return firstPage;
}

/* Do the munmap */
// void do_munmap(void *addr)
// {
// 	struct supplemental_page_table *spt = &thread_current()->spt;
// 	struct page *p = spt_find_page(spt, addr);
// 	int count = p->mapped_page_count;

// 	for (int i = 0; i < count; i++)
// 	{
// 		if (p)
// 			destroy(p);
// 		// {
// 		// 	if (pml4_get_page(thread_current()->pml4, p->va))
// 		// 		// 매핑된 프레임이 있다면 = swap out 되지 않았다면 -> 페이지를 제거하고 연결된 프레임도 제거
// 		// 		spt_remove_page(spt, p);
// 		// 	else
// 		// 	{ // swap out된 경우에는 매핑된 프레임이 없으므로 페이지만 제거
// 		// 		hash_delete(&spt->spt_hash, &p->hash_elem);
// 		// 		free(p);
// 		// 	}
// 		// }
// 		// pml4_clear_page(thread_current()->pml4, p->va);

// 		// 휘건 추가
// 		if (p != NULL) {
//             // 매핑된 프레임이 있다면
//             if (pml4_get_page(thread_current()->pml4, p->va)) {
//                 // 페이지와 매핑된 프레임 해제
//                 spt_remove_page(spt, p);
//             } else {
//                 // 프레임이 매핑되지 않은 경우, 페이지만 제거
//                 hash_delete(&spt->spt_hash, &p->hash_elem);
//                 free(p);  // 페이지 구조체 메모리 해제
//             }
//         }

// 		addr += PGSIZE;
// 		p = spt_find_page(spt, addr);
// 	}
// }
void do_munmap(void *addr)
{
	struct page *victim = spt_find_page(&thread_current()->spt, addr);
	struct page *victimFile = victim->file.fr->target;
	if (VM_TYPE(victim->operations->type) != VM_FILE || victim->file.fr->mappedCnt == 0)
	{
		return;
	}
	int unmapCnt = victim->file.fr->mappedCnt;
	while (unmapCnt > 0)
	{
		victim = spt_find_page(&thread_current()->spt, addr);
		spt_remove_page(&thread_current()->spt, victim);
		addr += PGSIZE;
		unmapCnt--;
	}
	file_close(victimFile);
}

// 휘건 추기
static bool
lazy_load_segment(struct page *page, void *aux)
{
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */

	// 휘건 추가
	struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)aux;

	// 1) 파일의 position을 ofs으로 지정한다.
	file_seek(lazy_load_arg->file, lazy_load_arg->ofs);
	// 2) 파일을 read_bytes만큼 물리 프레임에 읽어 들인다.
	if (file_read(lazy_load_arg->file, page->frame->kva, lazy_load_arg->read_bytes) != (int)(lazy_load_arg->read_bytes))
	{
		palloc_free_page(page->frame->kva);
		return false;
	}
	// 3) 다 읽은 지점부터 zero_bytes만큼 0으로 채운다.
	memset(page->frame->kva + lazy_load_arg->read_bytes, 0, lazy_load_arg->zero_bytes);

	return true;
}