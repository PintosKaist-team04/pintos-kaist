#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* inode를 식별합니다. */
/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* 디스크 상의 inode.
 * 반드시 DISK_SECTOR_SIZE 바이트 길이여야 합니다. */
/* On-disk inode.
 * Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk {
	disk_sector_t start;                /* 첫 번째 데이터 섹터. */
	/* First data sector. */
	off_t length;                       /* 파일 크기 (바이트 단위). */
	/* File size in bytes. */
	unsigned magic;                     /* 매직 넘버. */
	/* Magic number. */
	uint32_t unused[125];               /* 사용되지 않음. */
	/* Not used. */
};

/* SIZE 바이트 길이의 inode를 할당하기 위해 필요한 섹터 수를 반환합니다. */
/* Returns the number of sectors to allocate for an inode SIZE
 * bytes long. */
static inline size_t
bytes_to_sectors (off_t size) {
	return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* 메모리 상의 inode. */
/* In-memory inode. */
struct inode {
	struct list_elem elem;              /* inode 리스트의 요소. */
	/* Element in inode list. */
	disk_sector_t sector;               /* 디스크 위치의 섹터 번호. */
	/* Sector number of disk location. */
	int open_cnt;                       /* 열려 있는 개수. */
	/* Number of openers. */
	bool removed;                       /* 삭제되었으면 true, 그렇지 않으면 false. */
	/* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: 쓰기 가능, >0: 쓰기 금지. */
	/* 0: writes ok, >0: deny writes. */
	struct inode_disk data;             /* inode 내용. */
	/* Inode content. */
};

/* INODE 내에서 POS 바이트 오프셋을 포함하는 디스크 섹터를 반환합니다.
 * INODE에 해당 오프셋의 데이터가 없으면 -1을 반환합니다. */
/* Returns the disk sector that contains byte offset POS within
 * INODE.
 * Returns -1 if INODE does not contain data for a byte at offset
 * POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) {
	ASSERT (inode != NULL);
	if (pos < inode->data.length)
		return inode->data.start + pos / DISK_SECTOR_SIZE;
	else
		return -1;
}

/* 동일한 `struct inode'를 두 번 열 때 동일한 inode를 반환하기 위해
 * 열려 있는 inode의 리스트를 유지합니다. */
/* List of open inodes, so that opening a single inode twice
 * returns the same `struct inode'. */
static struct list open_inodes;

/* inode 모듈을 초기화합니다. */
/* Initializes the inode module. */
void
inode_init (void) {
	list_init (&open_inodes);
}

/* LENGTH 바이트 길이의 데이터로 inode를 초기화하고
 * 새로운 inode를 파일 시스템 디스크의 SECTOR에 씁니다.
 * 성공하면 true를 반환합니다.
 * 메모리 또는 디스크 할당에 실패하면 false를 반환합니다. */
/* Initializes an inode with LENGTH bytes of data and
 * writes the new inode to sector SECTOR on the file system
 * disk.
 * Returns true if successful.
 * Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length) {
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT (length >= 0);

	/* 이 어설션이 실패하면, inode 구조체가 정확히
	 * 한 섹터의 크기가 아니므로 수정해야 합니다. */
	/* If this assertion fails, the inode structure is not exactly
	 * one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);
	if (disk_inode != NULL) {
		size_t sectors = bytes_to_sectors (length);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		if (free_map_allocate (sectors, &disk_inode->start)) {
			disk_write (filesys_disk, sector, disk_inode);
			if (sectors > 0) {
				static char zeros[DISK_SECTOR_SIZE];
				size_t i;

				for (i = 0; i < sectors; i++) 
					disk_write (filesys_disk, disk_inode->start + i, zeros); 
			}
			success = true; 
		} 
		free (disk_inode);
	}
	return success;
}

/* SECTOR에서 inode를 읽어와서
 * 이를 포함하는 `struct inode'를 반환합니다.
 * 메모리 할당에 실패하면 null 포인터를 반환합니다. */
/* Reads an inode from SECTOR
 * and returns a `struct inode' that contains it.
 * Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) {
	struct list_elem *e;
	struct inode *inode;

	/* 이 inode가 이미 열려 있는지 확인합니다. */
	/* Check whether this inode is already open. */
	for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
			e = list_next (e)) {
		inode = list_entry (e, struct inode, elem);
		if (inode->sector == sector) {
			inode_reopen (inode);
			return inode; 
		}
	}

	/* 메모리 할당. */
	/* Allocate memory. */
	inode = malloc (sizeof *inode);
	if (inode == NULL)
		return NULL;

	/* 초기화. */
	/* Initialize. */
	list_push_front (&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	disk_read (filesys_disk, inode->sector, &inode->data);
	return inode;
}

