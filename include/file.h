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

#ifndef XIPFS_FILE_H
#define XIPFS_FILE_H

#include "xipfs.h"

#ifdef __cplusplus
extern "C" {
#endif

extern char *xipfs_infos_file;

int xipfs_file_erase(xipfs_file_t *filp);
int xipfs_file_exec(xipfs_file_t *filp, char *const argv[]);
int xipfs_file_filp_check(xipfs_file_t *filp);
off_t xipfs_file_get_max_pos(xipfs_file_t *filp);
off_t xipfs_file_get_reserved(xipfs_file_t *filp);
off_t xipfs_file_get_size(xipfs_file_t *filp);
off_t xipfs_file_get_size_(xipfs_file_t *filp);
int xipfs_file_path_check(const char *path);
int xipfs_file_read_8(xipfs_file_t *filp, off_t pos, char *byte);
int xipfs_file_rename(xipfs_file_t *filp, const char *to_path);
int xipfs_file_set_size(xipfs_file_t *filp, off_t size);
int xipfs_file_write_8(xipfs_file_t *filp, off_t pos, char byte);

#ifdef __cplusplus
}
#endif

#endif /* XIPFS_FILE_H */
