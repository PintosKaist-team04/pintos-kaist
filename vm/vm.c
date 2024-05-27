/* vm.c: 가상 메모리 객체에 대한 일반적인 인터페이스. */
/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

#include "threads/vaddr.h"
#include "threads/mmu.h"


/* 프레임 테이블 */
struct list frame_table;

/* 각 하위 시스템의 초기화 코드를 호출하여 가상 메모리 하위 시스템을 초기화합니다. */
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {

	vm_anon_init ();
	vm_file_init ();
	list_init(&frame_table);
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

/* 해시 도우미 함수들 */
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void page_destructor (struct hash_elem *page_elem, void *aux UNUSED);
		   

/* 초기화기와 함께 대기 중인 페이지 객체를 생성합니다. 페이지를 생성하려면 직접 생성하지 말고 이 함수나 vm_alloc_page를 통해 만드세요. */
/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	/* vm_type 비트마스크 적용 */
	enum vm_type check_type;
	if ((int)type == 9 || (int)type == 1) {
		check_type = VM_ANON;
	} else {
		check_type = type;
	}

	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* upage가 이미 사용 중인지 여부를 확인합니다. */
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, VM 유형에 따라 초기화기를 가져오고, 
		 * TODO: 그런 다음 uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다. 
		 * TODO: uninit_new를 호출한 후 필드를 수정해야 합니다.
		 * TODO: 페이지를 spt에 삽입합니다. 
		 * TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new.
		 * TODO: Insert the page into the spt. */
		struct page *page = malloc(sizeof(struct page));
		if(page == NULL)
			goto err;
		bool (*initializer)(struct page *, enum vm_type, void *kva) = NULL;
		switch (check_type)
		{		
			case VM_ANON:
				initializer = anon_initializer;
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
			/* case VM_PAGE_CACHE:
			 * 	initializer = cache_initializer;
			 * 	break; */
			default:
				goto err;
		}
		uninit_new (page, upage, init, type, aux, initializer);
		
		page->is_writable = writable;

		if(spt_insert_page(spt, page)) return true;
	}
err:
	free(aux);  //@todo: free 맞는지 고민하기
	return false;
}

/* spt에서 가상 주소를 찾아 페이지를 반환합니다. 오류 시 NULL을 반환합니다. */
/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page page;
	struct hash_elem *elem;
	va = pg_round_down(va);
	page.va = va;
	
	elem = hash_find(&spt->hash_pages, &page.hash_elem);

	if (elem != NULL) {
		return hash_entry (elem, struct page, hash_elem);
	}

	return NULL;
}


/* 페이지를 유효성 검사와 함께 spt에 삽입합니다. */
/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	
	/* TODO: Fill this function.
	* 위의 함수는 인자로 주어진 보조 페이지 테이블에 페이지 구조체를 삽입합니다.
	* 이 함수에서 주어진 보충 테이블에서 가상 주소가 존재하지 않는지 검사해야 합니다. */
	if (!hash_insert(&spt->hash_pages, &page->hash_elem))
		succ = true;
		
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->hash_pages, &page->hash_elem);
	// list_remove(&page->frame->elem);
	vm_dealloc_page (page);
}

/* 대체될 구조 프레임을 가져옵니다. */
/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	
	/* TODO: 대체 정책은 여러분의 결정에 달려 있습니다. */
	/* TODO: The policy for eviction is up to you. */
	victim = list_entry(list_pop_front(&frame_table), struct frame, elem);
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
	swap_out(victim->page);
	//어차피 victim에 덮어 씌워주면 됨.
	return victim;
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
	
	frame = malloc(sizeof(struct frame));

	// 프레임 구조체 멤버들 초기화
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	if (frame->kva == NULL) {
		free(frame);
		frame = vm_evict_frame();
	}

	// list_push_back(&frame_table, &frame->elem);

	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* 스택을 확장합니다. */
