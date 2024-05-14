/* inspect.c: VM을 위한 테스팅 유틸리티. */
/* 이 파일을 수정하지 마세요. */
/* inspect.c: Testing utility for VM. */
/* DO NOT MODIFY THIS FILE. */

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "vm/inspect.h"

static void
inspect (struct intr_frame *f) {
	const void *va = (const void *) f->R.rax;
	f->R.rax = PTE_ADDR (pml4_get_page (thread_current ()->pml4, va));
}

/* VM 구성 요소를 테스트하기 위한 도구. 이 함수를 int 0x42를 통해 호출합니다.
 * 입력:
 *   @RAX - 조사할 가상 주소
 * 출력:
 *   @RAX - 입력에 매핑된 물리 주소. */
/* Tool for testing vm component. Calling this function via int 0x42.
 * Input:
 *   @RAX - Virtual address to inspect
 * Output:
 *   @RAX - Physical address that mmaped to input. */
void
register_inspect_intr (void) {
	intr_register_int (0x42, 3, INTR_OFF, inspect, "Inspect Virtual Memory");
}
