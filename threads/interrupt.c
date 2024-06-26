#include "threads/interrupt.h"

#include <debug.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "devices/timer.h"
#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/intr-stubs.h"
#include "threads/io.h"
#include "threads/mmu.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/gdt.h"
#endif

/* x86_64 인터럽트의 수입니다. */
/* Number of x86_64 interrupts. */
#define INTR_CNT 256

/* FUNCTION을 호출하는 게이트를 생성합니다.

   게이트는 설명자 권한 수준 DPL을 가지며, 이는 프로세서가 DPL 또는
   그보다 낮은 숫자의 링에서 의도적으로 게이트를 호출할 수 있음을 의미합니다.
   실제로, DPL==3은 사용자 모드에서 게이트를 호출할 수 있도록 하고,
   DPL==0은 그러한 호출을 방지합니다. 사용자 모드에서 발생한 장애나 예외도
   DPL==0인 게이트의 호출을 유발합니다.

   TYPE은 인터럽트 게이트의 경우 14, 트랩 게이트의 경우 15여야 합니다.
   차이점은 인터럽트 게이트 진입 시 인터럽트가 비활성화되지만,
   트랩 게이트 진입 시에는 그렇지 않다는 것입니다.
   [IA32-v3a] 섹션 5.12.1.2 "예외 또는 인터럽트 처리자 절차에 의한 플래그 사용"에서
   논의되고 있습니다. */
/* Creates an gate that invokes FUNCTION.

   The gate has descriptor privilege level DPL, meaning that it
   can be invoked intentionally when the processor is in the DPL
   or lower-numbered ring.  In practice, DPL==3 allows user mode
   to call into the gate and DPL==0 prevents such calls.  Faults
   and exceptions that occur in user mode still cause gates with
   DPL==0 to be invoked.

   TYPE must be either 14 (for an interrupt gate) or 15 (for a
   trap gate).  The difference is that entering an interrupt gate
   disables interrupts, but entering a trap gate does not.  See
   [IA32-v3a] section 5.12.1.2 "Flag Usage By Exception- or
   Interrupt-Handler Procedure" for discussion. */

struct gate {
    unsigned off_15_0 : 16;   // 세그먼트 내의 오프셋 하위 16비트// low 16 bits of offset in segment
    unsigned ss : 16;         // 세그먼트 선택자// segment selector
    unsigned ist : 3;         // 인터럽트/트랩 게이트의 경우 0// # args, 0 for interrupt/trap gates
    unsigned rsv1 : 5;        // 예약됨(0이어야함) // reserved(should be zero I guess)
    unsigned type : 4;        // 타입 (STS_(TG,IG32,TG32)) // type(STS_{TG,IG32,TG32})
    unsigned s : 1;           // 시스템용, 0이어야 함 // must be 0 (system)
    unsigned dpl : 2;         // 디스크립터 권한 수준 // descriptor(meaning new) privilege level
    unsigned p : 1;           // 존재 여부  // Present
    unsigned off_31_16 : 16;  // 세그먼트 내의 오프셋 사위 비트// high bits of offset in segment
    uint32_t off_32_63;       // 세그먼트 내의 오프셋 32-63 비트
    uint32_t rsv2;            // 예약됨
};

/* 인터럽트 디스크립터 테이블 (IDT). 포맷은 CPU에 의해 고정됩니다.
   [IA32-v3a] 섹션 5.10 "인터럽트 디스크립터 테이블 (IDT)",
   섹션 5.11 "IDT 디스크립터", 섹션 5.12.1.2 "예외 또는 인터럽트 처리자
   절차에 의한 플래그 사용"에서 확인할 수 있습니다. */
/* The Interrupt Descriptor Table (IDT).  The format is fixed by
   the CPU.  See [IA32-v3a] sections 5.10 "Interrupt Descriptor
   Table (IDT)", 5.11 "IDT Descriptors", 5.12.1.2 "Flag Usage By
   Exception- or Interrupt-Handler Procedure". */
static struct gate idt[INTR_CNT];

static struct desc_ptr idt_desc = {.size = sizeof(idt) - 1, .address = (uint64_t)idt};

/* 함수, 권한 수준(DPL), 타입을 지정하여 게이트를 생성하는 매크로.
   - 함수 주소와 DPL(권한 수준), 그리고 타입(인터럽트 게이트 또는 트랩 게이트)에 따라
     게이트 구조체를 설정합니다. */
