#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"


/* 파일 시스템을 포함하는 디스크. */
/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* 파일 시스템 모듈을 초기화합니다.
 * FORMAT이 true이면 파일 시스템을 다시 포맷합니다. */
/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();

	fat_open ();
#else
	/* 원래 파일 시스템 */
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* 파일 시스템 모듈을 종료하고, 기록되지 않은 데이터를 디스크에 씁니다. */
/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* 원래 파일 시스템 */
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* 주어진 INITIAL_SIZE로 NAME이라는 파일을 생성합니다.
 * 성공하면 true를 반환하고, 그렇지 않으면 false를 반환합니다.
 * NAME이라는 파일이 이미 존재하거나 내부 메모리 할당에 실패하면 실패합니다. */
/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) {
	disk_sector_t inode_sector = 0;
	struct dir *dir = dir_open_root ();
	bool success = (dir != NULL
			&& free_map_allocate (1, &inode_sector)
			&& inode_create (inode_sector, initial_size)
			&& dir_add (dir, name, inode_sector));
	if (!success && inode_sector != 0)
		free_map_release (inode_sector, 1);
	dir_close (dir);

	return success;
}

/* 주어진 NAME의 파일을 엽니다.
 * 성공하면 새 파일을 반환하고, 그렇지 않으면 null 포인터를 반환합니다.
 * NAME이라는 파일이 존재하지 않거나 내부 메모리 할당에 실패하면 실패합니다. */
/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name) {
	struct dir *dir = dir_open_root ();
	struct inode *inode = NULL;

	if (dir != NULL)
		dir_lookup (dir, name, &inode);
	dir_close (dir);

	return file_open (inode);
}

/* NAME이라는 파일을 삭제합니다.
 * 성공하면 true를 반환하고, 실패하면 false를 반환합니다.
 * NAME이라는 파일이 존재하지 않거나 내부 메모리 할당에 실패하면 실패합니다. */
/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) {
	struct dir *dir = dir_open_root ();
	bool success = dir != NULL && dir_remove (dir, name);
	dir_close (dir);

	return success;
}

/* 파일 시스템을 포맷합니다. */
/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* FAT을 생성하고 디스크에 저장합니다. */
	/* Create FAT and save it to the disk. */
	fat_create ();
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}
