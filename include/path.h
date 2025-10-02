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

#ifndef XIPFS_PATH_H
#define XIPFS_PATH_H

#include "xipfs.h"

/**
 * @def XIPFS_PATH_UNDEFINED
 *
 * @brief The xipfs path is undefined
 */
#define XIPFS_PATH_UNDEFINED (0)

/**
 * @def XIPFS_PATH_CREATABLE
 *
 * @brief The xipfs path is creatable as file or as empty
 * directory
 */
#define XIPFS_PATH_CREATABLE (1)

/**
 * @def XIPFS_PATH_EXISTS_AS_FILE
 *
 * @brief The xipfs path exists as file
 */
#define XIPFS_PATH_EXISTS_AS_FILE (2)

/**
 * @def XIPFS_PATH_EXISTS_AS_EMPTY_DIR
 *
 * @brief The xipfs path exists as empty directory
 */
#define XIPFS_PATH_EXISTS_AS_EMPTY_DIR (3)

/**
 * @def XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR
 *
 * @brief The xipfs path exists as non-empty directory
 */
#define XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR (4)

/**
 * @def XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS
 *
 * @brief The xipfs path is invalid because one of the parent in
 * the path is not a directory
 */
#define XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS (5)

/**
 * @def XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND
 *
 * @brief The xipfs path is invalid because one of the parent in
 * the path does not exist
 */
#define XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND (6)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A structure representing an xipfs path
 */
typedef struct xipfs_path_s {
    /**
     * The xipfs path
     */
    char path[XIPFS_PATH_MAX];
    /**
     * The dirname components of xipfs path
     */
    char dirname[XIPFS_PATH_MAX];
    /**
     * The basename component of xipfs path
     */
    char basename[XIPFS_PATH_MAX];
    /**
     * The length of the xipfs path
     */
    size_t len;
    /**
     * The index of the last slash in the xipfs path
     */
    size_t last_slash;
    /**
     * The number of xipfs file structures in an xipfs file
     * system that tracks the path of the parent directory
     */
    size_t parent;
    /**
     * The xipfs file structure that enables the identification
     * of the type of the xipfs path
     */
    xipfs_file_t *witness;
    /**
     * The type of the xipfs path
     */
    unsigned char info;
} xipfs_path_t;

int xipfs_path_new(xipfs_mount_t *vfs_mp, xipfs_path_t *xipath, const char *path);
int xipfs_path_new_n(xipfs_mount_t *mp, xipfs_path_t *xipath, const char **path, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* XIPFS_PATH_H*/