/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	vm_alloc_page((VM_ANON|VM_MARKER_0), pg_round_down(addr), true);
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
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	void *rsp = get_user_if->rsp;
	
	/* TODO: Validate the fault */ /* TODO: 오류를 유효성 검사하세요. */
	/* TODO: Your code goes here */

	//주소가 존재하지 않을 때 or 커널 가상 주소일 때 (check_address와 똑같음)
	if (addr == NULL || is_kernel_vaddr(addr)){
		exit(-1);
		return false;
	}

	if (not_present){
		if (USER_STACK - (1 << 20) <= rsp - 8 && rsp - 8 == addr && addr <= USER_STACK) {
			vm_stack_growth(addr);
		}
		else if (USER_STACK - (1 << 20) <= rsp && rsp <= addr && addr <= USER_STACK) {
			vm_stack_growth(addr);
		}
		
		page = spt_find_page(spt, addr);

		if (page == NULL){
			exit(-1);
			return false;
		}

		if (write == true & page->is_writable == false){
			exit(-1);
			return false;
		}
		
		return vm_do_claim_page (page);
	}

	exit(-1);
	return false;
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

/* spt 테이블에 VA 키를 가지는 페이지가 있다면 물리 메모리 frame을 만들고 할당하는 함수를 호출합니다. */
/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL) return false;
	return vm_do_claim_page (page);
}

/* 프레임을 생성하고, page와 frame 링크 설정 및 pml4 테이블에 값을 넣고 swap_in 합니다.  */
/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* 링크를 설정합니다. */
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: 페이지 테이블 항목을 삽입하여 
	 * 페이지의 VA를 프레임의 PA로 매핑합니다. */
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	
	if (!pml4_set_page (thread_current()->pml4, page->va, frame->kva, page->is_writable))
	{
		vm_dealloc_page(page); //@todo: frame table에 있으면 안 해제해주기
		return false;
	}
	list_push_back(&frame_table, &frame->elem);
	return swap_in (page, frame->kva);
}

/* 새 보조 페이지 테이블을 초기화합니다. */
/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->hash_pages, page_hash, page_less, NULL);
}


/* src에서 dst로 보조 페이지 테이블을 복사합니다. */
/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	/* TODO: src의 table을 탐색하면서 table entry를 정확하게 dst 테이블에 만들기 
	 * TODO: 초기화되지 않은 (uninit) 페이지를 할당하고 그것들을 바로 요청하기 */

	// src table 탐색하기(해시 테이블 탐색)
	// @todo: hash_apply, 복사 기능 함수
	// 먼저 hash iterator 사용해서 복사 제대로 되는지 동작 확인하고 복사하는 부분 apply에 넣을 액션 함수로 빼기
	
	struct page *src_page;
	struct page *dst_page;

	struct hash_iterator i;
	hash_first (&i, &src->hash_pages);
	while (hash_next (&i)) {
		src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		
		if (src_page==NULL) return false;
		//src_page->Operations가 anon or unint
		enum vm_type type = src_page->operations->type;
		void *va = src_page->va;

		// page type 검사
		switch (type){
			case VM_UNINIT:
				//aux 메모리 할당
				void *aux = malloc(sizeof(struct aux));
				//parent aux 복사
				memcpy(aux, src_page->uninit.aux, sizeof(struct aux));
				//page 할당 후 끝
				if (!vm_alloc_page_with_initializer(src_page->uninit.type, va, src_page->is_writable, 
				src_page->uninit.init, aux)) return false;
				break;
			case VM_ANON:
				//페이지만 할당
				if (!vm_alloc_page(type, va, src_page->is_writable)) return false;
				//Frame 복사
				if (dst_page = spt_find_page(dst, va)){
					if (!vm_do_claim_page(dst_page)) return false;
					memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
				}
				break;
			default:
				break;
		}
	}
	return true;
}

/* 보조 페이지 테이블이 보유한 리소스를 해제합니다. */
/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: 스레드가 보유한 모든 보조 페이지 테이블을 파괴하고, 수정된 내용을 저장소에 씁니다. */
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	
	hash_clear(&spt->hash_pages, page_destructor);
}

/* hash table 함수 */

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}

void
page_destructor (struct hash_elem *page_elem, void *aux UNUSED){

	if (page_elem == NULL) return;

	struct page *p = hash_entry(page_elem, struct page, hash_elem);

	if (p == NULL) {
		// printf("wtf\n");
		return;
	}

	//@todo: 동적으로 할당받은게 뭐가 있는지 조사 후 추가
	vm_dealloc_page(p);
}