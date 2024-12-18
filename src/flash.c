/*******************************************************************************/
/*  © Université de Lille, The Pip Development Team (2015-2024)                */
/*  Copyright (C) 2020-2024 Orange                                             */
/*                                                                             */
/*  This software is a computer program whose purpose is to run a minimal,     */
/*  hypervisor relying on proven properties such as memory isolation.          */
/*                                                                             */
/*  This software is governed by the CeCILL license under French law and       */
/*  abiding by the rules of distribution of free software.  You can  use,      */
/*  modify and/ or redistribute the software under the terms of the CeCILL     */
/*  license as circulated by CEA, CNRS and INRIA at the following URL          */
/*  "http://www.cecill.info".                                                  */
/*                                                                             */
/*  As a counterpart to the access to the source code and  rights to copy,     */
/*  modify and redistribute granted by the license, users are provided only    */
/*  with a limited warranty  and the software's author,  the holder of the     */
/*  economic rights,  and the successive licensors  have only  limited         */
/*  liability.                                                                 */
/*                                                                             */
/*  In this respect, the user's attention is drawn to the risks associated     */
/*  with loading,  using,  modifying and/or developing or reproducing the      */
/*  software by the user in light of its specific status of free software,     */
/*  that may mean  that it is complicated to manipulate,  and  that  also      */
/*  therefore means  that it is reserved for developers  and  experienced      */
/*  professionals having in-depth computer knowledge. Users are therefore      */
/*  encouraged to load and test the software's suitability as regards their    */
/*  requirements in conditions enabling the security of their systems and/or   */
/*  data to be ensured and,  more generally, to use and operate it in the      */
/*  same conditions as regards security.                                       */
/*                                                                             */
/*  The fact that you are presently reading this means that you have had       */
/*  knowledge of the CeCILL license and that you accept its terms.             */
/*******************************************************************************/

#include <assert.h>

/*
 * xipfs include
 */
#include "include/errno.h"
#include "include/xipfs.h"

/**
 * @brief Returns the MCU flash memory base address
 *
 * @return The MCU flash memory base address
 */
unsigned
xipfs_flash_base_addr(void)
{
    return XIPFS_NVM_BASE;
}

/**
 * @brief Returns the MCU flash memory end address
 *
 * @return The MCU flash memory end address
 */
unsigned
xipfs_flash_end_addr(void)
{
    return XIPFS_NVM_BASE + XIPFS_NVM_NUMOF * XIPFS_NVM_PAGE_SIZE;
}

/**
 * @brief Checks whether an address points to the MCU's
 * flash memory address space
 *
 * @param addr The address to check
 *
 * @return 1 if the address points to the MCU's flash
 * memory address space, 0 otherwise
 */
int
xipfs_flash_in(const void *addr)
{
    uintptr_t val = (uintptr_t)addr;

    return
#if XIPFS_CPU_FLASH_BASE > 0
        (val >= xipfs_flash_base_addr()) &&
#endif
        (val < xipfs_flash_end_addr());
}

/**
 * @brief Checks whether an address is aligned with a
 * flash page
 *
 * @param addr The address to check
 *
 * @return 1 if the address is aligned with a flash
 * page, 0 otherwise
 */
int
xipfs_flash_page_aligned(const void *addr)
{
    uintptr_t val = (uintptr_t)addr;

    return val % XIPFS_NVM_PAGE_SIZE == 0;
}

/**
 * @brief Checks whether the copy of n bytes from addr
 * overflows the MCU's flash memory address space
 *
 * @param addr The address from which to copy n bytes
 *
 * @param n The number of bytes to copy from addr
 *
 * @return 1 if the copy of n bytes from addr overflows
 * the MCU's flash memory address space
 */
int
xipfs_flash_overflow(const void *addr, size_t n)
{
    return !xipfs_flash_in((char *)addr + n);
}