#define make_gate(g, function, d, t)                                \
    {                                                               \
        ASSERT((function) != NULL);                                 \
        ASSERT((d) >= 0 && (d) <= 3);                               \
        \			 
	ASSERT((t) >= 0 && (t) <= 15);                                  \
        \				
	*(g) = (struct gate){                                           \
            .off_15_0 = (uint64_t)(function) & 0xffff,              \
            .ss = SEL_KCSEG,                                        \
            .ist = 0,                                               \
            .rsv1 = 0,                                              \
            .type = (t),                                            \
            .s = 0,                                                 \
            .dpl = (d),                                             \
            .p = 1,                                                 \
            .off_31_16 = ((uint64_t)(function) >> 16) & 0xffff,     \
            .off_32_63 = ((uint64_t)(function) >> 32) & 0xffffffff, \
            .rsv2 = 0,                                              \
        };                                                          \
    }

/* 지정된 DPL로 FUNCTION을 호출하는 인터럽트 게이트를 생성합니다.
   인터럽트 게이트는 인터럽트를 비활성화하고 특정 함수를 호출합니다. */
/* Creates an interrupt gate that invokes FUNCTION with the given DPL. */
#define make_intr_gate(g, function, dpl) make_gate((g), (function), (dpl), 14)

/* 지정된 DPL로 FUNCTION을 호출하는 트랩 게이트를 생성합니다.
   트랩 게이트는 인터럽트를 비활성화하지 않고 특정 함수를 호출합니다. */
/* Creates a trap gate that invokes FUNCTION with the given DPL. */
#define make_trap_gate(g, function, dpl) make_gate((g), (function), (dpl), 15)

/* 각 인터럽트에 대한 인터럽트 핸들러 함수 배열입니다. */
/* Interrupt handler functions for each interrupt. */
static intr_handler_func *intr_handlers[INTR_CNT];

/* 디버깅 목적으로 각 인터럽트의 이름을 저장하는 배열입니다. */
/* Names for each interrupt, for debugging purposes. */
static const char *intr_names[INTR_CNT];

/* 외부 인터럽트는 CPU 외부의 장치, 예를 들어 타이머와 같은 장치에서 생성됩니다.
   외부 인터럽트는 인터럽트가 비활성화된 상태에서 실행되므로 중첩되거나
   선점되지 않습니다. 외부 인터럽트의 핸들러는 수면 상태로 들어갈 수 없으며,
   인터럽트 반환 직전에 새 프로세스를 스케줄링하기 위해 intr_yield_on_return()을
   호출할 수 있습니다. */
/* External interrupts are those generated by devices outside the
   CPU, such as the timer.  External interrupts run with
   interrupts turned off, so they never nest, nor are they ever
   pre-empted.  Handlers for external interrupts also may not
   sleep, although they may invoke intr_yield_on_return() to
   request that a new process be scheduled just before the
   interrupt returns. */
static bool in_external_intr; /* 현재 외부 인터럽트를 처리 중인가? */                /* Are we processing an external interrupt? */
static bool yield_on_return; /* 인터럽트 반환 시에 프로세스 전환을 요청할 것인가? */ /* Should we yield on interrupt return? */

/* 프로그래머블 인터럽트 컨트롤러(Interrupt Controller) 도우미 함수들입니다. */
/* Programmable Interrupt Controller helpers. */
static void pic_init(void);                /* PIC 초기화 함수 */
static void pic_end_of_interrupt(int irq); /* 인터럽트 처리 완료를 PIC에 알리는 함수 */

/* 인터럽트 핸들러 함수입니다. */
/* Interrupt handlers. */
void intr_handler(struct intr_frame *args);

/* 현재 인터럽트 상태를 반환합니다. */
/* Returns the current interrupt status. */
enum intr_level intr_get_level(void) {
    uint64_t flags;

    /* 프로세서 스택에 플래그 레지스터를 푸시한 후, 스택에서 값을 `flags`에 팝합니다.
       [IA32-v2b] "PUSHF" 및 "POP" 그리고 [IA32-v3a] 5.8.1 "Maskable Hardware Interrupts의 마스킹"을 참조하세요. */
    /* Push the flags register on the processor stack, then pop the
       value off the stack into `flags'.  See [IA32-v2b] "PUSHF"
       and "POP" and [IA32-v3a] 5.8.1 "Masking Maskable Hardware
       Interrupts". */
    asm volatile("pushfq; popq %0" : "=g"(flags));

