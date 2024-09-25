/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
// 휘건 추가
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "vm/uninit.h"

#define pg_round_down(va) (void *)((uint64_t)(va) & ~PGMASK)
// 보류
static bool
install_page(void *upage, void *kpage, bool writable);

// 휘건 추가
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED);
static unsigned less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);
bool page_insert(struct hash *h, struct page *p);
bool page_delete(struct hash *h, struct page *p);
struct list frame_table;
static struct frame *vm_get_frame(void);

// #include "userprog/process.c"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	// 휘건 추가
	list_init(&frame_table);
	// 보류
	struct list_elem *start = list_begin(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */

		// 휘건 추가
		struct page *page = (struct page *)malloc(sizeof(struct page));
		typedef bool (*initializerFunc)(struct page *, enum vm_type, void *);
		initializerFunc initializer = NULL;

		switch (VM_TYPE(type))
		{
		case VM_ANON:
			initializer = anon_initializer;
			break;
		case VM_FILE:
			initializer = file_backed_initializer;
			break;
		}
		uninit_new(page, upage, init, type, aux, initializer);

		page->writable = writable;
		return spt_insert_page(spt, page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{
	// struct page *page = NULL;
	// /* TODO: Fill this function. */

	// return page;

	// 현재 페이지 구조체의 포인터를 정의
	struct page *page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *e; // 해시 요소를 저장할 포인터를 정의

	// 주어진 가상 주소 va를 페이지 경계로 내림(round down)하여 페이지 구조체의 가상 주소 필드에 저장
	page->va = pg_round_down(va);

	// 페이지 테이블의 해시 테이블에서 해당 가상 주소에 대한 페이지 팀색
	e = hash_find(&spt->spt_hash, &page->hash_elem);
	free(page); // 페이지 구조체는 더 이상 필요 없으므로 메모리를 해제

	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
	// 찾은 해시 요소가 NULL이 아닐 경우, 해당 해시 요소에 대한 페이지 구조체 포인터를 반환
	// NULL일 경우 NULL반환
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	// 페이지를 삽입 성공 여부를 저장할 변수
	// int succ = false;
	// /* TODO: Fill this function. */

	// return succ;
	return page_insert(&spt->spt_hash, page);
	// 주어진 페이지를 spt 페이지 테이블의 해시 테이블에 삽입, 결과를 반환
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	// 휘건 추가
	struct thread *curr = thread_current(); // 현재 스레드의 pml4
	struct list_elem *e, *start;

	// 첫 번째 루프: 현재 위치에서 끝까지 검사
	for (start = e; start != list_end(&frame_table); list_next(start))
	{
		victim = list_entry(start, struct frame, frame_elem); // 현재 리스트 요소를 frame으로 변환
		if (pml4_is_accessed(curr->pml4, victim->page->va))	  // 현재 페이지가 최근에 접근된 적이 있는지 확인
		{
			pml4_set_accessed(curr->pml4, victim->page->va, 0); // 접근된 페이지는 accessed bit를 0으로 초기화
		}
		else
		{
			return victim; // accessed bit가 0인 페이지를 찾으면 바로 그 프레임을 반환
		}
	}

	// 두 번째 루프: 리스트의 처음부터 다시 현재 위치까지 검사
	for (start = list_begin(&frame_table); start != e; start = list_next(start))
	{
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va))
		{
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		}
		else
		{
			return victim;
		}
	}
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */
	// 휘건 추가
	swap_out(victim->page); // 프레임과 연결된 페이지를 스왑 디스크로 내보냄
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void)
{
	// struct frame *frame = NULL;
	// /* TODO: Fill this function. */

	// ASSERT(frame != NULL);
	// ASSERT(frame->page == NULL);
	// return frame;

	// 휘건 추가
	// 새로운 프레임 구조체 메모리 할당
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	ASSERT(frame != NULL);					// 프레임 할당이 제대로 되었는지 확인 (NULL인지 아닌지)
	ASSERT(frame->page == NULL);			// 새로운 프레임이 할당되었을 때, 페이지가 연결되어 있지 않아야 함
	frame->kva = palloc_get_page(PAL_USER); // 커널 가상 주소에 해당하는 페이지 할당 시도
	if (frame->kva == NULL)
	{
		// 페이지 할당에 실패하면 프레임을 교체하는 eviction 수행
		frame = vm_evict_frame();
		frame->page = NULL;
		return frame;
	}
	// 프레임 테이블에 새로 할당된 프레임을 추가
	list_push_back(&frame_table, &frame->frame_elem);
	frame->page = NULL; // 아직 페이지와 연결되지 않았다 표시
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	// 휘건 추가
	if (vm_alloc_page(VM_ANON | VM_MARKER_0, addr, 1))
	{
		vm_claim_page(addr);
		thread_current()->stack_bottom -= PGSIZE;
	}
	
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (is_kernel_vaddr(addr))
	{
		return false;
	}
	void *rsp_stack = is_kernel_vaddr(f->rsp) ? thread_current()->rsp_stack : f->rsp;

	if (not_present)
	{
		if (!vm_claim_page(addr))
		{
			if (rsp_stack - 8 <= addr && USER_STACK - 0x100000 <= addr && addr <= USER_STACK)
			{
				vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
				return true;
			}
			return false;
		}
		else
			return true;
	}
	return false;
	// return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function */

	// 휘건 추가
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL)
	{
		return false;
	}

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	// 휘건 추가
	if (install_page(page->va, frame->kva, page->writable))
	{
		return swap_in(page, frame->kva);
	}
	return false;

	// return swap_in(page, frame->kva);
}

