/* uninit.c: 초기화되지 않은 페이지의 구현.
 * 모든 페이지는 초기화되지 않은 페이지로 태어납니다. 첫 번째 페이지 오류가 발생하면
 * 핸들러 체인이 uninit_initialize (page->operations.swap_in)을 호출합니다.
 * uninit_initialize 함수는 페이지를 특정 페이지 객체 (anon, file, page_cache)로 변환시키고,
 * 페이지 객체를 초기화하고 vm_alloc_page_with_initializer 함수에서 전달된 초기화 콜백을 호출합니다.
 */
/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* 이 구조체를 수정하지 마세요. */
/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* 이 함수를 수정하지 마세요. */
/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */ /* 현재는 프레임이 없습니다. */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* 첫 번째 오류 발생 시 페이지를 초기화합니다. */
/* Initalize the page on first fault */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;

	/* 먼저 가져오고, page_initialize 함수가 값을 덮어쓸 수 있습니다. */
	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: 이 함수를 수정해야 할 수도 있습니다. */
	/* TODO: You may need to fix this function. */
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* uninit_page가 보유한 리소스를 해제합니다. 대부분의 페이지는 다른 페이지 객체로 변환되지만, 
 * 프로세스가 종료될 때 실행 중에 참조되지 않는 uninit 페이지가 있을 수 있습니다. 
 * 호출자가 PAGE를 해제합니다. */
/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	//@todo: pml4 clear page 안함?
	free(uninit->aux);
	return;
}