/* INODE를 다시 열고 반환합니다. */
/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode) {
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/* INODE의 inode 번호를 반환합니다. */
/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode) {
	return inode->sector;
}

/* INODE를 닫고 디스크에 씁니다.
 * 만약 이것이 INODE에 대한 마지막 참조였다면, 메모리를 해제합니다.
 * 만약 INODE가 제거된 inode였다면, 블록을 해제합니다. */
/* Closes INODE and writes it to disk.
 * If this was the last reference to INODE, frees its memory.
 * If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) {
	/* null 포인터를 무시합니다. */
	/* Ignore null pointer. */
	if (inode == NULL)
		return;

	/* 마지막 오픈한 사람이었던 경우 자원을 해제합니다. */
	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0) {
		/* inode 리스트에서 제거하고 잠금을 해제합니다. */
		/* Remove from inode list and release lock. */
		list_remove (&inode->elem);

		/* 제거된 경우 블록을 해제합니다. */
		/* Deallocate blocks if removed. */
		if (inode->removed) {
			free_map_release (inode->sector, 1);
			free_map_release (inode->data.start,
					bytes_to_sectors (inode->data.length)); 
		}

		free (inode); 
	}
}

/* INODE가 마지막으로 열려 있는 사람이 닫을 때 삭제되도록 표시합니다. */
/* Marks INODE to be deleted when it is closed by the last caller who
 * has it open. */
void
inode_remove (struct inode *inode) {
	ASSERT (inode != NULL);
	inode->removed = true;
}

/* INODE의 SIZE 바이트를 OFFSET에서 시작하여 BUFFER에 읽어옵니다.
 * 실제로 읽어온 바이트 수를 반환합니다. 오류가 발생하거나 파일 끝에 도달하면
 * SIZE보다 적을 수 있습니다. */
/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
 * Returns the number of bytes actually read, which may be less
 * than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) {
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;

	while (size > 0) {
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer. */
			disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
		} else {
			/* Read sector into bounce buffer, then partially copy
			 * into caller's buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			disk_read (filesys_disk, sector_idx, bounce);
			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}
	free (bounce);

	return bytes_read;
}
/* SIZE 바이트를 BUFFER로부터 INODE의 OFFSET 위치에 씁니다.
 * 실제로 쓰인 바이트 수를 반환합니다. 파일 끝에 도달하거나
 * 오류가 발생하면 SIZE보다 적을 수 있습니다.
 * (보통 파일 끝에 쓸 경우 inode가 확장되지만, 확장은 아직 구현되지 않았습니다.) */
/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
 * Returns the number of bytes actually written, which may be
 * less than SIZE if end of file is reached or an error occurs.
 * (Normally a write at end of file would extend the inode, but
 * growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
		off_t offset) {
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;

	if (inode->deny_write_cnt)
		return 0;

	while (size > 0) {
		/* 쓸 섹터, 섹터 내의 시작 바이트 오프셋. */
		/* Sector to write, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* inode에 남은 바이트, 섹터에 남은 바이트, 두 값 중 더 작은 값. */
		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* 실제로 이 섹터에 쓸 바이트 수. */
		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* 전체 섹터를 디스크에 직접 씁니다. */
			/* Write full sector directly to disk. */
			disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
		} else {
			/* 임시 버퍼가 필요합니다. */
			/* We need a bounce buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}

			/* 섹터에 우리가 쓰려는 부분 외에도 데이터가 있으면,
			   먼저 섹터를 읽어와야 합니다. 그렇지 않으면,
			   모두 0으로 된 섹터로 시작합니다. */
			/* If the sector contains data before or after the chunk
			   we're writing, then we need to read in the sector
			   first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || chunk_size < sector_left) 
				disk_read (filesys_disk, sector_idx, bounce);
			else
				memset (bounce, 0, DISK_SECTOR_SIZE);
			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
			disk_write (filesys_disk, sector_idx, bounce); 
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;
	}
	free (bounce);

	return bytes_written;
}

/* INODE에 대한 쓰기를 비활성화합니다.
 * inode를 여는 사람당 최대 한 번만 호출할 수 있습니다. */
/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
	inode->deny_write_cnt++;
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* INODE에 대한 쓰기를 다시 활성화합니다.
 * inode_deny_write()를 호출한 각 inode 여는 사람이 inode를 닫기 전에
 * 한 번 호출해야 합니다. */
/* Re-enables writes to INODE.
 * Must be called once by each inode opener who has called
 * inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) {
	ASSERT (inode->deny_write_cnt > 0);
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* INODE의 데이터 길이를 바이트 단위로 반환합니다. */
/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode) {
	return inode->data.length;
}
