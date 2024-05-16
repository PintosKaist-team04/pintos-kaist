/* vm.c: 가상 메모리 객체에 대한 일반적인 인터페이스. */
/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* 각 하위 시스템의 초기화 코드를 호출하여 가상 메모리 하위 시스템을 초기화합니다. */
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */ 
	/* TODO: Your code goes here. */
}

/* 페이지의 유형을 가져옵니다. 이 함수는 페이지가 초기화된 후의 유형을 알고 싶을 때 유용합니다.
 * 이 함수는 현재 완전히 구현되어 있습니다. */
/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* 도우미 함수들 */
/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* 초기화기와 함께 대기 중인 페이지 객체를 생성합니다. 페이지를 생성하려면 직접 생성하지 말고 이 함수나 vm_alloc_page를 통해 만드세요. */
/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* upage가 이미 사용 중인지 여부를 확인합니다. */
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, VM 유형에 따라 초기화기를 가져오고, 
		 * TODO: 그런 다음 uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다. 
		 * TODO: uninit_new를 호출한 후 필드를 수정해야 합니다. */

		/* TODO: 페이지를 spt에 삽입합니다. */

		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/* spt에서 가상 주소를 찾아 페이지를 반환합니다. 오류 시 NULL을 반환합니다. */
/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */

	return page;
}

/* 페이지를 유효성 검사와 함께 spt에 삽입합니다. */
/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* 대체될 구조 프레임을 가져옵니다. */
/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	
	 /* TODO: 대체 정책은 여러분의 결정에 달려 있습니다. */
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* 한 페이지를 대체하고 해당하는 프레임을 반환합니다.
 * 오류 시 NULL을 반환합니다. */
/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: 희생자를 스왑아웃하고 대체된 프레임을 반환합니다. */
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc()을 호출하고 프레임을 가져옵니다. 사용 가능한 페이지가 없으면 페이지를 대체하고 반환합니다. 
 * 이 함수는 항상 유효한 주소를 반환합니다.  즉, 사용자 풀 메모리가 가득 찬 경우, 이 함수는 사용 가능한 메모리 공간을 얻기 위해 프레임을 대체합니다. */
/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* 스택을 확장합니다. */
/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* 쓰기 보호된 페이지의 오류를 처리합니다. */
/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */ /* TODO: 오류를 유효성 검사하세요. */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* 페이지를 해제합니다.
 * 이 함수를 수정하지 마세요. */
/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */ 
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* VA에 할당된 페이지를 요구합니다. */
/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page (page);
}

/* PAGE를 요구하고 MMU를 설정합니다. */
/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* 링크를 설정합니다. */
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: 페이지 테이블 항목을 삽입하여 페이지의 VA를 프레임의 PA로 매핑합니다. */
	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	return swap_in (page, frame->kva);
}

/* 새 보조 페이지 테이블을 초기화합니다. */
/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
}

/* src에서 dst로 보조 페이지 테이블을 복사합니다. */
/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* 보조 페이지 테이블이 보유한 리소스를 해제합니다. */
/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: 스레드가 보유한 모든 보조 페이지 테이블을 파괴하고, 수정된 내용을 저장소에 씁니다. */
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
