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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "xmount_cache.h"
#include "xmount.h"
#include "macros.h"

/*******************************************************************************
 * Private definitions / macros
 ******************************************************************************/
#define LOG_WARNING(...) {            \
  LIBXMOUNT_LOG_WARNING(__VA_ARGS__); \
}
#define LOG_ERROR(...) {            \
  LIBXMOUNT_LOG_ERROR(__VA_ARGS__); \
}
#define LOG_DEBUG(...) {                              \
  LIBXMOUNT_LOG_DEBUG(glob_xmount.debug,__VA_ARGS__); \
}

#ifndef __LP64__
  //! Value used to indicate an uncached block entry
  #define XMOUNT_CACHE_INVALID_INDEX 0xFFFFFFFFFFFFFFFFLL
#else
  #define XMOUNT_CACHE_INVALID_INDEX 0xFFFFFFFFFFFFFFFF
#endif

//! Structures and vars needed for write access
#define XMOUNT_CACHE_FOLDER "/.xmount"
#define XMOUNT_CACHE_BLOCK_FILE XMOUNT_CACHE_FOLDER "/blocks.data"
#define XMOUNT_CACHE_BLOCK_INDEX_FILE XMOUNT_CACHE_FOLDER "/blocks.index"

/*******************************************************************************
 * Private types / structures / enums
 ******************************************************************************/
typedef struct s_XmountCacheHandle {
  //! Cache file to save changes to
  char *p_cache_file;
  //! Handle to cache file
  hGidaFs h_cache_file;
  //! Handle to block cache
  hGidaFsFile h_block_cache;
  //! Handle to block cache index
  hGidaFsFile h_block_cache_index;
  //! In-memory copy of cache index
  uint64_t *p_block_cache_index;
  //! Length (in elements) of in-memory block cache index
  uint64_t block_cache_index_len;
  // TODO: Move to s_XmountData
  //! Overwrite existing cache
  uint8_t overwrite_cache;
} ts_XmountCacheHandle, *pts_XmountCacheHandle;

/*******************************************************************************
 * Private functions declarations
 ******************************************************************************/
/*!
 * \brief Create a new xmount cache handle
 *
 * \param pp_h Pointer to an xmount cache handle
 * \param p_file Cache file path / name
 * \return e_XmountCache_Error_None on success
 */
te_XmountCache_Error XmountCache_CreateHandle(pts_XmountCacheHandle *pp_h,
                                              const char *p_file);

/*!
 * \brief Destroy an xmount cache handle
 *
 * \param pp_h Pointer to an xmount cache handle
 * \return e_XmountCache_Error_None on success
 */
te_XmountCache_Error XmountCache_DestroyHandle(pts_XmountCacheHandle *pp_h);

/*!
 * \brief Check if a file exists
 *
 * Checks if the given file p_file exists.
 *
 * \param p_file File to check for
 * \return e_XmountCache_Error_None if file exists
 */
te_XmountCache_Error XmountCache_FileExists(const char *p_file);

/*******************************************************************************
 * Public functions implementations
 ******************************************************************************/
/*
 * XmountCache_Create
 */
te_XmountCache_Error XmountCache_Create(pts_XmountCacheHandle *pp_h,
                                        const char *p_file,
                                        uint64_t image_size,
                                        uint8_t overwrite)
{
  pts_XmountCacheHandle p_h=NULL;
  teGidaFsError gidafs_ret=eGidaFsError_None;
  te_XmountCache_Error ret=e_XmountCache_Error_None;

  // Check params
  if(pp_h==NULL) return e_XmountCache_Error_InvalidHandlePointer;
  if(p_file==NULL) return e_XmountCache_Error_InvalidString;
  if(strlen(p_file)==0) return e_XmountCache_Error_InvalidFile;

  // Make sure file does not exist when overwrite was not specified
  if(overwrite==0 && XmountCache_FileExists(p_file)==e_XmountCache_Error_None) {
    // Given file exists and overwrite was not specified. This is fatal!
    return e_XmountCache_Error_ExistingFile;
  }

  // Create new handle
  ret=XmountCache_CreateHandle(&p_h,p_file);
  if(ret!=e_XmountCache_Error_None) return ret;

#define XMOUNTCACHE_CREATE__DESTROY_HANDLE do {                             \
  ret=XmountCache_DestroyHandle(&p_h);                                      \
  if(ret!=e_XmountCache_Error_None) {                                       \
    LOG_ERROR("Unable to destroy cache handle: Error code %u: Ignoring!\n", \
              ret);                                                         \
  }                                                                         \
  *pp_h=NULL;                                                               \
} while(0)

  // Create new cache file
  gidafs_ret=GidaFsLib_NewFs(&(p_h->h_cache_file),
                             p_h->p_cache_file,
                             0);
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to create new xmount cache file '%s': Error code %u!\n",
              p_h->p_cache_file,
              gidafs_ret);
    XMOUNTCACHE_CREATE__DESTROY_HANDLE;
    return FALSE;
  }

