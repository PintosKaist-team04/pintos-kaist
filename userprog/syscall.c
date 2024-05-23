#include "userprog/syscall.h"

#include <stdio.h>
#include <syscall-nr.h>

#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "intrinsic.h"
#include "lib/kernel/stdio.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/gdt.h"
#include "userprog/process.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *f UNUSED);
void check_address(void *uaddr);

void halt(void);
void exit(int status);
tid_t fork(const char *thread_name);
int exec(const char *file);
int wait(pid_t);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned length);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);


/* 시스템 호출.
 *
 * 이전에 시스템 호출 서비스는 인터럽트 핸들러에서 처리되었습니다
 * (예: 리눅스에서 int 0x80). 그러나 x86-64에서는 제조사가
 * 효율적인 시스템 호출 요청 경로를 제공합니다. 바로 `syscall` 명령입니다.
 *
 * syscall 명령은 Model Specific Register (MSR)에서 값을 읽어와서 동작합니다.
 * 자세한 내용은 메뉴얼을 참조하세요. */
/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */
// 파일 객체에 대한 파일 디스크립터를 생성하는 함수
static int process_add_file(struct file *f) {
    struct thread *curr = thread_current();
    struct file **fdt = curr->fdt;

    // limit을 넘지 않는 범위 안에서 빈 자리 탐색
    while (curr->next_fd < FDT_COUNT_LIMIT && fdt[curr->next_fd])
        curr->next_fd++;

    if (curr->next_fd >= FDT_COUNT_LIMIT)
        return -1;

    fdt[curr->next_fd] = f;

    return curr->next_fd;
}

// 파일 객체를 검색하는 함수
static struct file *process_get_file(int fd) {
    struct thread *curr = thread_current();
    struct file **fdt = curr->fdt;

    /* 파일 디스크립터에 해당하는 파일 객체를 리턴 */
    /* 없을 시 NULL 리턴 */
    if (fd < 2 || fd >= FDT_COUNT_LIMIT)
        return NULL;
    return fdt[fd];
}

// 파일 디스크립터 테이블에서 파일 객체를 제거하는 함수
static void process_close_file(int fd) {
    struct thread *curr = thread_current();
    struct file **fdt = curr->fdt;
    if (fd < 2 || fd >= FDT_COUNT_LIMIT)
        return NULL;

    fdt[fd] = NULL;
}


void syscall_init(void) {
    write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 | ((uint64_t)SEL_KCSEG) << 32);
    write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* 인터럽트 서비스 루틴은 syscall_entry가 유저랜드 스택을 커널
	 * 모드 스택으로 전환할 때까지 어떤 인터럽트도 처리해서는 안 됩니다.
	 * 따라서 FLAG_FL을 마스킹했습니다. */
    /* The interrupt service rountine should not serve any interrupts
     * until the syscall_entry swaps the userland stack to the kernel
     * mode stack. Therefore, we masked the FLAG_FL. */
    write_msr(MSR_SYSCALL_MASK, FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
    lock_init(&filesys_lock);
}

/* 주요 시스템 호출 인터페이스 */
/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED) {
    uint64_t syscall_n = f->R.rax;

    switch (syscall_n) {
        case SYS_HALT:
            halt();
            break;

        case SYS_EXIT:
            exit(f->R.rdi);
            break;

        case SYS_FORK:
            f->R.rax = fork(f->R.rdi);
            break;

        case SYS_EXEC:
            exec(f->R.rdi);
            break;

        case SYS_WAIT:
            f->R.rax = wait(f->R.rdi);
            break;

        case SYS_CREATE:
            f->R.rax = create(f->R.rdi, f->R.rsi);
            break;

        case SYS_REMOVE:
            f->R.rax = remove(f->R.rdi);
            break;

        case SYS_OPEN:
            f->R.rax = open(f->R.rdi);
            break;

        case SYS_FILESIZE:
            f->R.rax = filesize(f->R.rdi);
            break;

        case SYS_READ:
            f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
            break;

        case SYS_WRITE:
            f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
            break;

        case SYS_SEEK:
            seek(f->R.rdi, f->R.rsi);
            break;

        case SYS_TELL:
            f->R.rax = tell(f->R.rdi);
            break;

        case SYS_CLOSE:
            close(f->R.rdi);
            break;

        case SYS_MMAP:
            f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
            break;
        
        case SYS_MUNMAP:
            munmap(f->R.rdi);
            break;

        default:
            exit(-1);
            break;
    }
}

void check_address(void *uaddr) {
    struct thread *cur = thread_current();
    // @todo
    // if (uaddr == NULL || is_kernel_vaddr(uaddr) || pml4_get_page(cur->pml4, uaddr) == NULL) {
    //     exit(-1);
    // }
    if (uaddr == NULL || is_kernel_vaddr(uaddr)) {
        exit(-1);
    }
}

void halt(void) {
    power_off();
}

void exit(int status) {
    thread_current()->exit_status = status;
    printf("%s: exit(%d)\n", thread_name(), status);
    thread_exit();
}

tid_t fork(const char *thread_name) {
    check_address(thread_name);
    struct intr_frame *if_ = pg_round_up(&thread_name) - sizeof(struct intr_frame);
    
    return process_fork(thread_name, if_);
}

