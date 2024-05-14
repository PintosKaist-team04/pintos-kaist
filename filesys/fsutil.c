#include "filesys/fsutil.h"
#include <debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/disk.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

/* 루트 디렉터리의 파일 목록을 나열합니다. */
/* List files in the root directory. */
void
fsutil_ls (char **argv UNUSED) {
	struct dir *dir;
	char name[NAME_MAX + 1];

	printf ("루트 디렉터리의 파일:\n");
	printf ("Files in the root directory:\n");
	dir = dir_open_root ();
	if (dir == NULL)
		PANIC ("루트 디렉터리 열기 실패");
	if (dir == NULL)
		PANIC ("root dir open failed");
	while (dir_readdir (dir, name))
		printf ("%s\n", name);
	printf ("목록의 끝.\n");
	printf ("End of listing.\n");
}

/* 파일 ARGV[1]의 내용을 시스템 콘솔에
 * 16진수와 ASCII로 출력합니다. */
/* Prints the contents of file ARGV[1] to the system console as
 * hex and ASCII. */
void
fsutil_cat (char **argv) {
	const char *file_name = argv[1];

	struct file *file;
	char *buffer;

	printf ("'%s'를 콘솔에 출력합니다...\n", file_name);
	printf ("Printing '%s' to the console...\n", file_name);
	file = filesys_open (file_name);
	if (file == NULL)
		PANIC ("%s: 열기 실패", file_name);
	PANIC ("%s: open failed", file_name);
	buffer = palloc_get_page (PAL_ASSERT);
	for (;;) {
		off_t pos = file_tell (file);
		off_t n = file_read (file, buffer, PGSIZE);
		if (n == 0)
			break;

		hex_dump (pos, buffer, n, true); 
	}
	palloc_free_page (buffer);
	file_close (file);
}

/* 파일 ARGV[1]을 삭제합니다. */
/* Deletes file ARGV[1]. */
void
fsutil_rm (char **argv) {
	const char *file_name = argv[1];

	printf ("'%s'를 삭제합니다...\n", file_name);
	printf ("Deleting '%s'...\n", file_name);
	if (!filesys_remove (file_name))
		PANIC ("%s: 삭제 실패\n", file_name);
}

/* "scratch" 디스크, hdc 또는 hd1:0에서 파일 ARGV[1]로 복사하여
 * 파일 시스템에 저장합니다.
 *
 * 스크래치 디스크의 현재 섹터는
 * "PUT\0"으로 시작하는 문자열 뒤에
 * 파일 크기(바이트)를 나타내는 32비트 리틀 엔디안 정수가 옵니다.
 * 그 다음 섹터는 파일 내용을 저장합니다.
 *
 * 이 함수의 첫 호출은 스크래치 디스크의 시작부터 읽습니다.
 * 이후의 호출에서는 디스크를 따라 진행합니다.
 * 이 디스크 위치는 fsutil_get()에서 사용하는 것과 독립적입니다.
 * 따라서 모든 'put'은 모든 'get'보다 앞에 와야 합니다. */
/* Copies from the "scratch" disk, hdc or hd1:0 to file ARGV[1]
 * in the file system.
 *
 * The current sector on the scratch disk must begin with the
 * string "PUT\0" followed by a 32-bit little-endian integer
 * indicating the file size in bytes.  Subsequent sectors hold
 * the file content.
 *
 * The first call to this function will read starting at the
 * beginning of the scratch disk.  Later calls advance across the
 * disk.  This disk position is independent of that used for
 * fsutil_get(), so all `put's should precede all `get's. */