    return flags & FLAG_IF ? INTR_ON : INTR_OFF;
}

/* LEVEL에 지정된 대로 인터럽트를 활성화하거나 비활성화하고 이전 인터럽트 상태를 반환합니다. */
/* Enables or disables interrupts as specified by LEVEL and
   returns the previous interrupt status. */
enum intr_level intr_set_level(enum intr_level level) {
    return level == INTR_ON ? intr_enable() : intr_disable();
}

/* 인터럽트를 활성화하고 이전 인터럽트 상태를 반환합니다. */
/* Enables interrupts and returns the previous interrupt status. */
enum intr_level intr_enable(void) {
    enum intr_level old_level = intr_get_level();
    ASSERT(!intr_context());

    /* 인터럽트 플래그를 설정하여 인터럽트를 활성화합니다.

       [IA32-v2b] "STI" 및 [IA32-v3a] 5.8.1 "Maskable Hardware Interrupts의 마스킹"을 참조하세요. */

    /* Enable interrupts by setting the interrupt flag.

       See [IA32-v2b] "STI" and [IA32-v3a] 5.8.1 "Masking Maskable
       Hardware Interrupts". */
    asm volatile("sti");

    return old_level;
}

/* 인터럽트 비활성화하고 이전 인터럽트 상태를 반환합니다. */
/* Disables interrupts and returns the previous interrupt status. */
enum intr_level intr_disable(void) {
    enum intr_level old_level = intr_get_level();

    /* 인터럽트 플래그를 지워서 인터럽트를 비활성화합니다.
       [IA32-v2b] "CLI" 및 [IA32-v3a] 5.8.1 "Maskable Hardware Interrupts 마스킹" 참조. */

    /* Disable interrupts by clearing the interrupt flag.
       See [IA32-v2b] "CLI" and [IA32-v3a] 5.8.1 "Masking Maskable
       Hardware Interrupts". */
    asm volatile("cli" : : : "memory");

    return old_level;
}

/* 인터럽트 시스템을 초기화합니다. */
/* Initializes the interrupt system. */
void intr_init(void) {
    int i;

    /* 인터럽트 컨트롤러 초기화. */
    /* Initialize interrupt controller. */
    pic_init();

    /* Initialize IDT. */
    for (i = 0; i < INTR_CNT; i++) {
        make_intr_gate(&idt[i], intr_stubs[i], 0);  // 인터럽트 게이트 생성
        intr_names[i] = "unknown";                  // 기본 이름을 "unknown"으로 설정
    }

#ifdef USERPROG
    /* TSS(Task State Segment) 로드. */
    /* Load TSS. */
    ltr(SEL_TSS);
#endif
    /* IDT 레지스터를 로드합니다. */
    /* Load IDT register. */
    lidt(&idt_desc);

    /* intr_names 초기화. 각 인터럽트 핸들러에 대한 설명을 추가합니다. */
    /* Initialize intr_names. */
    intr_names[0] = "#DE Divide Error";
    intr_names[1] = "#DB Debug Exception";
    intr_names[2] = "NMI Interrupt";
    intr_names[3] = "#BP Breakpoint Exception";
    intr_names[4] = "#OF Overflow Exception";
    intr_names[5] = "#BR BOUND Range Exceeded Exception";
    intr_names[6] = "#UD Invalid Opcode Exception";
    intr_names[7] = "#NM Device Not Available Exception";
    intr_names[8] = "#DF Double Fault Exception";
    intr_names[9] = "Coprocessor Segment Overrun";
    intr_names[10] = "#TS Invalid TSS Exception";
    intr_names[11] = "#NP Segment Not Present";
    intr_names[12] = "#SS Stack Fault Exception";
    intr_names[13] = "#GP General Protection Exception";
    intr_names[14] = "#PF Page-Fault Exception";
    intr_names[16] = "#MF x87 FPU Floating-Point Error";
    intr_names[17] = "#AC Alignment Check Exception";
    intr_names[18] = "#MC Machine-Check Exception";
    intr_names[19] = "#XF SIMD Floating-Point Exception";
}

/* 인터럽트 VEC_NO를 HANDLER와 DPL 권한 수준으로 등록합니다.
   디버깅 목적으로 인터럽트의 이름은 NAME으로 지정됩니다.
   인터럽트 핸들러는 LEVEL로 설정된 인터럽트 상태로 호출됩니다. */