#define XMOUNTCACHE_CREATE__CLOSE_CACHE do {             \
  gidafs_ret=GidaFsLib_CloseFs(&(p_h->h_cache_file));    \
  if(gidafs_ret!=eGidaFsError_None) {                    \
    LOG_ERROR("Unable to close xmount cache file '%s': " \
                "Error code %u: Ignoring!\n",            \
              p_h->p_cache_file,                         \
              gidafs_ret);                               \
  }                                                      \
} while(0)

  // TODO: Check if cache file uses same block size as we do

  // Create needed xmount subdirectory
  gidafs_ret=GidaFsLib_CreateDir(p_h->h_cache_file,
                                 XMOUNT_CACHE_FOLDER,
                                 eGidaFsNodeFlag_RWXu);
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to create cache file directory '%s': Error code %u!\n",
              XMOUNT_CACHE_FOLDER,
              gidafs_ret);
    XMOUNTCACHE_CREATE__CLOSE_CACHE;
    XMOUNTCACHE_CREATE__DESTROY_HANDLE;
    return e_XmountCache_Error_FailedCacheInit;
  }

  // Create block cache file
  gidafs_ret=GidaFsLib_OpenFile(p_h->h_cache_file,
                                XMOUNT_CACHE_BLOCK_FILE,
                                &(p_h->h_block_cache),
                                eGidaFsOpenFileFlag_ReadWrite |
                                  eGidaFsOpenFileFlag_CreateAlways,
                                eGidaFsNodeFlag_Rall | eGidaFsNodeFlag_Wusr);
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to create block cache file '%s': Error code %u!\n",
              XMOUNT_CACHE_BLOCK_FILE,
              gidafs_ret);
    XMOUNTCACHE_CREATE__CLOSE_CACHE;
    XMOUNTCACHE_CREATE__DESTROY_HANDLE;
    return e_XmountCache_Error_FailedCacheInit;
  }

#define XMOUNTCACHE_CREATE__CLOSE_BLOCK_CACHE do {                \
  gidafs_ret=GidaFsLib_CloseFile(p_h->h_cache_file,               \
                                 &(p_h->h_block_cache));          \
  if(gidafs_ret!=eGidaFsError_None) {                             \
    LOG_ERROR("Unable to close block cache file: Error code %u: " \
                "Ignoring!\n",                                    \
              gidafs_ret);                                        \
  }                                                               \
} while(0)

  // Create block cache index file
  gidafs_ret=GidaFsLib_OpenFile(p_h->h_cache_file,
                                XMOUNT_CACHE_BLOCK_INDEX_FILE,
                                &(p_h->h_block_cache_index),
                                eGidaFsOpenFileFlag_ReadWrite |
                                  eGidaFsOpenFileFlag_CreateAlways,
                                eGidaFsNodeFlag_Rall | eGidaFsNodeFlag_Wusr);
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to create block cache index file '%s': Error code %u!\n",
              XMOUNT_CACHE_BLOCK_FILE,
              gidafs_ret);
    XMOUNTCACHE_CREATE__CLOSE_BLOCK_CACHE;
    XMOUNTCACHE_CREATE__CLOSE_CACHE;
    XMOUNTCACHE_CREATE__DESTROY_HANDLE;
    return e_XmountCache_Error_FailedCacheInit;
  }

#define XMOUNTCACHE_CREATE__CLOSE_BLOCK_CACHE_INDEX do {                \
  gidafs_ret=GidaFsLib_CloseFile(p_h->h_cache_file,                     \
                                 &(p_h->h_block_cache_index));          \
  if(gidafs_ret!=eGidaFsError_None) {                                   \
    LOG_ERROR("Unable to close block cache index file: Error code %u: " \
                "Ignoring!\n",                                          \
              gidafs_ret);                                              \
  }                                                                     \
} while(0)

  // Calculate how many cache blocks are needed and how big the cache block
  // index must be
  p_h->block_cache_index_len=image_size/XMOUNT_CACHE_BLOCK_SIZE;
  if((image_size%XMOUNT_CACHE_BLOCK_SIZE)!=0) p_h->block_cache_index_len++;

  LOG_DEBUG("Cache blocks: %" PRIu64 " entries using %" PRIu64 " bytes\n",
            p_h->block_cache_index_len,
            p_h->block_cache_index_len*sizeof(uint8_t));

  // Prepare in-memory buffer for block cache index
  p_h->p_block_cache_index=
    (uint64_t*)calloc(1,p_h->block_cache_index_len*sizeof(uint64_t));
  if(p_h->p_block_cache_index==NULL) {
    XMOUNTCACHE_CREATE__CLOSE_BLOCK_CACHE_INDEX;
    XMOUNTCACHE_CREATE__CLOSE_BLOCK_CACHE;
    XMOUNTCACHE_CREATE__CLOSE_CACHE;
    XMOUNTCACHE_CREATE__DESTROY_HANDLE;
    return e_XmountCache_Error_Alloc;
  }

  // Generate initial block cache index
  for(uint64_t i=0;i<p_h->block_cache_index_len;i++) {
    p_h->p_block_cache_index[i]=XMOUNT_CACHE_INVALID_INDEX;
  }

  // Write initial block cache index to cache file
  if(UpdateBlockCacheIndex(XMOUNT_BLOCK_CACHE_INVALID_INDEX,0)!=TRUE) {
    LOG_ERROR("Unable to generate initial block cache index file '%s': "
                "Error code %u!\n",
              XMOUNT_CACHE_BLOCK_FILE,
              gidafs_ret);
    XMOUNTCACHE_CREATE__CLOSE_BLOCK_CACHE_INDEX;
    XMOUNTCACHE_CREATE__CLOSE_BLOCK_CACHE;
    XMOUNTCACHE_CREATE__CLOSE_CACHE;
    XMOUNTCACHE_CREATE__DESTROY_HANDLE;
    return e_XmountCache_Error_FailedCacheInit;
  }

