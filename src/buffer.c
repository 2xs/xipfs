/*******************************************************************************/
/*  © Université de Lille, The Pip Development Team (2015-2025)                */
/*  Copyright (C) 2020-2025 Orange                                             */
/*                                                                             */
/*  This software is a computer program whose purpose is to run a filesystem   */
/*  with in-place execution and memory isolation.                              */
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
#include <assert.h>

/*
 * xipfs includes
 */
#include "include/xipfs.h"
#include "include/buffer.h"
#include "include/errno.h"
#include "include/flash.h"

#ifdef XIPFS_WORKSTATION
#include <stdlib.h>
#endif

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
     * A valid buffer state with RAM writes not commited to flash.
     */
    XIPFS_BUFFER_DIRTY,
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
#ifdef XIPFS_WORKSTATION
    /* There's no memory alignment required when building xipfs for a workstation,
     * and compilation would fail for a workstation because in that case
     * XIPFS_NVM_WRITE_BLOCK_ALIGNMENT is defined as an extern variable.
     * Moreover, we cannot decide at compilation time the buffer size, since
     * it is a runtime parameter according to the chosen target given by the command line.
     * We could set an arbitrary maximum buffer size and check it at runtime against the target
     * flashpage size but that would be a bit brittle.
     * Then, we chose to go full dynamic allocation and add xipfs_buf_allocate/xipfs_buf_free
     * into into the buffer API, only available when building for a workstation.
     */
    char *buf;

#else /* XIPFS_WORKSTATION */
    /* On real-life boards, we need to align the buffer according to the write block alignment
     * to perform fast buffer writes.
     */
    char buf[XIPFS_NVM_PAGE_SIZE] __attribute__ ((aligned(XIPFS_NVM_WRITE_BLOCK_ALIGNMENT)));
#endif /* XIPFS_WORKSTATION */
    /**
     * The flash page number loaded into the I/O buffer
     */
    unsigned page_num;
    /**
     * The flash page address loaded into the I/O buffer
     */
    const char *page_addr;
} xipfs_buf_t;

/**
 * @internal
 *
 * @brief The buffer used by xipfs
 */
static xipfs_buf_t xipfs_buf = {
#ifdef XIPFS_WORKSTATION
    .buf = NULL,
#endif
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
static inline int
xipfs_buffer_need_flush(void)
{
    return (xipfs_buf.state == XIPFS_BUFFER_DIRTY);
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
    size_t i = 0;
    unsigned int flash_value;

    if (xipfs_buf.state != XIPFS_BUFFER_DIRTY) {
        /* no need to flush the buffer */
        return 0;
    }

    // Is a flashpage erase needed ?
    for (i = 0; i < (XIPFS_NVM_PAGE_SIZE / sizeof(flash_value)); ++i) {
        if ( ((~xipfs_buf.page_addr[i]) & xipfs_buf.buf[i]) != 0 ) {
            if (xipfs_flash_erase_page(xipfs_buf.page_num) < 0) {
                /* xipfs_errno was set */
                return -1;
            }

            break;
        }
    }

    xipfs_nvm_write(xipfs_nvm_addr(xipfs_buf.page_num), xipfs_buf.buf, XIPFS_NVM_PAGE_SIZE);
    if (memcmp(xipfs_nvm_addr(xipfs_buf.page_num), xipfs_buf.buf, XIPFS_NVM_PAGE_SIZE) != 0) {
        return -1;
    }

    xipfs_buf.state = XIPFS_BUFFER_OK;

    return 0;
}

void
xipfs_buffer_invalidate(void)
{
#ifdef XIPFS_WORKSTATION
    (void)memset(xipfs_buf.buf, 0, XIPFS_NVM_PAGE_SIZE);
#else
    /**
     * ATTENTION : mind the '&' just before xipfs_buf.buf.
     */
    (void)memset(&xipfs_buf.buf, 0, XIPFS_NVM_PAGE_SIZE);
#endif
    xipfs_buf.state = XIPFS_BUFFER_KO;
    xipfs_buf.page_num = 0;
    xipfs_buf.page_addr = NULL;
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
xipfs_buffer_load(unsigned num, const void *addr)
{
    size_t i;

    for (i = 0; i < XIPFS_NVM_PAGE_SIZE; i++) {
        xipfs_buf.buf[i] = ((const char *)addr)[i];
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
    const void *addr, *ptr;
    size_t pos, i;
    unsigned num;

    assert(dest != NULL);
    assert(src != NULL);
    if ( (dest == NULL) || (src == NULL) ) {
        xipfs_errno = XIPFS_ENULLPOINTER;
        return -1;
    }

    for (i = 0; i < len; i++) {
        ptr = (const char *)src + i;
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
xipfs_buffer_read_8(char *dest, const void *src)
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
xipfs_buffer_read_32(unsigned *dest, const void *src)
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

    assert(dest != NULL);
    assert(src != NULL);
    if ( (dest == NULL) || (src == NULL) ) {
        xipfs_errno = XIPFS_ENULLPOINTER;
        return -1;
    }

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
        xipfs_buf.state = XIPFS_BUFFER_DIRTY;
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

#ifdef XIPFS_WORKSTATION
int xipfs_buffer_allocate(void)
{
    assert(XIPFS_NVM_PAGE_SIZE > 0);
    if (xipfs_buf.buf != NULL)
        return -1;

    xipfs_buf.buf = malloc(XIPFS_NVM_PAGE_SIZE);
    if (xipfs_buf.buf == NULL)
        return -1;
    return 0;
}

void xipfs_buffer_free(void)
{
    if (xipfs_buf.buf != NULL) {
        free(xipfs_buf.buf);
        xipfs_buf.buf = NULL;
    }
}
#endif
