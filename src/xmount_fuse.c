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

#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>

#include "xmount_fuse.h"
#include "xmount_cache.h"
#include "../libxmount/libxmount.h"
#include "xmount.h"

#define LOG_WARNING(...) {            \
  LIBXMOUNT_LOG_WARNING(__VA_ARGS__); \
}
#define LOG_ERROR(...) {            \
  LIBXMOUNT_LOG_ERROR(__VA_ARGS__); \
}
#define LOG_DEBUG(...) {                              \
  LIBXMOUNT_LOG_DEBUG(glob_xmount.debug,__VA_ARGS__); \
}

/*******************************************************************************
 * FUSE operations implementation
 *   File functions
 ******************************************************************************/
/*
 * XmountFuse_create
 */
int XmountFuse_create(const char *p_path,
                      mode_t mode,
                      struct fuse_file_info *p_fi)
{
  teGidaFsError ret=eGidaFsError_None;
  teGidaFsOpenFileFlag flags=eGidaFsOpenFileFlag_None;
  teGidaFsNodePermissions perms=eGidaFsNodeFlag_None;
  hGidaFsFile h_file=NULL;

  if((p_fi->flags&O_TMPFILE)==O_TMPFILE) {
    // GidaFS does not support the O_TMPFILE flag
    return -EOPNOTSUPP;
  }

  // Parse open flags and always add eGidaFsOpenFileFlag_Create
  if((p_fi->flags&O_RDONLY)==O_RDONLY) flags|=eGidaFsOpenFileFlag_ReadOnly;
  if((p_fi->flags&O_WRONLY)==O_WRONLY) flags|=eGidaFsOpenFileFlag_WriteOnly;
  if((p_fi->flags&O_RDWR)==O_RDWR) flags|=eGidaFsOpenFileFlag_ReadWrite;
  flags|=eGidaFsOpenFileFlag_Create;

  // Parse mode
  if((mode&S_IXOTH)==S_IXOTH) perms|=eGidaFsNodeFlag_Xoth;
  if((mode&S_IWOTH)==S_IWOTH) perms|=eGidaFsNodeFlag_Woth;
  if((mode&S_IROTH)==S_IROTH) perms|=eGidaFsNodeFlag_Roth;
  if((mode&S_IRWXO)==S_IRWXO) perms|=eGidaFsNodeFlag_RWXo;
  if((mode&S_IXGRP)==S_IXGRP) perms|=eGidaFsNodeFlag_Xgrp;
  if((mode&S_IWGRP)==S_IWGRP) perms|=eGidaFsNodeFlag_Wgrp;
  if((mode&S_IRGRP)==S_IRGRP) perms|=eGidaFsNodeFlag_Rgrp;
  if((mode&S_IRWXG)==S_IRWXG) perms|=eGidaFsNodeFlag_RWXg;
  if((mode&S_IXUSR)==S_IXUSR) perms|=eGidaFsNodeFlag_Xusr;
  if((mode&S_IWUSR)==S_IWUSR) perms|=eGidaFsNodeFlag_Wusr;
  if((mode&S_IRUSR)==S_IRUSR) perms|=eGidaFsNodeFlag_Rusr;
  if((mode&S_IRWXU)==S_IRWXU) perms|=eGidaFsNodeFlag_RWXu;

  // Try to open requested file
  if((ret=GidaFsLib_OpenFile(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                             p_path,
                             &h_file,
                             flags,
                             perms))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to open file '%s': Error %u!",p_path,ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  // Save file handle and return
  p_fi->fh=(uint64_t)h_file;
  return 0;
}

/*
 * XmountFuse_ftruncate
 */
int XmountFuse_ftruncate(const char *p_path,
                         off_t offset,
                         struct fuse_file_info *p_fi)
{
  teGidaFsError ret=eGidaFsError_None;
  hGidaFsFile h_file=(hGidaFsFile)p_fi->fh;

