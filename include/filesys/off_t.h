#ifndef FILESYS_OFF_T_H
#define FILESYS_OFF_T_H

#include <stdint.h>

/* 파일 내 오프셋입니다.
 * 이것은 다른 헤더에서 이 정의를 원하지만 다른 것은 원하지 않기 때문에
 * 별도의 헤더입니다. */
/* An offset within a file.
 * This is a separate header because multiple headers want this
 * definition but not any others. */
typedef int32_t off_t;

/* printf()용 형식 지정자, 예:
 * printf ("offset=%"PROTd"\n", offset); */
/* Format specifier for printf(), e.g.:
 * printf ("offset=%"PROTd"\n", offset); */
#define PROTd PRId32

#endif /* filesys/off_t.h */