#undef XMOUNTCACHE_CREATE__CLOSE_BLOCK_CACHE_INDEX
#undef XMOUNTCACHE_CREATE__CLOSE_BLOCK_CACHE
#undef XMOUNTCACHE_CREATE__CLOSE_CACHE
#undef XMOUNTCACHE_CREATE__DESTROY_HANDLE

  *pp_h=p_h;
  return e_XmountCache_Error_None;
}

/*
 * XmountCache_Open
 */
te_XmountCache_Error XmountCache_Open(pts_XmountCacheHandle *pp_h,
                                      const char *p_file,
                                      uint64_t image_size)
{
  pts_XmountCacheHandle p_h=NULL;
  teGidaFsError gidafs_ret=eGidaFsError_None;
  te_XmountCache_Error ret=e_XmountCache_Error_None;

  // Check params
  if(pp_h==NULL) return e_XmountCache_Error_InvalidHandlePointer;
  if(p_file==NULL) return e_XmountCache_Error_InvalidString;
  if(strlen(p_file)==0) return e_XmountCache_Error_InvalidFile;

  // Make sure file exists
  if(XmountCache_FileExists(p_file)!=e_XmountCache_Error_None) {
    // Given file does not exist. This is fatal!
    return e_XmountCache_Error_InexistingFile;
  }

  // Create new handle
  ret=XmountCache_CreateHandle(&p_h,p_file);
  if(ret!=e_XmountCache_Error_None) return ret;

#define XMOUNTCACHE_OPEN__DESTROY_HANDLE do {                               \
  ret=XmountCache_DestroyHandle(&p_h);                                      \
  if(ret!=e_XmountCache_Error_None) {                                       \
    LOG_ERROR("Unable to destroy cache handle: Error code %u: Ignoring!\n", \
              ret);                                                         \
  }                                                                         \
  *pp_h=NULL;                                                               \
} while(0)

  // Open cache file
  gidafs_ret=GidaFsLib_OpenFs(&(p_h->h_cache_file),p_h->p_cache_file);
  if(gidafs_ret!=eGidaFsError_None) {
    // TODO: Check for old cache file type and inform user it isn't supported
    // anymore!
    LOG_ERROR("Couldn't open xmount cache file '%s': Error code %u!\n",
              p_h->p_cache_file,
              gidafs_ret)
    return e_XmountCache_Error_FailedOpeningCache;
  }

#define XMOUNTCACHE_OPEN__CLOSE_CACHE do {               \
  gidafs_ret=GidaFsLib_CloseFs(&(p_h->h_cache_file));    \
  if(gidafs_ret!=eGidaFsError_None) {                    \
    LOG_ERROR("Unable to close xmount cache file '%s': " \
                "Error code %u: Ignoring!\n",            \
              p_h->p_cache_file,                         \
              gidafs_ret);                               \
  }                                                      \
} while(0)

#define XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE do {                  \
  gidafs_ret=GidaFsLib_CloseFile(p_h->h_cache_file,               \
                                 &(p_h->h_block_cache));          \
  if(gidafs_ret!=eGidaFsError_None) {                             \
    LOG_ERROR("Unable to close block cache file: Error code %u: " \
                "Ignoring!\n",                                    \
              gidafs_ret);                                        \
  }                                                               \
} while(0)

#define XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE_INDEX do {                  \
  gidafs_ret=GidaFsLib_CloseFile(p_h->h_cache_file,                     \
                                 &(p_h->h_block_cache_index));          \
  if(gidafs_ret!=eGidaFsError_None) {                                   \
    LOG_ERROR("Unable to close block cache index file: Error code %u: " \
                "Ignoring!\n",                                          \
              gidafs_ret);                                              \
  }                                                                     \
} while(0)









