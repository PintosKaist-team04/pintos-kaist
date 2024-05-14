#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"

/* 시스템 파일 아이노드의 섹터 */
/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* 빈 맵 파일 아이노드 섹터 / Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* 루트 디렉터리 파일 아이노드 섹터 / Root directory file inode sector. */

/* 파일 시스템에 사용되는 디스크 */
/* Disk used for file system. */
extern struct disk *filesys_disk;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);

#endif /* filesys/filesys.h */
