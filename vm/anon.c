/* anon.c: 디스크 이미지가 아닌 페이지(일명 익명 페이지)의 구현. */
/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

/* 아래 줄을 수정하지 마세요 */
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* 이 구조체를 수정하지 마세요 */
/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* 익명 페이지를 위한 데이터 초기화 */
/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* 할 일: 스왑 디스크를 설정하세요. */
	/* TODO: Set up the swap_disk. */
	swap_disk = NULL;
}

/* 파일 매핑 초기화 */
/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* 핸들러 설정 */
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
}

/* 스왑 디스크에서 내용을 읽어 페이지를 스왑 인합니다. */
/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* 내용을 스왑 디스크에 쓰면서 페이지를 스왑 아웃합니다. */
/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* 익명 페이지를 파괴합니다. PAGE는 호출자에 의해 해제될 것입니다. */
/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
