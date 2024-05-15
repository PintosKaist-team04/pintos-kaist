/* page_cache.c: 페이지 캐시 (버퍼 캐시)의 구현. */

#include "vm/vm.h"
static bool page_cache_readahead (struct page *page, void *kva);
static bool page_cache_writeback (struct page *page);
static void page_cache_destroy (struct page *page);

/* 이 구조체는 수정하지 마십시오 */
/* DO NOT MODIFY this struct */
static const struct page_operations page_cache_op = {
	.swap_in = page_cache_readahead,
	.swap_out = page_cache_writeback,
	.destroy = page_cache_destroy,
	.type = VM_PAGE_CACHE,
};

tid_t page_cache_workerd;

/* 파일 vm의 초기화 함수 */
/* The initializer of file vm */
void
pagecache_init (void) {
	/* TODO: 페이지 캐시를 위한 워커 데몬을 page_cache_kworkerd와 함께 생성 */
	/* TODO: Create a worker daemon for page cache with page_cache_kworkerd */
}

/* 페이지 캐시를 초기화합니다 */
/* Initialize the page cache */
bool
page_cache_initializer (struct page *page, enum vm_type type, void *kva) {
	/* 핸들러를 설정합니다 */
	/* Set up the handler */
	page->operations = &page_cache_op;

}

/* 프리페치(readhead)를 구현하기 위해 Swap in 메커니즘을 사용합니다 */
/* Utilize the Swap in mechanism to implement readahead */
static bool
page_cache_readahead (struct page *page, void *kva) {
}

/* 쓰기 백(writeback)을 구현하기 위해 Swap out 메커니즘을 사용합니다 */
/* Utilize the Swap out mechanism to implement writeback */
static bool
page_cache_writeback (struct page *page) {
}

/* 페이지 캐시를 파괴합니다. */
/* Destroy the page cache. */
static void
page_cache_destroy (struct page *page) {
}

/* 페이지 캐시를 위한 워커 스레드 */
/* Worker thread for page cache */
static void
page_cache_kworkerd (void *aux) {
}