/*


#define INITCACHEFILE__CLOSE_CACHE do {                                 \
  gidafs_ret=GidaFsLib_CloseFs(&(glob_xmount.cache.h_cache_file));      \
  if(gidafs_ret!=eGidaFsError_None) {                                   \
    LOG_ERROR("Unable to close cache file: Error code %u: Ignoring!\n", \
              gidafs_ret)                                               \
  }                                                                     \
} while(0)

#define INITCACHEFILE__CLOSE_BLOCK_CACHE do {                                 \
  gidafs_ret=GidaFsLib_CloseFile(glob_xmount.cache.h_cache_file,              \
                                 &(glob_xmount.cache.h_block_cache));         \
  if(gidafs_ret!=eGidaFsError_None) {                                         \
    LOG_ERROR("Unable to close block cache file: Error code %u: Ignoring!\n", \
              gidafs_ret)                                                     \
  }                                                                           \
} while(0)

#define INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX do {                         \
  gidafs_ret=GidaFsLib_CloseFile(glob_xmount.cache.h_cache_file,            \
                                 &(glob_xmount.cache.h_block_cache_index)); \
  if(gidafs_ret!=eGidaFsError_None) {                                       \
    LOG_ERROR("Unable to close block cache index file: Error code %u: "     \
                "Ignoring!\n",                                              \
              gidafs_ret)                                                   \
  }                                                                         \
} while(0)

  // TODO: Check if cache file uses same block size as we do

  if(is_new_cache_file==1) {
    // New cache file, create needed xmount subdirectory
    gidafs_ret=GidaFsLib_CreateDir(glob_xmount.cache.h_cache_file,
                                   XMOUNT_CACHE_FOLDER,
                                   eGidaFsNodeFlag_RWXu);
    if(gidafs_ret!=eGidaFsError_None) {
      LOG_ERROR("Unable to create cache file directory '%s': Error code %u!\n",
                XMOUNT_CACHE_FOLDER,
                gidafs_ret)
      INITCACHEFILE__CLOSE_CACHE;
      return FALSE;
    }
  }

  // Open / Create block cache file
  gidafs_ret=GidaFsLib_OpenFile(glob_xmount.cache.h_cache_file,
                                XMOUNT_CACHE_BLOCK_FILE,
                                &(glob_xmount.cache.h_block_cache),
                                eGidaFsOpenFileFlag_ReadWrite |
                                  (is_new_cache_file==1 ?
                                    eGidaFsOpenFileFlag_CreateAlways : 0),
                                eGidaFsNodeFlag_Rall |
                                  eGidaFsNodeFlag_Wusr);
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to open / create block cache file '%s': Error code %u!\n",
              XMOUNT_CACHE_BLOCK_FILE,
              gidafs_ret)
    INITCACHEFILE__CLOSE_CACHE;
    return FALSE;
  }

  // Open / Create block cache index file
  gidafs_ret=GidaFsLib_OpenFile(glob_xmount.cache.h_cache_file,
                                XMOUNT_CACHE_BLOCK_INDEX_FILE,
                                &(glob_xmount.cache.h_block_cache_index),
                                eGidaFsOpenFileFlag_ReadWrite |
                                  (is_new_cache_file==1 ?
                                    eGidaFsOpenFileFlag_CreateAlways : 0),
                                eGidaFsNodeFlag_Rall |
                                  eGidaFsNodeFlag_Wusr);
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to open / create block cache index file '%s': "
                "Error code %u!\n",
              XMOUNT_CACHE_BLOCK_FILE,
              gidafs_ret)
    INITCACHEFILE__CLOSE_BLOCK_CACHE;
    INITCACHEFILE__CLOSE_CACHE;
    return FALSE;
  }

  // Calculate how many cache blocks are needed and how big the cache block
  // index must be
  glob_xmount.cache.block_cache_index_len=image_size/CACHE_BLOCK_SIZE;
  if((image_size%CACHE_BLOCK_SIZE)!=0) {
    glob_xmount.cache.block_cache_index_len++;
  }

  LOG_DEBUG("Cache blocks: %u (0x%04X) entries using %zd (0x%08zX) bytes\n",
            glob_xmount.cache.block_cache_index_len,
            glob_xmount.cache.block_cache_index_len,
            glob_xmount.cache.block_cache_index_len*
              sizeof(t_CacheFileBlockIndex),
            glob_xmount.cache.block_cache_index_len*
              sizeof(t_CacheFileBlockIndex))

  // Prepare in-memory buffer for block cache index
  XMOUNT_MALLOC(glob_xmount.cache.p_block_cache_index,
                t_CacheFileBlockIndex*,
                glob_xmount.cache.block_cache_index_len*
                  sizeof(t_CacheFileBlockIndex))

  if(is_new_cache_file==1) {
    // Generate initial block cache index
    for(uint64_t i=0;i<glob_xmount.cache.block_cache_index_len;i++) {
      glob_xmount.cache.p_block_cache_index[i]=CACHE_BLOCK_FREE;
    }
    // Write initial block cache index to cache file
    if(UpdateBlockCacheIndex(XMOUNT_BLOCK_CACHE_INVALID_INDEX,0)!=TRUE) {
      LOG_ERROR("Unable to generate initial block cache index file '%s': "
                  "Error code %u!\n",
                XMOUNT_CACHE_BLOCK_FILE,
                gidafs_ret)
      INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX;
      INITCACHEFILE__CLOSE_BLOCK_CACHE;
      INITCACHEFILE__CLOSE_CACHE;
      XMOUNT_FREE(glob_xmount.cache.p_block_cache_index);
      return FALSE;
    }
  } else {
    // Existing cache file, make sure block cache index has correct size
    gidafs_ret=GidaFsLib_GetFileSize(glob_xmount.cache.h_cache_file,
                                     glob_xmount.cache.h_block_cache_index,
                                     &blockindex_size);
    if(gidafs_ret!=eGidaFsError_None) {
      LOG_ERROR("Unable to get block cache index file size: Error code %u!\n",
                gidafs_ret)
      INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX;
      INITCACHEFILE__CLOSE_BLOCK_CACHE;
      INITCACHEFILE__CLOSE_CACHE;
      XMOUNT_FREE(glob_xmount.cache.p_block_cache_index);
      return FALSE;
    }
    if(blockindex_size%sizeof(t_CacheFileBlockIndex)!=0 ||
       (blockindex_size/sizeof(t_CacheFileBlockIndex))!=
         glob_xmount.cache.block_cache_index_len)
    {
      // TODO: Be more helpfull in error message
      LOG_ERROR("Block cache index size is incorrect for given input image!\n")
      INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX;
      INITCACHEFILE__CLOSE_BLOCK_CACHE;
      INITCACHEFILE__CLOSE_CACHE;
      XMOUNT_FREE(glob_xmount.cache.p_block_cache_index);
      return FALSE;
    }
    // Read block cache index into memory
    gidafs_ret=GidaFsLib_ReadFile(glob_xmount.cache.h_cache_file,
                                  glob_xmount.cache.h_block_cache_index,
                                  0,
                                  blockindex_size,
                                  glob_xmount.cache.p_block_cache_index,
                                  &read);
    if(gidafs_ret!=eGidaFsError_None || read!=blockindex_size) {
      LOG_ERROR("Unable to read block cache index: Error code %u!\n",
                gidafs_ret);
      INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX;
      INITCACHEFILE__CLOSE_BLOCK_CACHE;
      INITCACHEFILE__CLOSE_CACHE;
      XMOUNT_FREE(glob_xmount.cache.p_block_cache_index);
      return FALSE;
    }
  }

#undef INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX
#undef INITCACHEFILE__CLOSE_BLOCK_CACHE
#undef INITCACHEFILE__CLOSE_CACHE
*/