  // Try to truncate file
  if((ret=GidaFsLib_TruncateFile(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                                 h_file,
                                 offset))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to truncate file handle for file '%s': Error %u!",
              p_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return 0;
}

/*
 * XmountFuse_open
 */
int XmountFuse_open(const char *p_path, struct fuse_file_info *p_fi) {
  teGidaFsError ret=eGidaFsError_None;
  teGidaFsOpenFileFlag flags=eGidaFsOpenFileFlag_None;
  hGidaFsFile h_file=NULL;

  if((p_fi->flags&O_TMPFILE)==O_TMPFILE) {
    // GidaFS does not support the O_TMPFILE flag
    return -EOPNOTSUPP;
  }

  // Parse open flags
  // TODO: Need further refinements, see man open(2)
  if((p_fi->flags&O_RDONLY)==O_RDONLY) flags|=eGidaFsOpenFileFlag_ReadOnly;
  if((p_fi->flags&O_WRONLY)==O_WRONLY) flags|=eGidaFsOpenFileFlag_WriteOnly;
  if((p_fi->flags&O_RDWR)==O_RDWR) flags|=eGidaFsOpenFileFlag_ReadWrite;
  if((p_fi->flags&O_CREAT)==O_CREAT) flags|=eGidaFsOpenFileFlag_Create;
  if((p_fi->flags&O_EXCL)==O_EXCL) flags|=eGidaFsOpenFileFlag_CreateAlways;
  if((p_fi->flags&O_TRUNC)==O_TRUNC) flags|=eGidaFsOpenFileFlag_CreateAlways;

  // Try to open requested file
  if((ret=GidaFsLib_OpenFile(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                             p_path,
                             &h_file,
                             flags,
                             eGidaFsNodeFlag_None))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to open file '%s': Error %u!",p_path,ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  // Save file handle and return
  p_fi->fh=(uint64_t)h_file;
  return 0;
}

/*
 * XmountFuse_read
 */
int XmountFuse_read(const char *p_path,
                    char *p_buf,
                    size_t count,
                    off_t offset,
                    struct fuse_file_info *p_fi)
{
  teGidaFsError ret=eGidaFsError_None;
  hGidaFsFile h_file=(hGidaFsFile)p_fi->fh;
  uint64_t read=0;

  // Try to read requested data
  if((ret=GidaFsLib_ReadFile(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                             h_file,
                             offset,
                             count,
                             p_buf,
                             &read))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to read %" PRIu64 " bytes at offset %" PRIu64
                " from file '%s': Error %u!",
              count,
              offset,
              p_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return read;
}

/*
 * XmountFuse_release
 */
int XmountFuse_release(const char *p_path, struct fuse_file_info *p_fi) {
  teGidaFsError ret=eGidaFsError_None;
  hGidaFsFile h_file=(hGidaFsFile)p_fi->fh;

  // Try to close file
  if((ret=GidaFsLib_CloseFile(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                              &h_file))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to close file handle for file '%s': Error %u!",
              p_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  // Clear file handle and return
  p_fi->fh=0;
  return 0;
}

/*
 * XmountFuse_write
 */
int XmountFuse_write(const char *p_path,
                     const char *p_buf,
                     size_t count,
                     off_t offset,
                     struct fuse_file_info *p_fi)
{
  teGidaFsError ret=eGidaFsError_None;
  hGidaFsFile h_file=(hGidaFsFile)p_fi->fh;
  uint64_t written=0;

  // Try to write requested data
  if((ret=GidaFsLib_WriteFile(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                              h_file,
                              offset,
                              count,
                              p_buf,
                              &written))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to write %" PRIu64 " bytes to offset %" PRIu64
                " in file '%s': Error %u!",
              count,
              offset,
              p_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return written;
}

/*******************************************************************************
 * FUSE operations implementation
 *   Directory functions
 ******************************************************************************/
/*
 * XmountFuse_mkdir
 */
int XmountFuse_mkdir(const char *p_path, mode_t mode) {
  teGidaFsError ret=eGidaFsError_None;
  teGidaFsNodePermissions perms=eGidaFsNodeFlag_None;

  // Parse mode
  if((mode&S_IXOTH)==S_IXOTH) perms|=eGidaFsNodeFlag_Xoth;
  if((mode&S_IWOTH)==S_IWOTH) perms|=eGidaFsNodeFlag_Woth;
  if((mode&S_IROTH)==S_IROTH) perms|=eGidaFsNodeFlag_Roth;
  if((mode&S_IRWXO)==S_IRWXO) perms|=eGidaFsNodeFlag_RWXo;
  if((mode&S_IXGRP)==S_IXGRP) perms|=eGidaFsNodeFlag_Xgrp;
  if((mode&S_IWGRP)==S_IWGRP) perms|=eGidaFsNodeFlag_Wgrp;
  if((mode&S_IRGRP)==S_IRGRP) perms|=eGidaFsNodeFlag_Rgrp;
  if((mode&S_IRWXG)==S_IRWXG) perms|=eGidaFsNodeFlag_RWXg;
  if((mode&S_IXUSR)==S_IXUSR) perms|=eGidaFsNodeFlag_Xusr;
  if((mode&S_IWUSR)==S_IWUSR) perms|=eGidaFsNodeFlag_Wusr;
  if((mode&S_IRUSR)==S_IRUSR) perms|=eGidaFsNodeFlag_Rusr;
  if((mode&S_IRWXU)==S_IRWXU) perms|=eGidaFsNodeFlag_RWXu;

  if((ret=GidaFsLib_CreateDir(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                              p_path,
                              perms))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to create directory '%s': Error %u!",p_path,ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return 0;
}

/*
 * XmountFuse_readdir
 */
int XmountFuse_readdir(const char *p_path,
                       void *p_buf,
                       fuse_fill_dir_t filler,
			                 off_t offset,
                       struct fuse_file_info *p_fi)
{
	(void)offset;
	(void)p_fi;
  teGidaFsError ret=eGidaFsError_None;
  hGidaFsDir h_dir=NULL;
  GidaFsDirEntry entry;

  // Try to open directory
  if((ret=GidaFsLib_OpenDir(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                            p_path,
                            &h_dir))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to open directory '%s': Error %u!",
              p_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  // Fill in "." and ".." entries
	filler(p_buf,".",NULL,0);
	filler(p_buf,"..",NULL,0);

  // Fill in directory content
  while((ret=GidaFsLib_ReadDir(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                               h_dir,
                               &entry))==eGidaFsError_None)
  {
    filler(p_buf,entry.name,NULL,0);
  }
  if(ret!=eGidaFsError_NoMoreEntries) {
    LOG_ERROR("Unable to read all directory entries of '%s': Error %u!",
              p_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  // Close directory
  if((ret=GidaFsLib_CloseDir(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                             &h_dir))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to close directory '%s': Error %u!",
              p_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return 0;
}

/*******************************************************************************
 * FUSE operations implementation
 *   Misc functions
 ******************************************************************************/
/*
 * XmountFuse_chmod
 */
int XmountFuse_chmod(const char *p_path, mode_t mode) {
  teGidaFsError ret=eGidaFsError_None;
  teGidaFsNodePermissions perms=eGidaFsNodeFlag_None;

#define XMOUNTFUSE_CHMOD__SET_FLAG(f,ff) if((mode&f)==f) perms|=ff

  XMOUNTFUSE_CHMOD__SET_FLAG(S_IXOTH,eGidaFsNodeFlag_Xoth);
  XMOUNTFUSE_CHMOD__SET_FLAG(S_IWOTH,eGidaFsNodeFlag_Woth);
  XMOUNTFUSE_CHMOD__SET_FLAG(S_IROTH,eGidaFsNodeFlag_Roth);
  XMOUNTFUSE_CHMOD__SET_FLAG(S_IXGRP,eGidaFsNodeFlag_Xgrp);
  XMOUNTFUSE_CHMOD__SET_FLAG(S_IWGRP,eGidaFsNodeFlag_Wgrp);
  XMOUNTFUSE_CHMOD__SET_FLAG(S_IRGRP,eGidaFsNodeFlag_Rgrp);
  XMOUNTFUSE_CHMOD__SET_FLAG(S_IXUSR,eGidaFsNodeFlag_Xusr);
  XMOUNTFUSE_CHMOD__SET_FLAG(S_IWUSR,eGidaFsNodeFlag_Wusr);
  XMOUNTFUSE_CHMOD__SET_FLAG(S_IRUSR,eGidaFsNodeFlag_Rusr);
  XMOUNTFUSE_CHMOD__SET_FLAG(S_ISVTX,eGidaFsNodeFlag_Svtx);
  XMOUNTFUSE_CHMOD__SET_FLAG(S_ISGID,eGidaFsNodeFlag_Sgid);
  XMOUNTFUSE_CHMOD__SET_FLAG(S_ISUID,eGidaFsNodeFlag_Suid);

#undef XMOUNTFUSE_CHMOD__SET_FLAG

  if((ret=GidaFsLib_SetAttr(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                            p_path,
                            perms,
                            0,
                            0,
                            0))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to set permissions 0x%08x on node '%s': Error %u!",
              mode,
              p_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return 0;
}

/*
 * XmountFuse_chown
 */
int XmountFuse_chown(const char *p_path, uid_t uid, gid_t gid) {
  // GidaFS does not implement user and group ids. Simply return no error
  // when someone calls this
  return 0;
}

/*
 * XmountFuse_getattr
 */
int XmountFuse_getattr(const char *p_path, struct stat *p_stbuf) {
  teGidaFsError ret=eGidaFsError_None;
  GidaFsNodeInfo node_info;

  // Get requested node infos
  if((ret=GidaFsLib_GetAttr(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                            p_path,
                            &node_info))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to stat node '%s': Error %u!",p_path,ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  // Clear stat buffer
  memset(p_stbuf,0x00,sizeof(struct stat));

  // Set uid and gid
  p_stbuf->st_uid=getuid();
  p_stbuf->st_gid=getgid();

  // Populate st_mode
  switch(node_info.type) {
    case eGidaFsNodeType_File: {
      p_stbuf->st_mode=S_IFREG;
      p_stbuf->st_nlink=node_info.link_count;
      break;
    }
    case eGidaFsNodeType_Dir: {
      p_stbuf->st_mode=S_IFDIR;
      // As GidaFS does not have '.' entries, need to increment link count here
      p_stbuf->st_nlink=node_info.link_count+1;
      break;
    }
    case eGidaFsNodeType_SymLink: {
      p_stbuf->st_mode=S_IFLNK;
      p_stbuf->st_nlink=node_info.link_count;
      break;
    }
    default: {
      LOG_ERROR("GidaFsLib_GetNodeAttr() returned unknown node type for '%s'",
                p_path);
      return -EFAULT;
    }
  }

#define XMOUNTFUSE_GETATTR__SET_FLAG(f,ff) \
  if((node_info.mode&f)==f) p_stbuf->st_mode|=ff

  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Xoth,S_IXOTH);
  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Woth,S_IWOTH);
  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Roth,S_IROTH);
  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Xgrp,S_IXGRP);
  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Wgrp,S_IWGRP);
  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Rgrp,S_IRGRP);
  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Xusr,S_IXUSR);
  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Wusr,S_IWUSR);
  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Rusr,S_IRUSR);
  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Svtx,S_ISVTX);
  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Sgid,S_ISGID);
  XMOUNTFUSE_GETATTR__SET_FLAG(eGidaFsNodeFlag_Suid,S_ISUID);

#undef XMOUNTFUSE_GETATTR__SET_FLAG

  // Populate rest of stat struct values
  p_stbuf->st_size=node_info.logical_size;
  p_stbuf->st_blksize=node_info.blocksize;
  // st_blocks uses 512B blocks
  p_stbuf->st_blocks=node_info.physical_size/512;
  // TODO: Our indoes are always 64bit. Might pose probs for ino_t !!
  p_stbuf->st_ino=node_info.inode;
  // TODO: Our times are always 64bit. Might pose probs on some platforms !!
  p_stbuf->st_mtime=node_info.mtime;
  p_stbuf->st_atime=node_info.atime;
  p_stbuf->st_ctime=node_info.ctime;

  LOG_DEBUG("'%s': Type: 0x%08x, Perm: 0x%08x -> Mode: 0x%08x",
            p_path,
            node_info.type,
            node_info.mode,
            p_stbuf->st_mode,
            p_stbuf->st_size);

	return 0;
}

int XmountFuse_link(const char *p_target_path, const char *p_link_path) {
  teGidaFsError ret=eGidaFsError_None;

  if((ret=GidaFsLib_Link(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                         p_target_path,
                         p_link_path))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to link '%s' to '%s': Error %u!",
              p_target_path,
              p_link_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return 0;
}

/*
 * XmountFuse_readlink
 */
int XmountFuse_readlink(const char *p_link_path,
                        char *p_buf,
                        size_t buf_size)
{
  teGidaFsError ret=eGidaFsError_None;

  if((ret=GidaFsLib_ReadSymLink(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                                p_link_path,
                                p_buf,
                                buf_size))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to read contents of symlink '%s': Error %u!",
              p_link_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return 0;
}

/*
 * XmountFuse_rename
 */
int XmountFuse_rename(const char *p_old_path, const char *p_new_path) {
  teGidaFsError ret=eGidaFsError_None;

  // Rename / move node
  if((ret=GidaFsLib_Move(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                         p_old_path,
                         p_new_path))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to move / rename '%s' to '%s': Error %u!",
              p_old_path,
              p_new_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return 0;
}

/*
 * XmountFuse_rmdir
 */
int XmountFuse_rmdir(const char *p_path) {
  return XmountFuse_unlink(p_path);
  // TODO: Check error and return correct errno code
}

/*
 * XmountFuse_statfs
 */
/*
int XmountFuse_statfs(const char *p_path, struct statvfs *p_stat) {
  uint64_t ufree=0;
  struct statvfs ustat;
  GidaFsInfo info_struct;
  teGidaFsError ret=eGidaFsError_None;

  // Get values from underlying fs
  if(statvfs(globs.p_gidafs_file,&ustat)!=0) {
    LOG_ERROR("Unable to get stats from underlying fs!");
    return -errno;
  }

  // Get GidaFS infos
  if((ret=GidaFsLib_GetFsInfo(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                              &info_struct))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to get GidaFS infos: Error %u",ret)
    return XmountCache_GidaFsError2Errno(ret);
  }

  // Calculate how many GidaFS blocks are free in underlying fs
  ufree=((uint64_t)(((uint64_t)(ustat.f_bfree))*((uint64_t)(ustat.f_frsize))));
  ufree/=info_struct.blocksize;
  LOG_ERROR("According to statvfs, %" PRIu64 " blocks of %" PRIu64
              " bytes are free in underlying fs",
            ufree,
            info_struct.blocksize);

  // Fill statvfs structure
  p_stat->f_bsize=info_struct.blocksize;
  p_stat->f_frsize=p_stat->f_bsize;
  p_stat->f_fsid=0x47494441;
  p_stat->f_flag=0;
  p_stat->f_namemax=GIDAFS_MAX_NODENAME_LEN;

  // It is not possible to set the following values correctly as they are
  // missing used vars!
  p_stat->f_blocks=ufree;
  p_stat->f_bfree=ufree;
  p_stat->f_bavail=p_stat->f_bfree;
  p_stat->f_files=ufree;
  p_stat->f_ffree=ufree;
  p_stat->f_favail=p_stat->f_ffree;

  return 0;
}
*/

/*
 * XmountFuse_symlink
 */
int XmountFuse_symlink(const char *p_target_path, const char *p_link_path) {
  teGidaFsError ret=eGidaFsError_None;

  if((ret=GidaFsLib_SymLink(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                            p_target_path,
                            p_link_path))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to symlink '%s' to '%s': Error %u!",
              p_target_path,
              p_link_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return 0;
}

/*
 * XmountFuse_truncate
 */
int XmountFuse_truncate(const char *p_path, off_t new_size) {
  teGidaFsError ret=eGidaFsError_None;
  hGidaFsFile h_file=NULL;

  // Try to open requested file
  if((ret=GidaFsLib_OpenFile(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                             p_path,
                             &h_file,
                             eGidaFsOpenFileFlag_ReadWrite,
                             eGidaFsNodeFlag_None))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to open file '%s': Error %u!",p_path,ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  // Try to truncate file to requested size
  if((ret=GidaFsLib_TruncateFile(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                                 h_file,
                                 new_size))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to truncate file '%s': Error %u!",p_path,ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  // Try to close file again
  if((ret=GidaFsLib_CloseFile(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                              &h_file))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to close file '%s': Error %u!",p_path,ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return 0;
}

/*
 * XmountFuse_utimens
 */
int XmountFuse_utimens(const char *p_path, const struct timespec tv[2]) {
  teGidaFsError ret=eGidaFsError_None;
  uint64_t mtime=0;
  uint64_t atime=0;

  // Decide what and how to update
  if(tv!=NULL) {
    if(tv[0].tv_nsec==UTIME_NOW) atime=(uint64_t)time(NULL);
    else if(tv[0].tv_nsec!=UTIME_OMIT) atime=(uint64_t)tv[0].tv_sec;

    if(tv[1].tv_nsec==UTIME_NOW) mtime=(uint64_t)time(NULL);
    else if(tv[1].tv_nsec!=UTIME_OMIT) mtime=(uint64_t)tv[1].tv_sec;
  } else {
    mtime=(uint64_t)time(NULL);
    atime=mtime;
  }

  // Update times
  if((ret=GidaFsLib_SetAttr(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),
                            p_path,
                            eGidaFsNodeFlag_Invalid,
                            mtime,
                            atime,
                            0))!=eGidaFsError_None)
  {
    LOG_ERROR("Unable to set modification / last accessed times on node '%s': "
                "Error %u!",
              p_path,
              ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return 0;
}

/*
 * XmountFuse_unlink
 */
int XmountFuse_unlink(const char *p_path) {
  teGidaFsError ret=eGidaFsError_None;

  if((ret=GidaFsLib_Unlink(XmountCache_GetGidaFsHandle(glob_xmount.h_cache),p_path))!=eGidaFsError_None) {
    LOG_ERROR("Unable to delete node '%s': Error %u",p_path,ret);
    return XmountCache_GidaFsError2Errno(ret);
  }

  return 0;
}







//! FUSE getattr implementation
/*!
 * \param p_path Path of file to get attributes from
 * \param p_stat Pointer to stat structure to save attributes to
 * \return 0 on success, negated error code on error
 */
/*
int Xmount_FuseGetAttr(const char *p_path, struct stat *p_stat) {
  te_XmountError xmount_ret=e_XmountError_None;

  memset(p_stat,0,sizeof(struct stat));
  if((xmount_ret=Xmount_GetFileAttr(p_path,p_stat))!=e_XmountError_None) {
    LOG_ERROR("Couldn't get file attributes: Error code %u!\n",xmount_ret);
    return -ENOENT;
  }
*/
/*
  if(strcmp(p_path,"/")==0) {
    // Attributes of mountpoint
    p_stat->st_mode=S_IFDIR | 0777;
    p_stat->st_nlink=2;
  } else if(strcmp(p_path,glob_xmount.output.p_virtual_image_path)==0) {
    // Attributes of virtual image
    if(!glob_xmount.output.writable) p_stat->st_mode=S_IFREG | 0444;
    else p_stat->st_mode=S_IFREG | 0666;
    p_stat->st_nlink=1;
    // Get output image file size
    if(!GetOutputImageSize((uint64_t*)&(p_stat->st_size))) {
      LOG_ERROR("Couldn't get image size!\n");
      return -ENOENT;
    }
    // Make sure virtual image seems to be fully allocated (not sparse file).
    p_stat->st_blocks=p_stat->st_size/512;
    if(p_stat->st_size%512!=0) p_stat->st_blocks++;
  } else if(strcmp(p_path,glob_xmount.output.p_info_path)==0) {
    // Attributes of virtual image info file
    p_stat->st_mode=S_IFREG | 0444;
    p_stat->st_nlink=1;
    // Get virtual image info file size
    if(glob_xmount.output.p_info_file!=NULL) {
      p_stat->st_size=strlen(glob_xmount.output.p_info_file);
    } else p_stat->st_size=0;
  } else return -ENOENT;
  // Set uid and gid of all files to uid and gid of current process
  p_stat->st_uid=getuid();
  p_stat->st_gid=getgid();
*/
/*
  return 0;
}
*/

//! FUSE mkdir implementation
/*!
 * \param p_path Directory path
 * \param mode Directory permissions
 * \return 0 on success, negated error code on error
 */
/*
int Xmount_FuseMkDir(const char *p_path, mode_t mode) {
  // TODO: Implement
  LOG_ERROR("Attempt to create directory \"%s\" "
            "on read-only filesystem!\n",p_path)
  return -1;
}
*/

//! FUSE create implementation.
/*!
 * \param p_path File to create
 * \param mode File mode
 * \param dev ??? but not used
 * \return 0 on success, negated error code on error
 */
/*
int Xmount_FuseMkNod(const char *p_path, mode_t mode, dev_t dev) {
  // TODO: Implement
  LOG_ERROR("Attempt to create illegal file \"%s\"\n",p_path)
  return -1;
}
*/

//! FUSE readdir implementation
/*!
 * \param p_path Path from where files should be listed
 * \param p_buf Buffer to write file entrys to
 * \param filler Function to write dir entrys to buffer
 * \param offset ??? but not used
 * \param p_fi File info struct
 * \return 0 on success, negated error code on error
 */
/*
int Xmount_FuseReadDir(const char *p_path,
                       void *p_buf,
                       fuse_fill_dir_t filler,
                       off_t offset,
                       struct fuse_file_info *p_fi)
{
  uint64_t dir_entries=0;
  char **pp_dir_entries=NULL;
  te_XmountError xmount_ret=e_XmountError_None;

  // Ignore some params
  (void)offset;
  (void)p_fi;

  // Get directory listing
  xmount_ret=Xmount_GetDirListing(p_path,
                                  &dir_entries,
                                  &pp_dir_entries);
  if(xmount_ret!=e_XmountError_None) {
    LOG_ERROR("Couldn't get directory listing: Error code %u!\n",xmount_ret);
    return -ENOENT;
  }

  // Add std . and .. entrys
  filler(p_buf,".",NULL,0);
  filler(p_buf,"..",NULL,0);

  // Add retrieved entries
  for(uint64_t i=0;i<dir_entries;i++) {
    filler(p_buf,pp_dir_entries[i],NULL,0);
    XMOUNT_FREE(pp_dir_entries[i]);
  }
  XMOUNT_FREE(pp_dir_entries);

  return 0;
}
*/

//! FUSE open implementation
/*!
 * \param p_path Path to file to open
 * \param p_fi File info struct
 * \return 0 on success, negated error code on error
 */
/*
int Xmount_FuseOpen(const char *p_path, struct fuse_file_info *p_fi) {

#define CHECK_OPEN_PERMS() {                                              \
  if(!glob_xmount.output.writable && (p_fi->flags & 3)!=O_RDONLY) {       \
    LOG_DEBUG("Attempt to open the read-only file \"%s\" for writing.\n", \
              p_path)                                                     \
    return -EACCES;                                                       \
  }                                                                       \
  return 0;                                                               \
}

  if(strcmp(p_path,glob_xmount.output.p_virtual_image_path)==0 ||
     strcmp(p_path,glob_xmount.output.p_info_path)==0)
  {
    CHECK_OPEN_PERMS();
  }

#undef CHECK_OPEN_PERMS

  LOG_DEBUG("Attempt to open inexistant file \"%s\".\n",p_path);
  return -ENOENT;
}
*/

//! FUSE read implementation
/*!
 * \param p_path Path (relative to mount folder) of file to read data from
 * \param p_buf Pre-allocated buffer where read data should be written to
 * \param size Number of bytes to read
 * \param offset Offset to start reading at
 * \param p_fi: File info struct
 * \return Read bytes on success, negated error code on error
 */
/*
int Xmount_FuseRead(const char *p_path,
                    char *p_buf,
                    size_t size,
                    off_t offset,
                    struct fuse_file_info *p_fi)
{
  (void)p_fi;

  int ret;
  uint64_t len;

#define READ_MEM_FILE(filebuf,filesize,filetypestr,mutex) {                    \
  len=filesize;                                                                \
  if(offset<len) {                                                             \
    if(offset+size>len) {                                                      \
      LOG_DEBUG("Attempt to read past EOF of virtual " filetypestr " file\n"); \
      LOG_DEBUG("Adjusting read size from %u to %u\n",size,len-offset);        \
      size=len-offset;                                                         \
    }                                                                          \
    pthread_mutex_lock(&mutex);                                                \
    memcpy(p_buf,filebuf+offset,size);                                         \
    pthread_mutex_unlock(&mutex);                                              \
    LOG_DEBUG("Read %" PRIu64 " bytes at offset %" PRIu64                      \
              " from virtual " filetypestr " file\n",size,offset);             \
    ret=size;                                                                  \
  } else {                                                                     \
    LOG_DEBUG("Attempt to read behind EOF of virtual " filetypestr " file\n"); \
    ret=0;                                                                     \
  }                                                                            \
}

  if(strcmp(p_path,glob_xmount.output.p_virtual_image_path)==0) {
    // Read data from virtual output file
    // Wait for other threads to end reading/writing data
    pthread_mutex_lock(&(glob_xmount.mutex_image_rw));
    // Get requested data
    if((ret=ReadOutputImageData(p_buf,offset,size))<0) {
      LOG_ERROR("Couldn't read data from virtual image file!\n")
    }
    // Allow other threads to read/write data again
    pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
  } else if(strcmp(p_path,glob_xmount.output.p_info_path)==0) {
    // Read data from virtual info file
    READ_MEM_FILE(glob_xmount.output.p_info_file,
                  strlen(glob_xmount.output.p_info_file),
                  "info",
                  glob_xmount.mutex_info_read);
  } else {
    // Attempt to read non existant file
    LOG_DEBUG("Attempt to read from non existant file \"%s\"\n",p_path)
    ret=-ENOENT;
  }

#undef READ_MEM_FILE

  // TODO: Return size of read data!!!!!
  return ret;
}
*/

//! FUSE rename implementation
/*!
 * \param p_path File to rename
 * \param p_npath New filename
 * \return 0 on error, negated error code on error
 */
/*
int Xmount_FuseRename(const char *p_path, const char *p_npath) {
  // TODO: Implement
  return -ENOENT;
}
*/

//! FUSE rmdir implementation
/*!
 * \param p_path Directory to delete
 * \return 0 on success, negated error code on error
 */
/*
int Xmount_FuseRmDir(const char *p_path) {
  // TODO: Implement
  return -1;
}
*/

//! FUSE unlink implementation
/*!
 * \param p_path File to delete
 * \return 0 on success, negated error code on error
 */
/*
int Xmount_FuseUnlink(const char *p_path) {
  // TODO: Implement
  return -1;
}
*/

//! FUSE statfs implementation
/*!
 * \param p_path Get stats for fs that the specified file resides in
 * \param stats Stats
 * \return 0 on success, negated error code on error
 */
/*
int Xmount_FuseStatFs(const char *p_path, struct statvfs *stats) {
  struct statvfs CacheFileFsStats;
  int ret;

  if(glob_xmount.writable==TRUE) {
    // If write support is enabled, return stats of fs upon which cache file
    // resides in
    if((ret=statvfs(glob_xmount.p_cache_file,&CacheFileFsStats))==0) {
      memcpy(stats,&CacheFileFsStats,sizeof(struct statvfs));
      return 0;
    } else {
      LOG_ERROR("Couldn't get stats for fs upon which resides \"%s\"\n",
                glob_xmount.p_cache_file)
      return ret;
    }
  } else {
    // TODO: Return read only
    return 0;
  }
}
*/

// FUSE write implementation
/*!
 * \param p_buf Buffer containing data to write
 * \param size Number of bytes to write
 * \param offset Offset to start writing at
 * \param p_fi: File info struct
 *
 * Returns:
 *   Written bytes on success, negated error code on error
 */
/*
int Xmount_FuseWrite(const char *p_path,
                     const char *p_buf,
                     size_t size,
                     off_t offset,
                     struct fuse_file_info *p_fi)
{
  (void)p_fi;

  uint64_t len;

  if(strcmp(p_path,glob_xmount.output.p_virtual_image_path)==0) {
    // Wait for other threads to end reading/writing data
    pthread_mutex_lock(&(glob_xmount.mutex_image_rw));

    // Get output image file size
    if(!GetOutputImageSize(&len)) {
      LOG_ERROR("Couldn't get virtual image size!\n")
      pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
      return 0;
    }
    if(offset<len) {
      if(offset+size>len) size=len-offset;
      if(WriteOutputImageData(p_buf,offset,size)!=size) {
        LOG_ERROR("Couldn't write data to virtual image file!\n")
        pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
        return 0;
      }
    } else {
      LOG_DEBUG("Attempt to write past EOF of virtual image file\n")
      pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
      return 0;
    }

    // Allow other threads to read/write data again
    pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
  } else if(strcmp(p_path,glob_xmount.output.p_info_path)==0) {
    // Attempt to write data to read only image info file
    LOG_DEBUG("Attempt to write data to virtual info file\n");
    return -ENOENT;
  } else {
    // Attempt to write to non existant file
    LOG_DEBUG("Attempt to write to the non existant file \"%s\"\n",p_path)
    return -ENOENT;
  }

  return size;
}
*/
