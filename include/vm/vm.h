#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include <hash.h>


enum vm_type {
	/* 페이지가 초기화되지 않음 */
	/* page not initialized */
	VM_UNINIT = 0,
	/* 파일과 관련 없는 페이지, 일명 익명 페이지 */
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,	
	/* 파일과 관련된 페이지 */
	/* page that realated to the file */
	VM_FILE = 2,	
	/* 프로젝트 4의 페이지 캐시를 보유하는 페이지 */
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	
	/* 상태를 저장하는 비트 플래그 */
	/* Bit flags to store state */

	/* 정보를 저장하는 보조 비트 플래그 표시기입니다. int에 값이 맞을 때까지 더 많은 표시기를 추가할 수 있습니다. */
	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* 이 값 이상으로는 초과하지 마십시오. */
	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

struct aux {
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
};

/* "페이지"의 표현입니다.
 * 이는 네 개의 "자식 클래스"인 uninit_page, file_page, anon_page 및 페이지 캐시 (프로젝트 4)를 가진 "부모 클래스"의 종류입니다.
 * 이 구조체의 사전 정의된 멤버를 제거하거나 수정하지 마십시오. */
/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */ /* 사용자 공간에서의 주소 */
	struct frame *frame;   /* Back reference for frame */ /* 프레임에 대한 역 참조 */
	
	/* Your implementation */ /* 여러분의 구현 */
	struct hash_elem hash_elem;
	bool is_writable;

	/* 페이지가 중복으로 여러 곳에 저장될 수 있으므로! */
	bool is_exist_frame;
	bool is_exist_swap;
	bool is_exist_disk;
	
	/* 각 유형의 데이터는 연합체에 바인딩됩니다.
	 * 각 함수는 자동으로 현재의 연합체를 감지합니다. */
	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* "프레임"의 표현 */
/* The representation of "frame" */
struct frame {
	void *kva;
	struct page *page;
};

/* 페이지 작업을 위한 함수 테이블입니다.
 * 이것은 C에서 "인터페이스"를 구현하는 한 가지 방법입니다.
 * "메소드"의 테이블을 구조체 멤버로 넣고,
 * 필요할 때마다 호출하십시오. */
/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* 현재 프로세스의 메모리 공간 표현입니다.
 * 이 구조체에 대해 특정한 디자인을 강요하고 싶지 않습니다.
 * 이 구조체에 대한 모든 디자인은 여러분에게 달려 있습니다. */
/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
struct supplemental_page_table {
	struct hash hash_pages;
	// todo : 보조 페이지 구조체 만들기
	// 보조 페이지의 테이블

};


#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

#endif  /* VM_VM_H */