/* Registers interrupt VEC_NO to invoke HANDLER with descriptor
   privilege level DPL.  Names the interrupt NAME for debugging
   purposes.  The interrupt handler will be invoked with
   interrupt status set to LEVEL. */
static void register_handler(uint8_t vec_no, int dpl, enum intr_level level, intr_handler_func *handler, const char *name) {
    ASSERT(intr_handlers[vec_no] == NULL);
    if (level == INTR_ON) {
        make_trap_gate(&idt[vec_no], intr_stubs[vec_no], dpl);
    } else {
        make_intr_gate(&idt[vec_no], intr_stubs[vec_no], dpl);
    }
    intr_handlers[vec_no] = handler;
    intr_names[vec_no] = name;
}

/* 외부 인터럽트 VEC_NO를 HANDLER에 등록합니다.
   디버깅 목적으로 이름이 NAME인 핸들러는 인터럽트가 비활성화된 상태에서 실행됩니다. */
/* Registers external interrupt VEC_NO to invoke HANDLER, which
   is named NAME for debugging purposes.  The handler will
   execute with interrupts disabled. */
void intr_register_ext(uint8_t vec_no, intr_handler_func *handler, const char *name) {
    ASSERT(vec_no >= 0x20 && vec_no <= 0x2f);
    register_handler(vec_no, 0, INTR_OFF, handler, name);
}

/* 내부 인터럽트 VEC_NO를 HANDLER로 등록합니다.
   디버깅 목적으로 이름이 NAME인 핸들러는 인터럽트 상태 LEVEL로 호출됩니다.

   핸들러는 디스크립터 권한 수준 DPL을 갖습니다. 즉, 프로세서가 DPL 또는 더 낮은 번호의 링에 있을 때 의도적으로 호출될 수 있습니다.
   실제로 DPL==3이면 사용자 모드에서 인터럽트를 호출할 수 있으며, DPL==0은 이러한 호출을 방지합니다.
   사용자 모드에서 발생하는 오류와 예외는 여전히 DPL==0의 인터럽트를 호출합니다.
   더 자세한 토론은 [IA32-v3a] 섹션 4.5 "Privilege Levels" 및 4.8.1.1 "Accessing Nonconforming Code Segments"를 참조하세요. */
/* Registers internal interrupt VEC_NO to invoke HANDLER, which
   is named NAME for debugging purposes.  The interrupt handler
   will be invoked with interrupt status LEVEL.

   The handler will have descriptor privilege level DPL, meaning
   that it can be invoked intentionally when the processor is in
   the DPL or lower-numbered ring.  In practice, DPL==3 allows
   user mode to invoke the interrupts and DPL==0 prevents such
   invocation.  Faults and exceptions that occur in user mode
   still cause interrupts with DPL==0 to be invoked.  See
   [IA32-v3a] sections 4.5 "Privilege Levels" and 4.8.1.1
   "Accessing Nonconforming Code Segments" for further
   discussion. */
void intr_register_int(uint8_t vec_no, int dpl, enum intr_level level, intr_handler_func *handler, const char *name) {
    ASSERT(vec_no < 0x20 || vec_no > 0x2f);
    register_handler(vec_no, dpl, level, handler, name);
}

/* 외부 인터럽트 처리 중에는 true를 반환하고, 그 외의 경우에는 false를 반환합니다. */
/* Returns true during processing of an external interrupt
   and false at all other times. */
bool intr_context(void) {
    return in_external_intr;
}

/* 외부 인터럽트 처리 중에는 인터럽트 핸들러가 반환하기 직전에 새 프로세스로 양보하도록 지시합니다.
   다른 시간에는 호출할 수 없습니다. */
/* During processing of an external interrupt, directs the
   interrupt handler to yield to a new process just before
   returning from the interrupt.  May not be called at any other
   time. */
void intr_yield_on_return(void) {
    ASSERT(intr_context());
    yield_on_return = true;
}

/* 8259A 프로그래머블 인터럽트 컨트롤러. */
/* 8259A Programmable Interrupt Controller. */

