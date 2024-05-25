/* file.c: 메모리 지원 파일 객체(매핑된 객체)의 구현. */
/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"

#include "userprog/process.h"
#include "threads/mmu.h"
#include "filesys/filesys.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* 이 구조체를 수정하지 마세요 */
/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* 파일 가상 메모리의 초기화자 */
/* The initializer of file vm */
void
vm_file_init (void) {
}

/* 파일 지원 페이지 초기화 */
/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {

	/* 핸들러 설정 */
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	struct aux *aux = (struct aux *)page->uninit.aux;

	file_page->file = aux->file;
	file_page->ofs = aux->ofs;
	file_page->page_read_bytes = aux->read_bytes;
	file_page->page_zero_bytes = aux->zero_bytes;
	file_page->length = aux->length;

	return true;
}

/* 파일에서 내용을 읽어 페이지를 스왑 인합니다. */
/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* 내용을 파일에 되돌려 쓰면서 페이지를 스왑 아웃합니다. */
/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* 파일 지원 페이지를 파괴합니다. PAGE는 호출자에 의해 해제될 것입니다. */
/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

	if (pml4_is_dirty(thread_current()->pml4, page->va) && page->is_writable) {
		file_write_at(file_page->file, page->va, file_page->page_read_bytes, file_page->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, false); // @todo: 어차피 클리어 해줄건데 왜필요함?
	}
	list_remove(&page->frame->elem);
	free(page->frame);

	pml4_clear_page(thread_current()->pml4, page->va);
}

/* mmap을 실행하세요 */
/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	void * upage = addr;
	size_t read_bytes = length;	//@todo: 만약 파일 길이보다 더 길게 요청했다면??
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;
	off_t ofs = offset;

	file = file_reopen(file);
	if (file == NULL) {
		return NULL;
	}
	
	// ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	// ASSERT(pg_ofs(addr) == 0);
	// ASSERT(offset % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;


		struct aux *aux = malloc(sizeof(struct aux)); // @todo 잊지말고 free하자!
		if(aux == NULL)
			return false;
		aux->file = file;
		aux->ofs = ofs;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;
		aux->length = length; //VM_FILE 전체 페이지 확인용
		
		if (!vm_alloc_page_with_initializer(VM_FILE, upage, writable, lazy_load_segment, aux)) {
			free(aux);
			return NULL;
		}

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		ofs += page_read_bytes;
	}
	// @todo lock 생각하기

	return addr;
}

/* munmap을 실행하세요 */
/* Do the munmap */
void
do_munmap (void *addr) {
	struct page *page;
	page = spt_find_page(&thread_current()->spt, addr);
	spt_remove_page(&thread_current()->spt, page);
}
