#include "filesys/file.h"
#include <debug.h>
#include "filesys/inode.h"
#include "threads/malloc.h"
/* 열린 파일. */
/* An open file. */
struct file {
	struct inode *inode;        /* 파일의 inode. */
                                /* File's inode. */
	off_t pos;                  /* 현재 위치. */
                                /* Current position. */
	bool deny_write;            /* file_deny_write()가 호출되었는지 여부. */
                                /* Has file_deny_write() been called? */
};

/* 주어진 INODE에 대해 파일을 열고 소유권을 가집니다.
 * 새 파일을 반환합니다. 할당 실패 또는 INODE가 null인 경우 null 포인터를 반환합니다. */
/* Opens a file for the given INODE, of which it takes ownership,
 * and returns the new file.  Returns a null pointer if an
 * allocation fails or if INODE is null. */
struct file *
file_open (struct inode *inode) {
	struct file *file = calloc (1, sizeof *file);
	if (inode != NULL && file != NULL) {
		file->inode = inode;
		file->pos = 0;
		file->deny_write = false;
		return file;
	} else {
		inode_close (inode);
		free (file);
		return NULL;
	}
}

/* FILE과 같은 inode에 대해 새 파일을 열고 반환합니다.
 * 실패 시 null 포인터를 반환합니다. */
/* Opens and returns a new file for the same inode as FILE.
 * Returns a null pointer if unsuccessful. */
struct file *
file_reopen (struct file *file) {
	return file_open (inode_reopen (file->inode));
}

/* 파일 객체를 속성을 포함하여 복제하고 FILE과 동일한 inode에 대해 새 파일을 반환합니다.
 * 실패 시 null 포인터를 반환합니다. */
/* Duplicate the file object including attributes and returns a new file for the
 * same inode as FILE. Returns a null pointer if unsuccessful. */
struct file *
file_duplicate (struct file *file) {
	struct file *nfile = file_open (inode_reopen (file->inode));
	if (nfile) {
		nfile->pos = file->pos;
		if (file->deny_write)
			file_deny_write (nfile);
	}
	return nfile;
}

/* FILE을 닫습니다. */
/* Closes FILE. */
void
file_close (struct file *file) {
	if (file != NULL) {
		file_allow_write (file);
		inode_close (file->inode);
		free (file);
	}
}

/* FILE에 캡슐화된 inode를 반환합니다. */
/* Returns the inode encapsulated by FILE. */
struct inode *
file_get_inode (struct file *file) {
	return file->inode;
}

/* FILE에서 SIZE 바이트를 BUFFER로 읽어옵니다.
 * 파일의 현재 위치에서 시작합니다.
 * 실제로 읽은 바이트 수를 반환합니다.
 * 파일 끝에 도달하면 SIZE보다 적을 수 있습니다.
 * 읽은 바이트 수만큼 FILE의 위치를 ​​진행시킵니다. */
/* Reads SIZE bytes from FILE into BUFFER,
 * starting at the file's current position.
 * Returns the number of bytes actually read,
 * which may be less than SIZE if end of file is reached.
 * Advances FILE's position by the number of bytes read. */
off_t
file_read (struct file *file, void *buffer, off_t size) {
	off_t bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
	file->pos += bytes_read;
	return bytes_read;
}

/* FILE에서 SIZE 바이트를 BUFFER로 읽어옵니다.
 * 파일의 FILE_OFS 오프셋에서 시작합니다.
 * 실제로 읽은 바이트 수를 반환합니다.
 * 파일 끝에 도달하면 SIZE보다 적을 수 있습니다.
 * 파일의 현재 위치는 영향을 받지 않습니다. */
/* Reads SIZE bytes from FILE into BUFFER,
 * starting at offset FILE_OFS in the file.
 * Returns the number of bytes actually read,
 * which may be less than SIZE if end of file is reached.
 * The file's current position is unaffected. */
