#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"

struct page;
enum vm_type;

typedef bool vm_initializer (struct page *, void *aux);

/* 초기화되지 않은 페이지. "지연 로딩"을 구현하기 위한 타입입니다. */
/* Uninitlialized page. The type for implementing the
 * "Lazy loading". */
struct uninit_page {
	/* 페이지의 내용을 초기화하다 */
	/* Initiate the contets of the page */
	vm_initializer *init;
	enum vm_type type;
	void *aux;
	/* struct page를 초기화하고 pa를 va에 매핑합니다 */
	/* Initiate the struct page and maps the pa to the va */
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};

void uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *kva));
#endif
