/*******************************************************************************
* xmount Copyright (c) 2008-2016 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* This program is free software: you can redistribute it and/or modify it      *
* under the terms of the GNU General Public License as published by the Free   *
* Software Foundation, either version 3 of the License, or (at your option)    *
* any later version.                                                           *
*                                                                              *
* This program is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for     *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* this program. If not, see <http://www.gnu.org/licenses/>.                    *
*******************************************************************************/

#ifndef XMOUNT_FUSE_H
#define XMOUNT_FUSE_H

#define FUSE_USE_VERSION 26
#include <fuse.h>

int XmountFuse_create(const char *p_path,
                      mode_t mode,
                      struct fuse_file_info *p_fi);

int XmountFuse_ftruncate(const char *p_path,
                         off_t offset,
                         struct fuse_file_info *p_fi);

int XmountFuse_open(const char *p_path, struct fuse_file_info *p_fi);

int XmountFuse_read(const char *p_path,
                    char *p_buf,
                    size_t count,
                    off_t offset,
                    struct fuse_file_info *p_fi);

int XmountFuse_release(const char *p_path, struct fuse_file_info *p_fi);

int XmountFuse_write(const char *p_path,
                     const char *p_buf,
                     size_t count,
                     off_t offset,
                     struct fuse_file_info *p_fi);

int XmountFuse_mkdir(const char *p_path, mode_t mode);

int XmountFuse_readdir(const char *p_path,
                       void *p_buf,
                       fuse_fill_dir_t filler,
			                 off_t offset,
                       struct fuse_file_info *p_fi);

int XmountFuse_chmod(const char *p_path, mode_t mode);

int XmountFuse_chown(const char *p_path, uid_t uid, gid_t gid);

int XmountFuse_getattr(const char *p_path, struct stat *p_stbuf);

int XmountFuse_link(const char *p_target_path, const char *p_link_path);

int XmountFuse_readlink(const char *p_link_path,
                        char *p_buf,
                        size_t buf_size);

int XmountFuse_rename(const char *p_old_path, const char *p_new_path);

int XmountFuse_rmdir(const char *p_path);

//int XmountFuse_statfs(const char *p_path, struct statvfs *p_stat);

int XmountFuse_symlink(const char *p_target_path, const char *p_link_path);

int XmountFuse_truncate(const char *p_path, off_t new_size);

int XmountFuse_utimens(const char *p_path, const struct timespec tv[2]);

int XmountFuse_unlink(const char *p_path);

/*
int Xmount_FuseAccess(const char*,
                      int);
*/
/*
int Xmount_FuseGetAttr(const char*,
                       struct stat*);
int Xmount_FuseMkDir(const char*,
                     mode_t);
int Xmount_FuseMkNod(const char*,
                     mode_t, dev_t);
int Xmount_FuseReadDir(const char*,
                       void*,
                       fuse_fill_dir_t,
                       off_t,
                       struct fuse_file_info*);
int Xmount_FuseOpen(const char*,
                    struct fuse_file_info*);
int Xmount_FuseRead(const char*,
                    char*,
                    size_t,
                    off_t,
                    struct fuse_file_info*);
int Xmount_FuseRename(const char*,
                      const char*);
int Xmount_FuseRmDir(const char*);
int Xmount_FuseUnlink(const char*);
*/
/*
int XmountFuse_StatFs(const char*,
                      struct statvfs*);
*/
/*
int Xmount_FuseWrite(const char *p_path,
                     const char*,
                     size_t,
                     off_t,
                     struct fuse_file_info*);
*/

#endif // XMOUNT_FUSE_H