#undef XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE_INDEX
#undef XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE
#undef XMOUNTCACHE_OPEN__CLOSE_CACHE
#undef XMOUNTCACHE_OPEN__DESTROY_HANDLE

  *pp_h=p_h;
  return e_XmountCache_Error_None;
}

/*
 * XmountCache_Close
 */
te_XmountCache_Error XmountCache_Close(pts_XmountCacheHandle *pp_h) {
  pts_XmountCacheHandle p_h=NULL;
  te_XmountCache_Error ret=e_XmountCache_Error_None;



  *pp_h=NULL;
  return e_XmountCache_Error_None;
}

/*
 * XmountCache_BlockCacheRead
 */
te_XmountCache_Error XmountCache_BlockCacheRead(pts_XmountCacheHandle p_h,
                                                char *p_buf,
                                                uint64_t block,
                                                uint64_t block_offset,
                                                uint64_t count)
{
  te_XmountCache_Error ret=e_XmountCache_Error_None;
}

/*
 * XmountCache_BlockCacheWrite
 */
te_XmountCache_Error XmountCache_BlockCacheWrite(pts_XmountCacheHandle p_h,
                                                 char *p_buf,
                                                 uint64_t block,
                                                 uint64_t block_offset,
                                                 uint64_t count)
{
  te_XmountCache_Error ret=e_XmountCache_Error_None;
}

/*
 * XmountCache_BlockCacheAppend
 */
te_XmountCache_Error XmountCache_BlockCacheAppend(pts_XmountCacheHandle p_h,
                                                  char *p_buf,
                                                  uint64_t block)
{
  te_XmountCache_Error ret=e_XmountCache_Error_None;
}

/*
 * XmountCache_IsBlockCached
 */