/* 모든 PC에는 두 개의 8259A 프로그래머블 인터럽트 컨트롤러 (PIC) 칩이 있습니다.
   하나는 "마스터"로서 0x20 및 0x21 포트에서 접근할 수 있습니다.
   다른 하나는 마스터의 IRQ 2 라인에 연결된 "슬레이브"로서 0xa0 및 0xa1 포트에서 접근할 수 있습니다.
   포트 0x20으로의 접근은 A0 라인을 0으로 설정하고, 0x21로의 접근은 A1 라인을 1로 설정합니다.
   슬레이브 PIC에 대해서도 비슷한 상황이 적용됩니다.

   기본적으로 PIC가 전달하는 0...15 번의 인터럽트는 인터럽트 벡터 0...15로 이동합니다.
   불행히도, 해당 벡터는 CPU 트랩 및 예외에도 사용됩니다. 따라서 PIC를 다시 프로그래밍하여 0...15의 인터럽트가
   대신 32...47 (0x20...0x2f)의 인터럽트 벡터로 전달되도록 합니다. */
/* Every PC has two 8259A Programmable Interrupt Controller (PIC)
   chips.  One is a "master" accessible at ports 0x20 and 0x21.
   The other is a "slave" cascaded onto the master's IRQ 2 line
   and accessible at ports 0xa0 and 0xa1.  Accesses to port 0x20
   set the A0 line to 0 and accesses to 0x21 set the A1 line to
   1.  The situation is similar for the slave PIC.

   By default, interrupts 0...15 delivered by the PICs will go to
   interrupt vectors 0...15.  Unfortunately, those vectors are
   also used for CPU traps and exceptions.  We reprogram the PICs
   so that interrupts 0...15 are delivered to interrupt vectors
   32...47 (0x20...0x2f) instead. */

/* 아래 코드는 8259A 프로그래머블 인터럽트 컨트롤러(PIC)를 초기화하는 함수입니다.
    이 함수는 각각의 PIC 칩에 대한 설정을 변경하여 인터럽트가 CPU 트랩이나 예외와 충돌하지 않도록 합니다.
    PIC를 초기화하면 0...15의 인터럽트가 32...47의 인터럽트 벡터로 이동하게 됩니다.
 */

/* PIC를 초기화합니다. 자세한 내용은 [8259A]를 참조하세요. */
/* Initializes the PICs.  Refer to [8259A] for details. */
static void pic_init(void) {
    /* 두 PIC에서 모든 인터럽트를 마스크합니다. */
    /* Mask all interrupts on both PICs. */
    outb(0x21, 0xff);
    outb(0xa1, 0xff);

    /* Initialize master. */
    outb(0x20, 0x11); /* ICW1: single mode, edge triggered, expect ICW4. */
    outb(0x21, 0x20); /* ICW2: line IR0...7 -> irq 0x20...0x27. */
    outb(0x21, 0x04); /* ICW3: slave PIC on line IR2. */
    outb(0x21, 0x01); /* ICW4: 8086 mode, normal EOI, non-buffered. */

    /* Initialize slave. */
    outb(0xa0, 0x11); /* ICW1: single mode, edge triggered, expect ICW4. */
    outb(0xa1, 0x28); /* ICW2: line IR0...7 -> irq 0x28...0x2f. */
    outb(0xa1, 0x02); /* ICW3: slave ID is 2. */
    outb(0xa1, 0x01); /* ICW4: 8086 mode, normal EOI, non-buffered. */

    /* Unmask all interrupts. */
    outb(0x21, 0x00);
    outb(0xa1, 0x00);
}

/* 주어진 IRQ에 대한 인터럽트 종료 신호를 PIC에 보냅니다.
   IRQ를 인식하지 않으면 다시 전달되지 않기 때문에 이 작업은 중요합니다. */
/* Sends an end-of-interrupt signal to the PIC for the given IRQ.
   If we don't acknowledge the IRQ, it will never be delivered to
   us again, so this is important.  */
static void pic_end_of_interrupt(int irq) {
    ASSERT(irq >= 0x20 && irq < 0x30);

    /* 마스터 PIC를 인식합니다. */
    /* Acknowledge master PIC. */
    outb(0x20, 0x20);

    /* 이것이 슬레이브 인터럽트인 경우 슬레이브 PIC를 인식합니다. */
    /* Acknowledge slave PIC if this is a slave interrupt. */
    if (irq >= 0x28)
        outb(0xa0, 0x20);
}

/* 인터럽트 핸들러. */
/* Interrupt handlers. */

/* 모든 인터럽트, 트랩 및 예외에 대한 핸들러입니다.
   이 함수는 어셈블리 언어로 작성된 intr-stubs.S 파일의 인터럽트 스텁에 의해 호출됩니다.
   FRAME은 인터럽트 및 중단된 스레드의 레지스터를 설명합니다. */
