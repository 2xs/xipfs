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

#ifndef XIPFS_FLASH_H
#define XIPFS_FLASH_H

/**
 * @def XIPFS_FLASH_ERASE_STATE
 *
 * @brief The erase state of the NVM as a 32-bit value
 */
#define XIPFS_FLASH_ERASE_STATE \
    ((XIPFS_NVM_ERASE_STATE << 24) | \
     (XIPFS_NVM_ERASE_STATE << 16) | \
     (XIPFS_NVM_ERASE_STATE <<  8) | \
     (XIPFS_NVM_ERASE_STATE      ))

#ifdef __cplusplus
extern "C" {
#endif

unsigned xipfs_flash_base_addr(void);
unsigned xipfs_flash_end_addr(void);
int xipfs_flash_erase_page(unsigned page);
int xipfs_flash_in(const void *addr);
int xipfs_flash_is_erased_page(unsigned page);
int xipfs_flash_overflow(const void *addr, size_t size);
int xipfs_flash_page_aligned(const void *addr);
int xipfs_flash_page_overflow(const void *addr, size_t size);
int xipfs_flash_write_32(void *dest, uint32_t src);
int xipfs_flash_write_8(void *dest, uint8_t src);
int xipfs_flash_write_unaligned(void *dest, const void *src, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* XIPFS_FLASH_H*/
