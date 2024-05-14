#ifndef FILESYS_FAT_H
#define FILESYS_FAT_H

#include "devices/disk.h"
#include "filesys/file.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t cluster_t;  /* FAT 내의 클러스터 인덱스 / Index of a cluster within FAT. */

#define FAT_MAGIC 0xEB3C9000 /* FAT 디스크를 식별하는 MAGIC 문자열 / MAGIC string to identify FAT disk */
#define EOChain 0x0FFFFFFF   /* 클러스터 체인의 끝 / End of cluster chain */

/* FAT 정보의 섹터들 / Sectors of FAT information. */
#define SECTORS_PER_CLUSTER 1 /* 클러스터 당 섹터 수 / Number of sectors per cluster */
#define FAT_BOOT_SECTOR 0     /* FAT 부트 섹터 / FAT boot sector. */
#define ROOT_DIR_CLUSTER 1    /* 루트 디렉토리를 위한 클러스터 / Cluster for the root directory */

void fat_init (void);
void fat_open (void);
void fat_close (void);
void fat_create (void);
void fat_close (void);

cluster_t fat_create_chain (
    cluster_t clst /* 늘릴 클러스터 #, 0: 새로운 체인 생성 / Cluster # to stretch, 0: Create a new chain */
);
void fat_remove_chain (
    cluster_t clst, /* 제거할 클러스터 / Cluster # to be removed */
    cluster_t pclst /* clst의 이전 클러스터, 0: clst가 체인의 시작 / Previous cluster of clst, 0: clst is the start of chain */
);
cluster_t fat_get (cluster_t clst);
void fat_put (cluster_t clst, cluster_t val);
disk_sector_t cluster_to_sector (cluster_t clst);

#endif /* filesys/fat.h */
