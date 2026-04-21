#ifndef XIPFS_CONFIG_H
#define XIPFS_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define XIPFS_WORKSTATION

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
extern uint8_t xipfs_workstation_nvm_erase_state;
extern uintptr_t xipfs_workstation_nvm_write_block_alignment;
extern size_t xipfs_workstation_nvm_write_block_size;
extern size_t xipfs_workstation_nvm_page_size;

#define XIPFS_CPU_FLASH_BASE            (1)
#define XIPFS_NVM_BASE                  ((uintptr_t)xipfs_workstation_nvm_base)
#define XIPFS_NVM_ERASE_STATE           (xipfs_workstation_nvm_erase_state)
#define XIPFS_NVM_NUMOF                 (xipfs_workstation_nvm_numof)
#define XIPFS_NVM_WRITE_BLOCK_ALIGNMENT (xipfs_workstation_nvm_write_block_alignment)
#define XIPFS_NVM_WRITE_BLOCK_SIZE      (xipfs_workstation_nvm_write_block_size)
#define XIPFS_NVM_PAGE_SIZE             (xipfs_workstation_nvm_page_size)

#endif /* XIPFS_CONFIG_H */
