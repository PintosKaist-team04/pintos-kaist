/* anon.c: 디스크 이미지가 아닌 페이지(일명 익명 페이지)의 구현. */
/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

#include <bitmap.h>

#include "threads/mmu.h"

static struct disk *swap_disk;
struct bitmap *swap_table;
struct lock bitmap_lock;
/* 아래 줄을 수정하지 마세요 */
/* DO NOT MODIFY BELOW LINE */

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
	swap_disk = disk_get(1, 1);
	swap_table = bitmap_create(disk_size(swap_disk) / 8); // 디스크는 섹터(512바이트) 단위로 관리함 그래서 8 섹터가 있어야 하나의 페이지를 저장가능
	lock_init(&bitmap_lock);
}

/* 파일 매핑 초기화 */
/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* 핸들러 설정 */
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->swap_idx = BITMAP_ERROR;

	return true;
}

/* 스왑 디스크에서 내용을 읽어 페이지를 스왑 인합니다. */
/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	size_t swap_idx = anon_page->swap_idx;
	if (!bitmap_test(swap_table, swap_idx)) {
		return false;
	}
	if (swap_idx == BITMAP_ERROR) {
		PANIC("swap_in idx is crazy");
		return false;
	}

	for (int i = 0; i < 8; i++) {
		disk_read(swap_disk, swap_idx * 8 + i, kva + i * DISK_SECTOR_SIZE);
	}

	page->frame->kva = kva;

	lock_acquire(&bitmap_lock);
	bitmap_set(swap_table, swap_idx, false);
	lock_release(&bitmap_lock);

	return true;
}

/* 내용을 스왑 디스크에 쓰면서 페이지를 스왑 아웃합니다. */
/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	lock_acquire(&bitmap_lock);
	size_t swap_idx = bitmap_scan_and_flip(swap_table, 0, 1, false);
	lock_release(&bitmap_lock);
	if (swap_idx == BITMAP_ERROR) {
		return false;
	}
	anon_page->swap_idx = swap_idx;

	for (int i = 0; i < 8; i++) {
		disk_write(swap_disk, swap_idx * 8 + i, page->frame->kva + DISK_SECTOR_SIZE * i);
	}

	page->frame->page = NULL;
	page->frame = NULL;
	
	
	pml4_clear_page(thread_current()->pml4, page->va);
	return true;
}

/* 익명 페이지를 파괴합니다. PAGE는 호출자에 의해 해제될 것입니다. */
/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	
	// 스왑 테이블에서 스왑 인덱스 해제
	if (anon_page->swap_idx != BITMAP_ERROR) {
		bitmap_reset(swap_table, anon_page->swap_idx);
	}

	// 프레임이 존재하면 프레임을 리스트에서 제거하고 해제
	if (page->frame) {
		list_remove(&page->frame->elem);
		page->frame->page = NULL;
		free(page->frame);
		page->frame = NULL;
	}

	// pml4_clear_page(thread_current()->pml4, page->va);
}