/* Handler for all interrupts, faults, and exceptions.  This
   function is called by the assembly language interrupt stubs in
   intr-stubs.S.  FRAME describes the interrupt and the
   interrupted thread's registers. */
void intr_handler(struct intr_frame *frame) {
    bool external;
    intr_handler_func *handler;

    /* 외부 인터럽트는 특별합니다.
       한 번에 하나만 처리합니다 (따라서 인터럽트는 꺼져 있어야 함)
         그리고 PIC에서 인식되어야 합니다. (아래 참조)
         외부 인터럽트 핸들러는 슬립할 수 없습니다. */
    /* External interrupts are special.
       We only handle one at a time (so interrupts must be off)
       and they need to be acknowledged on the PIC (see below).
       An external interrupt handler cannot sleep. */
    external = frame->vec_no >= 0x20 && frame->vec_no < 0x30;
    if (external) {
        ASSERT(intr_get_level() == INTR_OFF);
        ASSERT(!intr_context());

        in_external_intr = true;
        yield_on_return = false;
    }

    /* 인터럽트의 핸들러를 호출합니다. */
    /* Invoke the interrupt's handler. */
    handler = intr_handlers[frame->vec_no];
    if (handler != NULL)
        handler(frame);
    else if (frame->vec_no == 0x27 || frame->vec_no == 0x2f) {
        /* 핸들러가 없지만 하드웨어 결함이나 하드웨어 경쟁 조건으로 인해
           이 인터럽트가 잘못으로 트리거될 수 있습니다. 무시합니다. */
        /* There is no handler, but this interrupt can trigger
           spuriously due to a hardware fault or hardware race
           condition.  Ignore it. */
    } else {
        /* 핸들러가 없고, 잘못된 트리거가 아닙니다. 예상치 않은 인터럽트 핸들러를 호출합니다. */
        /* No handler and not spurious.  Invoke the unexpected
           interrupt handler. */
        intr_dump_frame(frame);
        PANIC("Unexpected interrupt");
    }

    /* 외부 인터럽트 처리를 완료합니다. */
    /* Complete the processing of an external interrupt. */
    if (external) {
        ASSERT(intr_get_level() == INTR_OFF);
        ASSERT(intr_context());

        in_external_intr = false;
        pic_end_of_interrupt(frame->vec_no);

        if (yield_on_return)
            thread_yield();
    }
}

/* 디버깅을 위해 인터럽트 프레임 F를 콘솔에 덤프합니다. */
/* Dumps interrupt frame F to the console, for debugging. */
void intr_dump_frame(const struct intr_frame *f) {
    /* CR2는 마지막 페이지 폴트의 선형 주소입니다.
       [IA32-v2a] "MOV--제어 레지스터로부터/제어 레지스터로 이동" 및
       [IA32-v3a] 5.14 "인터럽트 14--페이지 폴트 예외 (#PF)"를 참조하십시오. */
    /* CR2 is the linear address of the last page fault.
       See [IA32-v2a] "MOV--Move to/from Control Registers" and
       [IA32-v3a] 5.14 "Interrupt 14--Page Fault Exception
       (#PF)". */
    uint64_t cr2 = rcr2();
    printf("Interrupt %#04llx (%s) at rip=%llx\n", f->vec_no, intr_names[f->vec_no], f->rip);
    printf(" cr2=%016llx error=%16llx\n", cr2, f->error_code);
    printf("rax %016llx rbx %016llx rcx %016llx rdx %016llx\n", f->R.rax, f->R.rbx, f->R.rcx, f->R.rdx);
    printf("rsp %016llx rbp %016llx rsi %016llx rdi %016llx\n", f->rsp, f->R.rbp, f->R.rsi, f->R.rdi);
    printf("rip %016llx r8 %016llx  r9 %016llx r10 %016llx\n", f->rip, f->R.r8, f->R.r9, f->R.r10);
    printf("r11 %016llx r12 %016llx r13 %016llx r14 %016llx\n", f->R.r11, f->R.r12, f->R.r13, f->R.r14);
    printf("r15 %016llx rflags %08llx\n", f->R.r15, f->eflags);
    printf("es: %04x ds: %04x cs: %04x ss: %04x\n", f->es, f->ds, f->cs, f->ss);
}

/* 인터럽트 VEC의 이름을 반환합니다. */
/* Returns the name of interrupt VEC. */
const char *intr_name(uint8_t vec) {
    return intr_names[vec];
}