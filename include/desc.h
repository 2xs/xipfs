/*******************************************************************************/
/*  © Université de Lille, The Pip Development Team (2015-2024)                */
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

#ifndef DESC_H
#define DESC_H

#include "xipfs.h"

#ifdef __cplusplus
extern "C" {
#endif

int xipfs_file_desc_track(xipfs_file_desc_t *descp);
int xipfs_dir_desc_track(xipfs_dir_desc_t *descp);
int xipfs_file_desc_untrack(xipfs_file_desc_t *descp);
int xipfs_dir_desc_untrack(xipfs_dir_desc_t *descp);
int xipfs_file_desc_tracked(xipfs_file_desc_t *descp);
int xipfs_dir_desc_tracked(xipfs_dir_desc_t *descp);
int xipfs_desc_untrack_all(xipfs_mount_t *mp);
int xipfs_desc_update(xipfs_mount_t *mp, xipfs_file_t *removed, size_t reserved);

#ifdef __cplusplus
}
#endif

#endif /* DESC_H */
