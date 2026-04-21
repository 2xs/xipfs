#include "mkxipfs.h"

#include <string.h>

uintptr_t xipfs_workstation_nvm_base = 0;
size_t xipfs_workstation_nvm_numof = 0;
uint8_t xipfs_workstation_nvm_erase_state = 0;
uintptr_t xipfs_workstation_nvm_write_block_alignment = 0;
size_t xipfs_workstation_nvm_write_block_size = 0;
size_t xipfs_workstation_nvm_page_size = 0;

void *xipfs_nvm_addr(unsigned page)
{
    return (void *)(xipfs_workstation_nvm_base + (uintptr_t)page * XIPFS_NVM_PAGE_SIZE);
}

void xipfs_nvm_erase(unsigned page)
{
    memset(xipfs_nvm_addr(page), XIPFS_NVM_ERASE_STATE, XIPFS_NVM_PAGE_SIZE);
}

unsigned xipfs_nvm_page(const void *addr)
{
    return (unsigned)(((uintptr_t)addr - xipfs_workstation_nvm_base) / XIPFS_NVM_PAGE_SIZE);
}

void xipfs_nvm_write(void *target_addr, const void *data, size_t len)
{
    memcpy(target_addr, data, len);
}
