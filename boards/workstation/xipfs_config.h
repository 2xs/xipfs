#ifndef XIPFS_CONFIG_H
#define XIPFS_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define XIPFS_PATH_MAX (64)
#define XIPFS_MAGIC (0xf9d3b6cbUL)
#define XIPFS_FILESIZE_SLOT_MAX (86)
#define XIPFS_EXEC_ARGC_MAX (64)
#define XIPFS_MAX_OPEN_DESC (16)

/*
 * Runtime-provided backing memory values (set by host tools like mkxipfs).
 */
extern uintptr_t xipfs_workstation_nvm_base;
extern size_t xipfs_workstation_nvm_numof;

#define XIPFS_CPU_FLASH_BASE (1)
#define XIPFS_NVM_BASE ((uintptr_t)xipfs_workstation_nvm_base)
#define XIPFS_NVM_ERASE_STATE (0xff)
#define XIPFS_NVM_NUMOF ((size_t)xipfs_workstation_nvm_numof)
#define XIPFS_NVM_WRITE_BLOCK_ALIGNMENT (4)
#define XIPFS_NVM_WRITE_BLOCK_SIZE (4)
#define XIPFS_NVM_PAGE_SIZE (4096)

#endif /* XIPFS_CONFIG_H */