off_t
file_read_at (struct file *file, void *buffer, off_t size, off_t file_ofs) {
	return inode_read_at (file->inode, buffer, size, file_ofs);
}

/* FILE에 BUFFER에서 SIZE 바이트를 씁니다.
 * 파일의 현재 위치에서 시작합니다.
 * 실제로 쓴 바이트 수를 반환합니다.
 * 파일 끝에 도달하면 SIZE보다 적을 수 있습니다.
 * (일반적으로 이 경우 파일을 확장해야 하지만, 파일 확장은 아직 구현되지 않았습니다.)
 * 읽은 바이트 수만큼 FILE의 위치를 ​​진행시킵니다. */
/* Writes SIZE bytes from BUFFER into FILE,
 * starting at the file's current position.
 * Returns the number of bytes actually written,
 * which may be less than SIZE if end of file is reached.
 * (Normally we'd grow the file in that case, but file growth is
 * not yet implemented.)
 * Advances FILE's position by the number of bytes read. */
off_t
file_write (struct file *file, const void *buffer, off_t size) {
	off_t bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
	file->pos += bytes_written;
	return bytes_written;
}

/* FILE에 BUFFER에서 SIZE 바이트를 씁니다.
 * 파일의 FILE_OFS 오프셋에서 시작합니다.
 * 실제로 쓴 바이트 수를 반환합니다.
 * 파일 끝에 도달하면 SIZE보다 적을 수 있습니다.
 * (일반적으로 이 경우 파일을 확장해야 하지만, 파일 확장은 아직 구현되지 않았습니다.)
 * 파일의 현재 위치는 영향을 받지 않습니다. */
/* Writes SIZE bytes from BUFFER into FILE,
 * starting at offset FILE_OFS in the file.
 * Returns the number of bytes actually written,
 * which may be less than SIZE if end of file is reached.
 * (Normally we'd grow the file in that case, but file growth is
 * not yet implemented.)
 * The file's current position is unaffected. */
off_t
file_write_at (struct file *file, const void *buffer, off_t size,
		off_t file_ofs) {
	return inode_write_at (file->inode, buffer, size, file_ofs);
}

/* file_allow_write()가 호출되거나 FILE이 닫힐 때까지
 * FILE의 기본 inode에 대한 쓰기 작업을 방지합니다. */
/* Prevents write operations on FILE's underlying inode
 * until file_allow_write() is called or FILE is closed. */
void
file_deny_write (struct file *file) {
	ASSERT (file != NULL);
	if (!file->deny_write) {
		file->deny_write = true;
		inode_deny_write (file->inode);
	}
}

/* FILE의 기본 inode에 대한 쓰기 작업을 다시 활성화합니다.
 * (같은 inode를 열고 있는 다른 파일에 의해 쓰기가 여전히 거부될 수 있습니다.) */
/* Re-enables write operations on FILE's underlying inode.
 * (Writes might still be denied by some other file that has the
 * same inode open.) */
void
file_allow_write (struct file *file) {
	ASSERT (file != NULL);
	if (file->deny_write) {
		file->deny_write = false;
		inode_allow_write (file->inode);
	}
}

/* FILE의 크기를 바이트 단위로 반환합니다. */
/* Returns the size of FILE in bytes. */
off_t
file_length (struct file *file) {
	ASSERT (file != NULL);
	return inode_length (file->inode);
}

/* FILE의 현재 위치를 파일 시작 지점에서 NEW_POS 바이트로 설정합니다. */
/* Sets the current position in FILE to NEW_POS bytes from the
 * start of the file. */
void
file_seek (struct file *file, off_t new_pos) {
	ASSERT (file != NULL);
	ASSERT (new_pos >= 0);
	file->pos = new_pos;
}

/* FILE의 현재 위치를 파일 시작 지점에서 바이트 오프셋으로 반환합니다. */
/* Returns the current position in FILE as a byte offset from the
 * start of the file. */
off_t
file_tell (struct file *file) {
	ASSERT (file != NULL);
	return file->pos;
}