int exec(const char *file) {
    check_address(file);

    // process.c 파일의 process_create_initd 함수와 유사하다.
    // 단, 스레드를 새로 생성하는 건 fork에서 수행하므로
    // 이 함수에서는 새 스레드를 생성하지 않고 process_exec을 호출한다.

    // process_exec 함수 안에서 filename을 변경해야 하므로
    // 커널 메모리 공간에 cmd_line의 복사본을 만든다.
    // (현재는 const char* 형식이기 때문에 수정할 수 없다.)
    char *file_copy;
    file_copy = palloc_get_page(0);
    if (file_copy == NULL)
        exit(-1);                      // 메모리 할당 실패 시 status -1로 종료한다.
    strlcpy(file_copy, file, PGSIZE);  // cmd_line을 복사한다.

    // 스레드의 이름을 변경하지 않고 바로 실행한다.
    if (process_exec(file_copy) == -1)
        exit(-1);  // 실패 시 status -1로 종료한다.
}

int wait(pid_t) {
    return process_wait(pid_t);
}

bool create(const char *file, unsigned initial_size) {
    check_address(file);
    bool is_success = filesys_create(file, initial_size);

    return is_success;
}

bool remove(const char *file) {
    check_address(file);
    bool is_success = filesys_remove(file);
    return is_success;
}

int open(const char *file) {
    check_address(file);

    lock_acquire(&filesys_lock);
    struct file *f = filesys_open(file);

    if (f == NULL) {
        lock_release(&filesys_lock);
        return -1;
    }

    int fd = process_add_file(f);
    if (fd == -1)
        file_close(f);

    lock_release(&filesys_lock);
    return fd;
}

int filesize(int fd) {
    struct file *file = process_get_file(fd);
    if (file == NULL)
        return -1;

    return file_length(file);
}

int read(int fd, void *buffer, unsigned size) {
    //@fixme : 버퍼가 쓰기 가능인지 체크하기
    check_address(buffer);

    struct page *bf_page = spt_find_page(&thread_current()->spt, pg_round_down(buffer));
    if (!bf_page->is_writable) {
        exit(-1);
    }
       
    char *ptr = (char *)buffer;
    int bytes_read = 0;

    lock_acquire(&filesys_lock);
    if (fd == STDIN_FILENO) {
        for (int i = 0; i < size; i++) {
            *ptr++ = input_getc();
            bytes_read++;
        }
        lock_release(&filesys_lock);
    } else {
        struct file *file = process_get_file(fd);
        if (file == NULL) {
            lock_release(&filesys_lock);
            return -1;
        }
        bytes_read = file_read(file, buffer, size);
        lock_release(&filesys_lock);
    }
    return bytes_read;
}

int write(int fd, const void *buffer, unsigned length) {
    check_address(buffer);

    char *ptr = (char *)buffer;
    int bytes_write = 0;

    lock_acquire(&filesys_lock);
    if (fd == STDOUT_FILENO) {
        putbuf(buffer, length);
        lock_release(&filesys_lock);
    }

    else {
        struct file *file = process_get_file(fd);
        if (file == NULL) {
            lock_release(&filesys_lock);
            return -1;
        }
        bytes_write = file_write(file, buffer, length);
        lock_release(&filesys_lock);
    }
    return bytes_write;
}

void seek(int fd, unsigned position) {
    struct file *file = process_get_file(fd);
    if (file == NULL)
        return;

    file_seek(file, position);
}

unsigned tell(int fd) {
    struct file *file = process_get_file(fd);
    if (file == NULL)
        return;

    return file_tell(file);
}

void close(int fd) {
    struct file *file = process_get_file(fd);
    if (file == NULL)
        return;
    file_close(file);
    process_close_file(fd);
}

void *mmap (void *addr, size_t length, int writable, int fd, off_t offset) {
    /* 실패 case 
     * 1. addr이 0 인 경우 null인 경우는 호출까지만(?) << 리눅스는 넣을수 있는데를 알아서 찾는데 우리 핀토스는 걍 실패처리
     * 2. addr+length가 커널 영역을 침범하는 경우 
     * 3. addr과 offset이 pgsize가 아닌 경우
     * 4. fd가 없는 경우
     * 5. length 혹은 file size가 0 인 경우
     * 6. offset이 파일 크기보다 큰 경우
     * 7. fd 값이 표준 입출력(0, 1) 인 경우
     * 8. 기존에 mmap 된 가상 주소인 경우(spt find)
     */

    // check_address(addr);
    // check_address(addr + length);

    // 주소 유효성 검사. 커널 영역을 침범했는가? 페이지 단위의 주소인가. @todo: addr + length 고민
    if (addr == NULL || is_kernel_vaddr(addr) || is_kernel_vaddr(pg_round_up(addr + length)) || pg_ofs(addr)) {
        return NULL;
    }

    // int a = (uint64_t)addr & (PGSIZE - 1); // @todo 삭제할것 for testing
    // 길이가 있는지
    if (length <= 0) {
		return NULL;
	}


    // 이미 사용중인 주소였는지/페이지 였는지 (겹치지 않게)
    if(spt_find_page(&thread_current()->spt, addr)) {
        return NULL;
    }
    
    // 입출력/오류 파일이 아닌지
    if (fd <= STDIN_FILENO || fd == STDOUT_FILENO) {
        return NULL;
    }

    struct file *file = process_get_file(fd);
    if (file == NULL) {
        return NULL;
    }

    int fsize = filesize(fd);

    if (fsize <= 0 || fsize <= offset) {
        return NULL;
    }
    
    if (fsize < length) length = fsize;

    file = file_reopen(file);
	if (file == NULL) {
		return NULL;
	}
	
    return do_mmap(addr, length, writable, file, offset);
}

void munmap (void *addr) {
    printf("not yet\n");
    return NULL;
}
