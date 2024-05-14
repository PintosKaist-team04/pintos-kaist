#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"

/* A directory. */
struct dir {
	struct inode *inode;                /* Backing store. */
	off_t pos;                          /* 현재 위치 Current position. */
};

/* 단일 디렉토리 항목 구조체 */
/* A single directory entry. */
struct dir_entry {
	disk_sector_t inode_sector;         /* Sector number of header. */
	char name[NAME_MAX + 1];            /* Null 문자로 종료되는 파일 이름 / Null terminated file name. */
	bool in_use;                        /* 사용 중 여부 / In use or free? */
};

/* 주어진 SECTOR에 ENTRY_CNT 개의 항목을 위한 공간을 가진 디렉토리를 생성합니다.
 * 성공하면 true를 반환하고, 실패하면 false를 반환합니다. */
/* Creates a directory with space for ENTRY_CNT entries in the
 * given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (disk_sector_t sector, size_t entry_cnt) {
	return inode_create (sector, entry_cnt * sizeof (struct dir_entry));
}

/* 주어진 INODE에 대해 디렉토리를 열고 반환합니다. 이 디렉토리에 대한 소유권을 가집니다.
 * 실패 시 null 포인터를 반환합니다. */
/* Opens and returns the directory for the given INODE, of which
 * it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) {
	struct dir *dir = calloc (1, sizeof *dir);
	if (inode != NULL && dir != NULL) {
		dir->inode = inode;
		dir->pos = 0;
		return dir;
	} else {
		inode_close (inode);
		free (dir);
		return NULL;
	}
}

/* 루트 디렉토리를 열고 그에 대한 디렉토리를 반환합니다.
 * 성공하면 true를 반환하고, 실패하면 false를 반환합니다. */
/* Opens the root directory and returns a directory for it.
 * Return true if successful, false on failure. */
struct dir *
dir_open_root (void) {
	return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* DIR과 같은 inode에 대한 새로운 디렉토리를 열고 반환합니다.
 * 실패 시 null 포인터를 반환합니다. */
/* Opens and returns a new directory for the same inode as DIR.
 * Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) {
	return dir_open (inode_reopen (dir->inode));
}

/* DIR을 파괴하고 관련된 자원을 해제합니다. */
/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) {
	if (dir != NULL) {
		inode_close (dir->inode);
		free (dir);
	}
}

/* DIR에 캡슐화된 inode를 반환합니다. */
/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) {
	return dir->inode;
}

/* DIR에서 주어진 NAME의 파일을 검색합니다.
 * 성공하면 true를 반환하고, *EP를 디렉토리 엔트리로 설정합니다.
 * EP가 NULL이 아니고 OFSP가 NULL이 아니면 *OFSP를 디렉토리 항목의 바이트 오프셋으로 설정합니다.
 * 그렇지 않으면 false를 반환하고 EP와 OFSP를 무시합니다. */
/* Searches DIR for a file with the given NAME.
 * If successful, returns true, sets *EP to the directory entry
 * if EP is non-null, and sets *OFSP to the byte offset of the
 * directory entry if OFSP is non-null.
 * otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
		struct dir_entry *ep, off_t *ofsp) {
	struct dir_entry e;
	size_t ofs;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
			ofs += sizeof e)
		if (e.in_use && !strcmp (name, e.name)) {
			if (ep != NULL)
				*ep = e;
			if (ofsp != NULL)
				*ofsp = ofs;
			return true;
		}
	return false;
}

/* DIR에서 주어진 NAME의 파일을 검색하고 존재하면 true를 반환하고 그렇지 않으면 false를 반환합니다.
 * 성공 시 *INODE를 파일의 inode로 설정하고, 그렇지 않으면 null 포인터로 설정합니다.
 * 호출자는 *INODE를 닫아야 합니다. */