void
fsutil_put (char **argv) {
	static disk_sector_t sector = 0;

	const char *file_name = argv[1];
	struct disk *src;
	struct file *dst;
	off_t size;
	void *buffer;

	printf ("'%s'를 파일 시스템에 넣습니다...\n", file_name);
	printf ("Putting '%s' into the file system...\n", file_name);

	/* Allocate buffer. */
	buffer = malloc (DISK_SECTOR_SIZE);
	if (buffer == NULL)
		PANIC ("버퍼 할당 실패");

	/* Open source disk and read file size. */
	src = disk_get (1, 0);
	if (src == NULL)
		PANIC ("소스 디스크 열기 실패 (hdc 또는 hd1:0)");

	/* Read file size. */
	disk_read (src, sector++, buffer);
	if (memcmp (buffer, "PUT", 4))
		PANIC ("%s: 스크래치 디스크에서 PUT 시그니처 누락", file_name);
	size = ((int32_t *) buffer)[1];
	if (size < 0)
		PANIC ("%s: 유효하지 않은 파일 크기 %d", file_name, size);

	/* Create destination file. */
	if (!filesys_create (file_name, size))
		PANIC ("%s: 생성 실패", file_name);
	dst = filesys_open (file_name);
	if (dst == NULL)
		PANIC ("%s: 열기 실패", file_name);

	/* Do copy. */
	while (size > 0) {
		int chunk_size = size > DISK_SECTOR_SIZE ? DISK_SECTOR_SIZE : size;
		disk_read (src, sector++, buffer);
		if (file_write (dst, buffer, chunk_size) != chunk_size)
			PANIC ("%s: %"PROTd" 바이트 쓰기 실패",
					file_name, size);
		size -= chunk_size;
	}

	/* Finish up. */
	file_close (dst);
	free (buffer);
}


/* 파일 시스템에서 파일 FILE_NAME을 스크래치 디스크로 복사합니다.
 *
 * 스크래치 디스크의 현재 섹터는
 * "GET\0" 다음에 파일 크기(바이트)가 32비트 리틀 엔디안 정수로 기록됩니다.
 * 이후의 섹터에는 파일 데이터가 저장됩니다.
 *
 * 이 함수의 첫 호출은 스크래치 디스크의 시작부터 쓰기를 시작합니다.
 * 이후의 호출에서는 디스크를 따라 진행합니다.
 * 이 디스크 위치는 fsutil_put()에서 사용하는 것과 독립적입니다.
 * 따라서 모든 'put'은 모든 'get'보다 앞에 와야 합니다. */
/* Copies file FILE_NAME from the file system to the scratch disk.
 *
 * The current sector on the scratch disk will receive "GET\0"
 * followed by the file's size in bytes as a 32-bit,
 * little-endian integer.  Subsequent sectors receive the file's
 * data.
 *
 * The first call to this function will write starting at the
 * beginning of the scratch disk.  Later calls advance across the
 * disk.  This disk position is independent of that used for
 * fsutil_put(), so all `put's should precede all `get's. */
void
fsutil_get (char **argv) {
	static disk_sector_t sector = 0;

	const char *file_name = argv[1];
	void *buffer;
	struct file *src;
	struct disk *dst;
	off_t size;

	printf ("'%s'를 파일 시스템에서 가져옵니다...\n", file_name);
	printf ("Getting '%s' from the file system...\n", file_name);

	/* Allocate buffer. */
	buffer = malloc (DISK_SECTOR_SIZE);
	if (buffer == NULL)
		PANIC ("버퍼 할당 실패");

	/* Open source file. */
	src = filesys_open (file_name);
	if (src == NULL)
		PANIC ("%s: 열기 실패", file_name);
	size = file_length (src);

	/* Open target disk. */
	dst = disk_get (1, 0);
	if (dst == NULL)
		PANIC ("대상 디스크 열기 실패 (hdc 또는 hd1:0)");

	/* Write size to sector 0. */
	memset (buffer, 0, DISK_SECTOR_SIZE);
	memcpy (buffer, "GET", 4);
	((int32_t *) buffer)[1] = size;
	disk_write (dst, sector++, buffer);

	/* Do copy. */
	while (size > 0) {
		int chunk_size = size > DISK_SECTOR_SIZE ? DISK_SECTOR_SIZE : size;
		if (sector >= disk_size (dst))
			PANIC ("%s: 스크래치 디스크 공간 부족", file_name);
		if (file_read (src, buffer, chunk_size) != chunk_size)
			PANIC ("%s: 읽기 실패 %"PROTd" 바이트 읽힘", file_name, size);
		memset (buffer + chunk_size, 0, DISK_SECTOR_SIZE - chunk_size);
		disk_write (dst, sector++, buffer);
		size -= chunk_size;
	}

	/* Finish up. */
	file_close (src);
	free (buffer);
}

