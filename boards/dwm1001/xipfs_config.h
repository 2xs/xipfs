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

#ifndef XIPFS_CONFIG_H
#define XIPFS_CONFIG_H

/**
 * @def XIPFS_PATH_MAX
 *
 * @brief The maximum length of an xipfs path
 */
#define XIPFS_PATH_MAX (64)

/**
 * @def XIPFS_MAGIC
 *
 * @brief The magic number of an xipfs file system
 */
#define XIPFS_MAGIC (0xf9d3b6cb)

/**
 * @def XIPFS_FILESIZE_SLOT_MAX
 *
 * @brief The maximum slot number for the list holding file
 * sizes
 */
#define XIPFS_FILESIZE_SLOT_MAX (86)

/**
 * @def XIPFS_EXEC_ARGC_MAX
 *
 * @brief The maximum number of arguments on the command line
 */
#define XIPFS_EXEC_ARGC_MAX (64)

/**
 * @def XIPFS_NVM_BASE
 *
 * @brief The non-volatile memory base address
 */
#define XIPFS_NVM_BASE (0)

/**
 * @def XIPFS_NVM_ERASE_STATE
 *
 * @brief The non-volatile memory erased state
 */
#define XIPFS_NVM_ERASE_STATE (0xff)

/**
 * @def XIPFS_NVM_NUMOF
 *
 * @brief The non-volatile memory flash page number
 */
#define XIPFS_NVM_NUMOF (128)

/**
 * @def XIPFS_NVM_WRITE_BLOCK_ALIGNMENT
 *
 * @brief The write alignment for the non-volatile memory
 */
#define XIPFS_NVM_WRITE_BLOCK_ALIGNMENT (4)

/**
 * @def XIPFS_NVM_WRITE_BLOCK_SIZE
 *
 * @brief The write size for the non-volatile memory
 */
#define XIPFS_NVM_WRITE_BLOCK_SIZE (4)

/**
 * @def XIPFS_NVM_PAGE_SIZE
 *
 * @brief The non-volatile memory flash page size
 */
#define XIPFS_NVM_PAGE_SIZE (4096)

/**
 * @def XIPFS_MAX_OPEN_DESC
 *
 * @brief The maximum number of opened descriptors
 */
#define XIPFS_MAX_OPEN_DESC (16)

#endif /* XIPFS_CONFIG_H */
