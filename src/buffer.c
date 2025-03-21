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

/*
 * libc includes
 */
#include <stdint.h>
#include <string.h>

/*
 * xipfs includes
 */
#include "include/xipfs.h"
#include "include/buffer.h"
#include "include/errno.h"
#include "include/flash.h"

/**
 * @internal
 *
 * @brief An enumeration that describes the state of the buffer
 */
typedef enum xipfs_buffer_state_e {
    /**
     * A valid buffer state
     */
    XIPFS_BUFFER_OK,
    /**
     *  An invalid buffer state
     */
    XIPFS_BUFFER_KO,
} xipfs_buffer_state_t;

/**
 * @internal
 *
 * @brief A structure that describes xipfs buffer
 */
typedef struct xipfs_buf_s {
    /**
     * The state of the buffer
     */
    xipfs_buffer_state_t state;
    /**
     * The I/O buffer
     */
    char buf[XIPFS_NVM_PAGE_SIZE] __attribute__ ((aligned(FLASHPAGE_WRITE_BLOCK_ALIGNMENT)));
    /**
     * The flash page number loaded into the I/O buffer
     */
    unsigned page_num;
    /**
     * The flash page address loaded into the I/O buffer
     */
    char *page_addr;
} xipfs_buf_t;

/**
 * @internal
 *
 * @brief The buffer used by xipfs
 */
static xipfs_buf_t xipfs_buf = {
    .state = XIPFS_BUFFER_KO,
};

/**
 * @internal
 *
 * @pre num must be a valid flash page number
 *
 * @brief Check whether the page passed as an argument is the
 * same as the one in the buffer
 *
 * @param num A flash page number
 *
 * @return Returns one if the page passed as an argument is the
 * same as the one in the buffer or a zero otherwise
 */
static int
xipfs_buffer_page_changed(unsigned num)
{
    return xipfs_buf.page_num != num;
}

/**
 * @internal
 *
 * @brief Checks whether the I/O buffer requires flushing
 *
 * @return Returns one if the buffer requires flushing or a
 * zero otherwise
 */
static int
xipfs_buffer_need_flush(void)
{
    size_t i;

    if (xipfs_buf.state == XIPFS_BUFFER_KO) {
        return 0;
    }

    for (i = 0; i < XIPFS_NVM_PAGE_SIZE; i++) {
        if (xipfs_buf.buf[i] != xipfs_buf.page_addr[i]) {
            return 1;
        }
    }

    return 0;
}

/**
 * @internal
 *
 * @brief Flushes the I/O buffer
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_buffer_flush(void)
{

    if (xipfs_buffer_need_flush() == 0) {
        /* no need to flush the buffer */
        return 0;
    }

    if (xipfs_flash_erase_page(xipfs_buf.page_num) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    if(flashpage_write_and_verify(xipfs_buf.page_num, xipfs_buf.buf) != FLASHPAGE_OK) {
        return -1;
    }

    (void)memset(&xipfs_buf, 0, sizeof(xipfs_buf));
    xipfs_buf.state = XIPFS_BUFFER_KO;

    return 0;
}

/**
 * @internal
 *
 * @pre num must be a valid flash page number
 *
 * @pre addr must be a valid flash page address
 *
 * @brief Loads a flash page into the I/O buffer
 *
 * @param num The number of the flash page to load into the I/O
 * buffer
 *
 * @param addr The address of the flash page to load into the
 * I/O buffer
 */
static void
xipfs_buffer_load(unsigned num, void *addr)
{
    size_t i;

    for (i = 0; i < XIPFS_NVM_PAGE_SIZE; i++) {
        xipfs_buf.buf[i] = ((char *)addr)[i];
    }
    xipfs_buf.page_num = num;
    xipfs_buf.page_addr = addr;
    xipfs_buf.state = XIPFS_BUFFER_OK;
}

/**
 * @brief Buffered implementation of the read(2) function
 *
 * @param dest A pointer to an accessible memory region where to
 * store the read bytes
 *
 * @param src A pointer to an accessible memory region from
 * which to read bytes
 *
 * @param len The number of bytes to read
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_buffer_read(void *dest, const void *src, size_t len)
{
    void *addr, *ptr;
    size_t pos, i;
    unsigned num;

    for (i = 0; i < len; i++) {
        ptr = (char *)src + i;
        if (xipfs_flash_in(ptr) < 0) {
            /* xipfs_errno was set */
            return -1;
        }
        num = xipfs_nvm_page(ptr);
        addr = xipfs_nvm_addr(num);
        if (xipfs_buf.state == XIPFS_BUFFER_KO) {
            xipfs_buffer_load(num, addr);
        } else if (xipfs_buffer_page_changed(num) == 1) {
            if (xipfs_buffer_flush() < 0) {
                /* xipfs_errno was set */
                return -1;
            }
            xipfs_buffer_load(num, addr);
        }
        pos = (uintptr_t)ptr % XIPFS_NVM_PAGE_SIZE;
        ((char *)dest)[i] = xipfs_buf.buf[pos];
    }

    return 0;
}

/**
 * @brief Reads a byte
 *
 * @param dest A pointer to an accessible memory region where to
 * store the read byte
 *
 * @param src A pointer to an accessible memory region from
 * which to read byte
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_buffer_read_8(char *dest, void *src)
{
    return xipfs_buffer_read(dest, src, sizeof(*dest));
}

/**
 * @brief Read a word
 *
 * @param dest A pointer to an accessible memory region where to
 * store the read bytes
 *
 * @param src A pointer to an accessible memory region from
 * which to read bytes
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_buffer_read_32(unsigned *dest, void *src)
{
    return xipfs_buffer_read(dest, src, sizeof(*dest));
}

/**
 * @brief Buffered implementation of the write(2) function
 *
 * @param dest A pointer to an accessible memory region where to
 * store the bytes to write
 *
 * @param src A pointer to an accessible memory region from
 * which to write bytes
 *
 * @param len The number of bytes to write
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_buffer_write(void *dest, const void *src, size_t len)
{
    void *addr, *ptr;
    size_t pos, i;
    unsigned num;

    for (i = 0; i < len; i++) {
        ptr = (char *)dest + i;
        if (xipfs_flash_in(ptr) < 0) {
            /* xipfs_errno was set */
            return -1;
        }
        num = xipfs_nvm_page(ptr);
        addr = xipfs_nvm_addr(num);
        if (xipfs_buf.state == XIPFS_BUFFER_KO) {
            xipfs_buffer_load(num, addr);
        } else if (xipfs_buffer_page_changed(num) == 1) {
            if (xipfs_buffer_flush() < 0) {
                /* xipfs_errno was set */
                return -1;
            }
            xipfs_buffer_load(num, addr);
        }
        pos = (uintptr_t)ptr % XIPFS_NVM_PAGE_SIZE;
        xipfs_buf.buf[pos] = ((char *)src)[i];
    }

    return 0;
}

/**
 * @brief Write a byte
 *
 * @param dest A pointer to an accessible memory region where to
 * write byte
 *
 * @param src A pointer to an accessible memory region from
 * which to write byte
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_buffer_write_8(void *dest, char src)
{
    return xipfs_buffer_write(dest, &src, sizeof(src));
}

/**
 * @brief Write a word
 *
 * @param dest A pointer to an accessible memory region where to
 * write bytes
 *
 * @param src A pointer to an accessible memory region from
 * which to write bytes
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_buffer_write_32(void *dest, unsigned src)
{
    return xipfs_buffer_write(dest, &src, sizeof(src));
}