te_XmountCache_Error XmountCache_IsBlockCached(pts_XmountCacheHandle p_h,
                                               uint64_t block)
{
  te_XmountCache_Error ret=e_XmountCache_Error_None;
}

/*******************************************************************************
 * Private functions implementations
 ******************************************************************************/
/*
 * XmountCache_CreateHandle
 */
te_XmountCache_Error XmountCache_CreateHandle(pts_XmountCacheHandle *pp_h,
                                              const char *p_file)
{
  pts_XmountCacheHandle p_h=NULL;

  // Check given handle pointer
  if(pp_h==NULL) return e_XmountCache_Error_InvalidHandlePointer;

  // Alloc memory for handle
  p_h=(pts_XmountCacheHandle)calloc(1,sizeof(ts_XmountCacheHandle));
  if(p_h==NULL) {
    *pp_h=NULL;
    return e_XmountCache_Error_Alloc;
  }

  // Init values. The p_cache_file alloc and memcpy works corrently as strlen()
  // counts the amount of bytes, not chars. No UTF8-issue here.
  p_h->p_cache_file=(char*)calloc(1,strlen(p_file)+1);
  if(p_h->p_cache_file==NULL) {
    free(p_h);
    *pp_h=NULL;
    return e_XmountCache_Error_Alloc;
  }
  memcpy(p_h->p_cache_file,p_file,strlen(p_file));
  p_h->h_cache_file=NULL;
  p_h->h_block_cache=NULL;
  p_h->h_block_cache_index=NULL;
  p_h->p_block_cache_index=NULL;

  // Return new handle
  *pp_h=p_h;
  return e_XmountCache_Error_None;
}

/*
 * XmountCache_DestroyHandle
 */
te_XmountCache_Error XmountCache_DestroyHandle(pts_XmountCacheHandle *pp_h) {
  pts_XmountCacheHandle p_h=NULL;

  // Check given handle pointer
  if(pp_h==NULL) return e_XmountCache_Error_InvalidHandlePointer;
  if(*pp_h==NULL) return e_XmountCache_Error_InvalidHandle;
  p_h=*pp_h;

  // Free handle
  if(p_h->p_cache_file!=NULL) free(p_h->p_cache_file);
  if(p_h->p_block_cache_index!=NULL) free(p_h->p_block_cache_index);
  free(p_h);

  // Return destroyed handle
  *pp_h=NULL;
  return e_XmountCache_Error_None;
}

/*
 * XmountCache_FileExists
 */
te_XmountCache_Error XmountCache_FileExists(const char *p_file) {
  struct stat statbuf;

  if(lstat(p_file,&statbuf)==0) return e_XmountCache_Error_None;
  else return e_XmountCache_Error_InexistingFile;
}






















//! Create / load cache file to enable virtual write support
/*!
 * \return TRUE on success, FALSE on error
 */
int InitCacheFile() {
  uint64_t blockindex_size=0;
  uint64_t image_size=0;
  uint64_t read=0;
  uint8_t is_new_cache_file=0;
  teGidaFsError gidafs_ret=eGidaFsError_None;

  // Get input image size for later use
  if(!GetMorphedImageSize(&image_size)) {
    LOG_ERROR("Couldn't get morphed image size!\n")
    return FALSE;
  }

  if(!glob_xmount.cache.overwrite_cache) {
    // Try to open an existing cache file or create a new one
    gidafs_ret=GidaFsLib_OpenFs(&(glob_xmount.cache.h_cache_file),
                                glob_xmount.cache.p_cache_file);
    if(gidafs_ret!=eGidaFsError_None &&
       gidafs_ret!=eGidaFsError_FailedOpeningFsFile)
    {
      // TODO: Check for old cache file type and inform user it isn't supported
      // anymore!
      LOG_ERROR("Couldn't open cache file '%s': Error code %u!\n",
                glob_xmount.cache.p_cache_file,
                gidafs_ret)
      return FALSE;
    } else if(gidafs_ret==eGidaFsError_FailedOpeningFsFile) {
      // Unable to open cache file. It might simply not exist.
      LOG_DEBUG("Cache file '%s' does not exist. Creating new one\n",
                glob_xmount.cache.p_cache_file)
      gidafs_ret=GidaFsLib_NewFs(&(glob_xmount.cache.h_cache_file),
                                 glob_xmount.cache.p_cache_file,
                                 0);
      if(gidafs_ret!=eGidaFsError_None) {
        // There is really a problem opening/creating the file
        LOG_ERROR("Couldn't open cache file '%s': Error code %u!\n",
                  glob_xmount.cache.p_cache_file,
                  gidafs_ret)
        return FALSE;
      }
      is_new_cache_file=1;
    }
  } else {
    // Overwrite existing cache file or create a new one
    gidafs_ret=GidaFsLib_NewFs(&(glob_xmount.cache.h_cache_file),
                               glob_xmount.cache.p_cache_file,
                               0);
    if(gidafs_ret!=eGidaFsError_None) {
      // There is really a problem opening/creating the file
      LOG_ERROR("Couldn't open cache file '%s': Error code %u!\n",
                glob_xmount.cache.p_cache_file,
                gidafs_ret)
      return FALSE;
    }
    is_new_cache_file=1;
  }

#define INITCACHEFILE__CLOSE_CACHE do {                                 \
  gidafs_ret=GidaFsLib_CloseFs(&(glob_xmount.cache.h_cache_file));      \
  if(gidafs_ret!=eGidaFsError_None) {                                   \
    LOG_ERROR("Unable to close cache file: Error code %u: Ignoring!\n", \
              gidafs_ret)                                               \
  }                                                                     \
} while(0)

