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
 * FUSE function implementation
 ******************************************************************************/
//! FUSE access implementation
/*!
 * \param p_path Path of file to get attributes from
 * \param perm Requested permissisons
 * \return 0 on success, negated error code on error
 */
/*
int Xmount_FuseAccess(const char *path, int perm) {
  // TODO: Implement propper file permission handling
  // http://www.cs.cf.ac.uk/Dave/C/node20.html
  // Values for the second argument to access.
  // These may be OR'd together.
  //#define	R_OK	4		// Test for read permission.
  //#define	W_OK	2		// Test for write permission.
  //#define	X_OK	1		// Test for execute permission.
  //#define	F_OK	0		// Test for existence.
  return 0;
}
*/

//! FUSE getattr implementation
/*!
 * \param p_path Path of file to get attributes from
 * \param p_stat Pointer to stat structure to save attributes to
 * \return 0 on success, negated error code on error
 */
int Xmount_FuseGetAttr(const char *p_path, struct stat *p_stat) {
  te_XmountError xmount_ret=e_XmountError_None;

  memset(p_stat,0,sizeof(struct stat));
  if((xmount_ret=Xmount_GetFileAttr(p_path,p_stat))!=e_XmountError_None) {
    LOG_ERROR("Couldn't get file attributes: Error code %u!\n",xmount_ret);
    return -ENOENT;
  }
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
  return 0;
}

//! FUSE mkdir implementation
/*!
 * \param p_path Directory path
 * \param mode Directory permissions
 * \return 0 on success, negated error code on error
 */
int Xmount_FuseMkDir(const char *p_path, mode_t mode) {
  // TODO: Implement
  LOG_ERROR("Attempt to create directory \"%s\" "
            "on read-only filesystem!\n",p_path)
  return -1;
}

//! FUSE create implementation.
/*!
 * \param p_path File to create
 * \param mode File mode
 * \param dev ??? but not used
 * \return 0 on success, negated error code on error
 */
int Xmount_FuseMkNod(const char *p_path, mode_t mode, dev_t dev) {
  // TODO: Implement
  LOG_ERROR("Attempt to create illegal file \"%s\"\n",p_path)
  return -1;
}

//! FUSE readdir implementation
/*!
 * \param p_path Path from where files should be listed
 * \param p_buf Buffer to write file entrys to
 * \param filler Function to write dir entrys to buffer
 * \param offset ??? but not used
 * \param p_fi File info struct
 * \return 0 on success, negated error code on error
 */
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

//! FUSE open implementation
/*!
 * \param p_path Path to file to open
 * \param p_fi File info struct
 * \return 0 on success, negated error code on error
 */
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

//! FUSE read implementation
/*!
 * \param p_path Path (relative to mount folder) of file to read data from
 * \param p_buf Pre-allocated buffer where read data should be written to
 * \param size Number of bytes to read
 * \param offset Offset to start reading at
 * \param p_fi: File info struct
 * \return Read bytes on success, negated error code on error
 */
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

//! FUSE rename implementation
/*!
 * \param p_path File to rename
 * \param p_npath New filename
 * \return 0 on error, negated error code on error
 */
int Xmount_FuseRename(const char *p_path, const char *p_npath) {
  // TODO: Implement
  return -ENOENT;
}

//! FUSE rmdir implementation
/*!
 * \param p_path Directory to delete
 * \return 0 on success, negated error code on error
 */
int Xmount_FuseRmDir(const char *p_path) {
  // TODO: Implement
  return -1;
}

//! FUSE unlink implementation
/*!
 * \param p_path File to delete
 * \return 0 on success, negated error code on error
 */
int Xmount_FuseUnlink(const char *p_path) {
  // TODO: Implement
  return -1;
}

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