/* Searches DIR for a file with the given NAME
 * and returns true if one exists, false otherwise.
 * On success, sets *INODE to an inode for the file, otherwise to
 * a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
		struct inode **inode) {
	struct dir_entry e;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	if (lookup (dir, name, &e, NULL))
		*inode = inode_open (e.inode_sector);
	else
		*inode = NULL;

	return *inode != NULL;
}

/* NAME이라는 이름의 파일을 DIR에 추가합니다. DIR에는 이미 해당 이름의 파일이 없어야 합니다.
 * 파일의 inode는 INODE_SECTOR에 있습니다.
 * 성공하면 true를 반환하고, 실패하면 false를 반환합니다.
 * NAME이 유효하지 않거나(예: 너무 길거나) 디스크 또는 메모리 오류가 발생하면 실패합니다. */
/* Adds a file named NAME to DIR, which must not already contain a
 * file by that name.  The file's inode is in sector
 * INODE_SECTOR.
 * Returns true if successful, false on failure.
 * Fails if NAME is invalid (i.e. too long) or a disk or memory
 * error occurs. */
bool
dir_add (struct dir *dir, const char *name, disk_sector_t inode_sector) {
	struct dir_entry e;
	off_t ofs;
	bool success = false;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	/* NAME의 유효성을 검사합니다. */
	/* Check NAME for validity. */
	if (*name == '\0' || strlen (name) > NAME_MAX)
		return false;

	/* NAME이 사용 중이지 않은지 확인합니다. */
	/* Check that NAME is not in use. */
	if (lookup (dir, name, NULL, NULL))
		goto done;

	/* OFS를 빈 슬롯의 오프셋으로 설정합니다.
	 * 빈 슬롯이 없으면 현재 파일 끝으로 설정됩니다.
	 * inode_read_at()은 파일 끝에서만 짧은 읽기를 반환합니다.
	 * 그렇지 않으면 낮은 메모리와 같은 일시적인 이유로 인해 짧은 읽기를 얻지 않았는지 확인해야 합니다. */
	/* Set OFS to offset of free slot.
	 * If there are no free slots, then it will be set to the
	 * current end-of-file.
	 * inode_read_at() will only return a short read at end of file.
	 * Otherwise, we'd need to verify that we didn't get a short
	 * read due to something intermittent such as low memory. */
	for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
			ofs += sizeof e)
		if (!e.in_use)
			break;

	/* Write slot. */
	e.in_use = true;
	strlcpy (e.name, name, sizeof e.name);
	e.inode_sector = inode_sector;
	success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

done:
	return success;
}

/* DIR에서 NAME에 해당하는 항목을 제거합니다.
 * 성공하면 true를 반환하고, 실패하면 false를 반환합니다.
 * 이는 NAME에 해당하는 파일이 없는 경우에만 발생합니다. */
/* Removes any entry for NAME in DIR.
 * Returns true if successful, false on failure,
 * which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) {
	struct dir_entry e;
	struct inode *inode = NULL;
	bool success = false;
	off_t ofs;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	/* Find directory entry. */
	if (!lookup (dir, name, &e, &ofs))
		goto done;

	/* Open inode. */
	inode = inode_open (e.inode_sector);
	if (inode == NULL)
		goto done;

	/* Erase directory entry. */
	e.in_use = false;
	if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
		goto done;

	/* Remove inode. */
	inode_remove (inode);
	success = true;

done:
	inode_close (inode);
	return success;
}

/* DIR에서 다음 디렉토리 항목을 읽고 이름을 NAME에 저장합니다. 
 * 성공하면 true, 디렉토리에 더 이상 항목이 없으면 false를 반환합니다. */
/* Reads the next directory entry in DIR and stores the name in
 * NAME.  Returns true if successful, false if the directory
 * contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1]) {
	struct dir_entry e;

	while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) {
		dir->pos += sizeof e;
		if (e.in_use) {
			strlcpy (name, e.name, NAME_MAX + 1);
			return true;
		}
	}
	return false;
}