#define INITCACHEFILE__CLOSE_BLOCK_CACHE do {                                 \
  gidafs_ret=GidaFsLib_CloseFile(glob_xmount.cache.h_cache_file,              \
                                 &(glob_xmount.cache.h_block_cache));         \
  if(gidafs_ret!=eGidaFsError_None) {                                         \
    LOG_ERROR("Unable to close block cache file: Error code %u: Ignoring!\n", \
              gidafs_ret)                                                     \
  }                                                                           \
} while(0)

#define INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX do {                         \
  gidafs_ret=GidaFsLib_CloseFile(glob_xmount.cache.h_cache_file,            \
                                 &(glob_xmount.cache.h_block_cache_index)); \
  if(gidafs_ret!=eGidaFsError_None) {                                       \
    LOG_ERROR("Unable to close block cache index file: Error code %u: "     \
                "Ignoring!\n",                                              \
              gidafs_ret)                                                   \
  }                                                                         \
} while(0)

  // TODO: Check if cache file uses same block size as we do

  if(is_new_cache_file==1) {
    // New cache file, create needed xmount subdirectory
    gidafs_ret=GidaFsLib_CreateDir(glob_xmount.cache.h_cache_file,
                                   XMOUNT_CACHE_FOLDER,
                                   eGidaFsNodeFlag_RWXu);
    if(gidafs_ret!=eGidaFsError_None) {
      LOG_ERROR("Unable to create cache file directory '%s': Error code %u!\n",
                XMOUNT_CACHE_FOLDER,
                gidafs_ret)
      INITCACHEFILE__CLOSE_CACHE;
      return FALSE;
    }
  }

  // Open / Create block cache file
  gidafs_ret=GidaFsLib_OpenFile(glob_xmount.cache.h_cache_file,
                                XMOUNT_CACHE_BLOCK_FILE,
                                &(glob_xmount.cache.h_block_cache),
                                eGidaFsOpenFileFlag_ReadWrite |
                                  (is_new_cache_file==1 ?
                                    eGidaFsOpenFileFlag_CreateAlways : 0),
                                eGidaFsNodeFlag_Rall |
                                  eGidaFsNodeFlag_Wusr);
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to open / create block cache file '%s': Error code %u!\n",
              XMOUNT_CACHE_BLOCK_FILE,
              gidafs_ret)
    INITCACHEFILE__CLOSE_CACHE;
    return FALSE;
  }

  // Open / Create block cache index file
  gidafs_ret=GidaFsLib_OpenFile(glob_xmount.cache.h_cache_file,
                                XMOUNT_CACHE_BLOCK_INDEX_FILE,
                                &(glob_xmount.cache.h_block_cache_index),
                                eGidaFsOpenFileFlag_ReadWrite |
                                  (is_new_cache_file==1 ?
                                    eGidaFsOpenFileFlag_CreateAlways : 0),
                                eGidaFsNodeFlag_Rall |
                                  eGidaFsNodeFlag_Wusr);
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to open / create block cache index file '%s': "
                "Error code %u!\n",
              XMOUNT_CACHE_BLOCK_FILE,
              gidafs_ret)
    INITCACHEFILE__CLOSE_BLOCK_CACHE;
    INITCACHEFILE__CLOSE_CACHE;
    return FALSE;
  }

  // Calculate how many cache blocks are needed and how big the cache block
  // index must be
  glob_xmount.cache.block_cache_index_len=image_size/CACHE_BLOCK_SIZE;
  if((image_size%CACHE_BLOCK_SIZE)!=0) {
    glob_xmount.cache.block_cache_index_len++;
  }

  LOG_DEBUG("Cache blocks: %u (0x%04X) entries using %zd (0x%08zX) bytes\n",
            glob_xmount.cache.block_cache_index_len,
            glob_xmount.cache.block_cache_index_len,
            glob_xmount.cache.block_cache_index_len*
              sizeof(t_CacheFileBlockIndex),
            glob_xmount.cache.block_cache_index_len*
              sizeof(t_CacheFileBlockIndex))

  // Prepare in-memory buffer for block cache index
  XMOUNT_MALLOC(glob_xmount.cache.p_block_cache_index,
                t_CacheFileBlockIndex*,
                glob_xmount.cache.block_cache_index_len*
                  sizeof(t_CacheFileBlockIndex))

  if(is_new_cache_file==1) {
    // Generate initial block cache index
    for(uint64_t i=0;i<glob_xmount.cache.block_cache_index_len;i++) {
      glob_xmount.cache.p_block_cache_index[i]=CACHE_BLOCK_FREE;
    }
    // Write initial block cache index to cache file
    if(UpdateBlockCacheIndex(XMOUNT_BLOCK_CACHE_INVALID_INDEX,0)!=TRUE) {
      LOG_ERROR("Unable to generate initial block cache index file '%s': "
                  "Error code %u!\n",
                XMOUNT_CACHE_BLOCK_FILE,
                gidafs_ret)
      INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX;
      INITCACHEFILE__CLOSE_BLOCK_CACHE;
      INITCACHEFILE__CLOSE_CACHE;
      XMOUNT_FREE(glob_xmount.cache.p_block_cache_index);
      return FALSE;
    }
  } else {
    // Existing cache file, make sure block cache index has correct size
    gidafs_ret=GidaFsLib_GetFileSize(glob_xmount.cache.h_cache_file,
                                     glob_xmount.cache.h_block_cache_index,
                                     &blockindex_size);
    if(gidafs_ret!=eGidaFsError_None) {
      LOG_ERROR("Unable to get block cache index file size: Error code %u!\n",
                gidafs_ret)
      INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX;
      INITCACHEFILE__CLOSE_BLOCK_CACHE;
      INITCACHEFILE__CLOSE_CACHE;
      XMOUNT_FREE(glob_xmount.cache.p_block_cache_index);
      return FALSE;
    }
    if(blockindex_size%sizeof(t_CacheFileBlockIndex)!=0 ||
       (blockindex_size/sizeof(t_CacheFileBlockIndex))!=
         glob_xmount.cache.block_cache_index_len)
    {
      // TODO: Be more helpfull in error message
      LOG_ERROR("Block cache index size is incorrect for given input image!\n")
      INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX;
      INITCACHEFILE__CLOSE_BLOCK_CACHE;
      INITCACHEFILE__CLOSE_CACHE;
      XMOUNT_FREE(glob_xmount.cache.p_block_cache_index);
      return FALSE;
    }
    // Read block cache index into memory
    gidafs_ret=GidaFsLib_ReadFile(glob_xmount.cache.h_cache_file,
                                  glob_xmount.cache.h_block_cache_index,
                                  0,
                                  blockindex_size,
                                  glob_xmount.cache.p_block_cache_index,
                                  &read);
    if(gidafs_ret!=eGidaFsError_None || read!=blockindex_size) {
      LOG_ERROR("Unable to read block cache index: Error code %u!\n",
                gidafs_ret);
      INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX;
      INITCACHEFILE__CLOSE_BLOCK_CACHE;
      INITCACHEFILE__CLOSE_CACHE;
      XMOUNT_FREE(glob_xmount.cache.p_block_cache_index);
      return FALSE;
    }
  }