//	준용 추가
bool page_less(const struct hash_elem *a_,
			   const struct hash_elem *b_, void *aux UNUSED)
{
	const struct page *a = hash_entry(a_, struct page, hash_elem);
	const struct page *b = hash_entry(b_, struct page, hash_elem);

	return a->va < b->va;
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	//	준용 추가
	// hash_init(&spt->table, hash_bytes, page_less, NULL);
	lock_init(&spt->sptLock);
	// 휘건 추가
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

// 휘건 추가

// 가상 주소를 hashed index로 변환하는 함수
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED)
{
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	// 주어진 해시 테이블 요소 p_를 page 구조체로 변환 (p_는 page의 hash_elem 필드를 가리킴)
	return hash_bytes(&p->va, sizeof(p->va));
	// 페이지의 가상 주소(va)를 바이트 단위 해싱하여, 해당 주소를 기반으로 해시 값을 반환
}

// 해시 테이블 내 두 페이지 요소에 대해 주소값을 비교하는 함수
static unsigned less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	// 해시 테이블 요소 a와 b를 page 구조체로 변환
	const struct page *p_a = hash_entry(a, struct page, hash_elem);
	const struct page *p_b = hash_entry(b, struct page, hash_elem);
	return p_a->va < p_b->va;
	// p_a의 가상 주소(va)가 p_b의 가상 주소보다 작은 경우 true

	// aux는 추가적인 데이터를 전달할 수 있지만 사용되지 않음
}

// 페이지에 들어있는 hash_elem 구조체를 인지로 받은 해시 테이블에 삽입하는 함수
bool page_insert(struct hash *h, struct page *p)
{
	// 페이지의 hash_elem을 해시 테이블 h에 삽입
	if (!hash_insert(h, &p->hash_elem))
	{
		return true; // 삽입 성공시 NULL을 반환하므로 true
	}
	else
	{
		return false; // 이미 존재하는 경우 hash_insert가 삽입하지 않으므로 false
	}
}

// 페이지에 들어있는 hash_elem 구조체를 해시 테이블에서 삭제하는 함수
bool page_delete(struct hash *h, struct page *p)
{
	// 페이지의 hash_elem을 해시 테이블 h에서 삭제
	if (!hash_delete(h, &p->hash_elem))
	{
		return true; // 삭제 성공시 NULL이 아닌 값이 반환되므로 true
	}
	else
	{
		return false; // 삭제할 요소가 없으면 hash_delete가 NULL을 반환하므로 false
	}
}

// 휘건 추가 보류
static bool
install_page(void *upage, void *kpage, bool writable)
{
	struct thread *t = thread_current();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page(t->pml4, upage) == NULL && pml4_set_page(t->pml4, upage, kpage, writable));
}