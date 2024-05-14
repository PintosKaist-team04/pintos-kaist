#include "filesys/free-map.h"
#include <bitmap.h>
#include <debug.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
static struct file *free_map_file;   /* 프리 맵 파일. / Free map file */
static struct bitmap *free_map;      /* 프리 맵, 디스크 섹터당 하나의 비트. / Free map, one bit per disk sector. */

/* 프리 맵을 초기화합니다. */
/* Initializes the free map. */
void
free_map_init (void) {
	free_map = bitmap_create (disk_size (filesys_disk));
	if (free_map == NULL)
		PANIC ("비트맵 생성 실패--디스크가 너무 큼");
	/* PANIC ("bitmap creation failed--disk is too large"); */
	bitmap_mark (free_map, FREE_MAP_SECTOR);
	bitmap_mark (free_map, ROOT_DIR_SECTOR);
}

/* 프리 맵에서 CNT개의 연속된 섹터를 할당하고
 * 첫 번째 섹터를 *SECTORP에 저장합니다.
 * 성공하면 true를 반환하고, 사용 가능한 모든 섹터가 없으면 false를 반환합니다. */
/* Allocates CNT consecutive sectors from the free map and stores
 * the first into *SECTORP.
 * Returns true if successful, false if all sectors were
 * available. */
bool
free_map_allocate (size_t cnt, disk_sector_t *sectorp) {
	disk_sector_t sector = bitmap_scan_and_flip (free_map, 0, cnt, false);
	if (sector != BITMAP_ERROR
			&& free_map_file != NULL
			&& !bitmap_write (free_map, free_map_file)) {
		bitmap_set_multiple (free_map, sector, cnt, false);
		sector = BITMAP_ERROR;
	}
	if (sector != BITMAP_ERROR)
		*sectorp = sector;
	return sector != BITMAP_ERROR;
}

/* SECTOR부터 시작하는 CNT 섹터를 사용 가능하게 만듭니다. */
/* Makes CNT sectors starting at SECTOR available for use. */
void
free_map_release (disk_sector_t sector, size_t cnt) {
	ASSERT (bitmap_all (free_map, sector, cnt));
	bitmap_set_multiple (free_map, sector, cnt, false);
	bitmap_write (free_map, free_map_file);
}

/* 프리 맵 파일을 열고 디스크에서 읽습니다. */
/* Opens the free map file and reads it from disk. */
void
free_map_open (void) {
	free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
	if (free_map_file == NULL)
		PANIC ("프리 맵을 열 수 없음");
	/* PANIC ("can't open free map"); */
	if (!bitmap_read (free_map, free_map_file))
		PANIC ("프리 맵을 읽을 수 없음");
	/* PANIC ("can't read free map"); */
}

/* 프리 맵을 디스크에 쓰고 프리 맵 파일을 닫습니다. */
/* Writes the free map to disk and closes the free map file. */
void
free_map_close (void) {
	file_close (free_map_file);
}

/* 디스크에 새 프리 맵 파일을 생성하고 프리 맵을 씁니다. */
/* Creates a new free map file on disk and writes the free map to
 * it. */
void
free_map_create (void) {
	/* inode를 생성합니다. */
	/* Create inode. */
	if (!inode_create (FREE_MAP_SECTOR, bitmap_file_size (free_map)))
		PANIC ("프리 맵 생성 실패");
	/* PANIC ("free map creation failed"); */

	/* 비트맵을 파일에 씁니다. */
	/* Write bitmap to file. */
	free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
	if (free_map_file == NULL)
		PANIC ("프리 맵을 열 수 없음");
	/* PANIC ("can't open free map"); */
	if (!bitmap_write (free_map, free_map_file))
		PANIC ("프리 맵을 쓸 수 없음");
	/* PANIC ("can't write free map"); */
}