#undef INITCACHEFILE__CLOSE_BLOCK_CACHE_INDEX
#undef INITCACHEFILE__CLOSE_BLOCK_CACHE
#undef INITCACHEFILE__CLOSE_CACHE

  return TRUE;
}

//! Update block cache index
/*!
 * \return TRUE on success, FALSE on error
 */
int UpdateBlockCacheIndex(uint64_t index, t_CacheFileBlockIndex value) {
  uint64_t update_size=0;
  uint64_t written=0;
  teGidaFsError gidafs_ret=eGidaFsError_None;
  t_CacheFileBlockIndex *p_buf;

  if(index!=XMOUNT_BLOCK_CACHE_INVALID_INDEX) {
    // Update specific index element in cache file
    p_buf=glob_xmount.cache.p_block_cache_index+index;
    update_size=sizeof(t_CacheFileBlockIndex);
  } else {
    // Dump whole block cache index to cache file
    p_buf=glob_xmount.cache.p_block_cache_index;
    update_size=glob_xmount.cache.block_cache_index_len*
                  sizeof(t_CacheFileBlockIndex);
  }

  // Update cache file
  gidafs_ret=GidaFsLib_WriteFile(glob_xmount.cache.h_cache_file,
                                 glob_xmount.cache.h_block_cache_index,
                                 0,
                                 update_size,
                                 p_buf,
                                 &written);
  if(gidafs_ret!=eGidaFsError_None || written!=update_size) {
    LOG_ERROR("Unable to update block cache index file '%s': "
                "Error code %u!\n",
              XMOUNT_CACHE_BLOCK_INDEX_FILE,
              gidafs_ret)
    return FALSE;
  }

  return TRUE;
}
