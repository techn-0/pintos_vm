#ifndef THREADS_PALLOC_H
#define THREADS_PALLOC_H

#include <stdint.h>
#include <stddef.h>

/* How to allocate pages. */
enum palloc_flags {
	PAL_ASSERT = 001,           /* Panic on failure. */
	PAL_ZERO = 002,             /* Zero page contents. */
	PAL_USER = 004              /* User page. */
};

/* Maximum number of pages to put in user pool. */
extern size_t user_page_limit;

uint64_t palloc_init (void);
void *palloc_get_page (enum palloc_flags); // 물리 페이지를 할당하고, 해당 페이지의 커널 가상 주소를 반환
void *palloc_get_multiple (enum palloc_flags, size_t page_cnt);
void palloc_free_page (void *);
void palloc_free_multiple (void *, size_t page_cnt);

#endif /* threads/palloc.h */
