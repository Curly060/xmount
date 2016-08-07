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
/*
int Xmount_FuseAccess(const char*,
                      int);
*/
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
/*
int XmountFuse_StatFs(const char*,
                      struct statvfs*);
*/
int Xmount_FuseWrite(const char *p_path,
                     const char*,
                     size_t,
                     off_t,
                     struct fuse_file_info*);

#endif // XMOUNT_FUSE_H
