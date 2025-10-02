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

#ifndef XIPFS_FS_H
#define XIPFS_FS_H

#ifdef __cplusplus
extern "C" {
#endif

int xipfs_fs_format(xipfs_mount_t *vfs_mp);
int xipfs_fs_free_pages(xipfs_mount_t *vfs_mp);
int xipfs_fs_get_page_number(xipfs_mount_t *vfs_mp);
xipfs_file_t *xipfs_fs_head(xipfs_mount_t *vfs_mp);
int xipfs_fs_mountp_check(xipfs_mount_t *mountp);
xipfs_file_t *xipfs_fs_new_file(xipfs_mount_t *vfs_mp, const char *path, size_t size, int exec);
xipfs_file_t *xipfs_fs_next(xipfs_file_t *filp);
int xipfs_fs_remove(void *dst);
int xipfs_fs_rename_all(xipfs_mount_t *vfs_mp, const char *from, const char *to);
xipfs_file_t *xipfs_fs_tail(xipfs_mount_t *vfs_mp);
xipfs_file_t *xipfs_fs_tail_next(xipfs_mount_t *vfs_mp);

#ifdef __cplusplus
}
#endif

#endif /* XIPFS_FS_H*/