/**
 * @brief Checks whether the copy of n bytes from addr
 * overflows the flash page pointed to by addr
 *
 * @param addr The address from which to copy n bytes
 *
 * @param n The number of bytes to copy from addr
 *
 * @return 1 if the copy of n bytes from addr overflows
 * the flash page pointed to by addr, 0 otherwise
 */
int
xipfs_flash_page_overflow(const void *addr, size_t n)
{
    uintptr_t val;

    val = (uintptr_t)addr;

    return !(val % XIPFS_NVM_PAGE_SIZE + n <= XIPFS_NVM_PAGE_SIZE);
}

/**
 * @brief Copy n bytes from the unaligned memory area
 * src to the unaligned memory area dest
 *
 * @param dest The address where to copy n bytes from
 * src
 *
 * @param src The address from which to copy n bytes to
 * dest
 *
 * @param n The number of bytes to copy from src to dest
 *
 * @return 0 if the n bytes were copied from src to
 * dest, -1 otherwise
 *
 * @warning src and dest addresses must be different
 *
 * @warning dest must point to the MCU's flash memory
 * address space
 *
 * @warning The copy must not overflow the flash page
 * pointed to by dest
 *
 * @warning The copy must no overflow the MCU's flash
 * memory address space
 */
int
xipfs_flash_write_unaligned(void *dest, const void *src, size_t n)
{
    uint32_t mod, shift, addr, addr4, val4;
    uint8_t byte;
    size_t i;

    assert(dest != src);
    assert(xipfs_flash_in(dest) == 1);
    assert(xipfs_flash_overflow(dest, n) == 0);
    assert(xipfs_flash_page_overflow(dest, n) == 0);

    for (i = 0; i < n; i++) {
        /* retrieve the current byte to write */
        byte = ((uint8_t *)src)[i];

        /* cast the address to a 4-bytes integer */
        addr = (uint32_t)dest + i;

        /* calculate the modulus from the address */
        mod = addr & ((uint32_t)XIPFS_NVM_WRITE_BLOCK_ALIGNMENT-1);

        /* align the address to the previous multiple of 4 */
        addr4 = addr & ~mod;

        /* read 4 bytes at the address aligned to a multiple of 4 */
        val4 = *(uint32_t *)addr4;

        /* calculate the byte shift value */
        shift = mod << ((uint32_t)XIPFS_NVM_WRITE_BLOCK_SIZE-1);

        /* clear the byte corresponding to the shift */
        val4 &= ~((uint32_t)XIPFS_NVM_ERASE_STATE << shift);

        /* set the byte corresponding to the shift */
        val4 |= (uint32_t)byte << shift;

        /* write bytes to flash memory */
        xipfs_nvm_write((void *)addr4, &val4, XIPFS_NVM_WRITE_BLOCK_SIZE);

        /* checks the written byte against the expected byte */
        if (*(uint8_t *)addr != byte) {
            /* write failed */
            return -1;
        }
    }

    /* write succeeded */
    return 0;
}

/**
 * @brief Checks whether a flash page needs to be erased
 *
 * @param page The flash page to check
 *
 * @return 1 if the flash page needs to be erased, 0
 * otherwise
 */
int
xipfs_flash_is_erased_page(unsigned page)
{
    char *ptr;
    size_t i;

    ptr = xipfs_nvm_addr(page);
    for (i = 0; i < XIPFS_NVM_PAGE_SIZE; i++) {
        if (ptr[i] != XIPFS_NVM_ERASE_STATE) {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Erases a flash page, if needed
 *
 * @param page The flash page to erase
 *
 * @return 0 if the flash page was erased or if the
 * flash page was already erased, -1 otherwise
 */
int
xipfs_flash_erase_page(unsigned page)
{
    if (xipfs_flash_is_erased_page(page)) {
        return 0;
    }

    xipfs_nvm_erase(page);

    if (xipfs_flash_is_erased_page(page)) {
        return 0;
    }

    xipfs_errno = XIPFS_ENVMC;

    return -1;
}
