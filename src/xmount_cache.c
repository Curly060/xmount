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

#include <stdlib.h> // For calloc
#include <string.h> // For memcpy
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

/*!
 * \brief Updates a block cache entry
 *
 * Updates the given block cache index entry on disk. If the special value
 * XMOUNT_CACHE_INVALID_INDEX is given as entry value, the whole block cache
 * index is updated.
 *
 * \param p_h Cache handle
 * \param entry Block cache entry number to update
 * \return e_XmountCache_Error_None on success
 */
te_XmountCache_Error XmountCache_UpdateIndex(pts_XmountCacheHandle p_h,
                                             uint64_t entry);

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

  // Params check
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
    // TODO: Own error code needed here
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
    // TODO: Own error code needed here
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
  ret=XmountCache_UpdateIndex(p_h,XMOUNT_CACHE_INVALID_INDEX);
  if(ret!=e_XmountCache_Error_None) {
    LOG_ERROR("Unable to update initial block cache index file: "
                "Error code %u!\n",
              XMOUNT_CACHE_BLOCK_FILE,
              ret);
    XMOUNTCACHE_CREATE__CLOSE_BLOCK_CACHE_INDEX;
    XMOUNTCACHE_CREATE__CLOSE_BLOCK_CACHE;
    XMOUNTCACHE_CREATE__CLOSE_CACHE;
    XMOUNTCACHE_CREATE__DESTROY_HANDLE;
    return ret;
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
  uint64_t blockindex_size=0;
  uint64_t read=0;
  pts_XmountCacheHandle p_h=NULL;
  teGidaFsError gidafs_ret=eGidaFsError_None;
  te_XmountCache_Error ret=e_XmountCache_Error_None;

  // Params check
  if(pp_h==NULL) return e_XmountCache_Error_InvalidHandlePointer;
  if(p_file==NULL) return e_XmountCache_Error_InvalidString;
  if(strlen(p_file)==0) return e_XmountCache_Error_InvalidFile;

  // If file does not exist, create it
  if(XmountCache_FileExists(p_file)!=e_XmountCache_Error_None) {
    // Given file does not exist. Call XmountCache_Create instead!
    return XmountCache_Create(pp_h,p_file,image_size,0);
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

  // Open block cache file
  gidafs_ret=GidaFsLib_OpenFile(p_h->h_cache_file,
                                XMOUNT_CACHE_BLOCK_FILE,
                                &(p_h->h_block_cache),
                                eGidaFsOpenFileFlag_ReadWrite,
                                0);
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to open block cache file '%s': Error code %u!\n",
              XMOUNT_CACHE_BLOCK_FILE,
              gidafs_ret);
    XMOUNTCACHE_OPEN__CLOSE_CACHE;
    XMOUNTCACHE_OPEN__DESTROY_HANDLE;
    // TODO: Own error code needed here
    return e_XmountCache_Error_FailedOpeningCache;
  }

#define XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE do {                  \
  gidafs_ret=GidaFsLib_CloseFile(p_h->h_cache_file,               \
                                 &(p_h->h_block_cache));          \
  if(gidafs_ret!=eGidaFsError_None) {                             \
    LOG_ERROR("Unable to close block cache file: Error code %u: " \
                "Ignoring!\n",                                    \
              gidafs_ret);                                        \
  }                                                               \
} while(0)

  // Open block cache index file
  gidafs_ret=GidaFsLib_OpenFile(p_h->h_cache_file,
                                XMOUNT_CACHE_BLOCK_INDEX_FILE,
                                &(p_h->h_block_cache_index),
                                eGidaFsOpenFileFlag_ReadWrite,
                                0);
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to open block cache index file '%s': Error code %u!\n",
              XMOUNT_CACHE_BLOCK_FILE,
              gidafs_ret);
    XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE;
    XMOUNTCACHE_OPEN__CLOSE_CACHE;
    XMOUNTCACHE_OPEN__DESTROY_HANDLE;
    // TODO: Own error code needed here
    return e_XmountCache_Error_FailedOpeningCache;
  }

#define XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE_INDEX do {                  \
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

  // Make sure block cache index has correct size
  gidafs_ret=GidaFsLib_GetFileSize(p_h->h_cache_file,
                                   p_h->h_block_cache_index,
                                   &blockindex_size);
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to get block cache index file size: Error code %u!\n",
              gidafs_ret)
    XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE_INDEX;
    XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE;
    XMOUNTCACHE_OPEN__CLOSE_CACHE;
    XMOUNTCACHE_OPEN__DESTROY_HANDLE;
    return e_XmountCache_Error_FailedGettingIndexSize;
  }
  if(blockindex_size%sizeof(uint64_t)!=0 ||
     (blockindex_size/sizeof(uint64_t))!=p_h->block_cache_index_len)
  {
    // TODO: Be more helpfull in error message
    LOG_ERROR("Block cache index size is incorrect for given input image!\n")
    XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE_INDEX;
    XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE;
    XMOUNTCACHE_OPEN__CLOSE_CACHE;
    XMOUNTCACHE_OPEN__DESTROY_HANDLE;
    return e_XmountCache_Error_InvalidIndexSize;
  }

  // Prepare in-memory buffer for block cache index
  p_h->p_block_cache_index=
    (uint64_t*)calloc(1,p_h->block_cache_index_len*sizeof(uint64_t));
  if(p_h->p_block_cache_index==NULL) {
    XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE_INDEX;
    XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE;
    XMOUNTCACHE_OPEN__CLOSE_CACHE;
    XMOUNTCACHE_OPEN__DESTROY_HANDLE;
    return e_XmountCache_Error_Alloc;
  }

  // Read block cache index into memory
  gidafs_ret=GidaFsLib_ReadFile(p_h->h_cache_file,
                                p_h->h_block_cache_index,
                                0,
                                blockindex_size,
                                p_h->p_block_cache_index,
                                &read);
  if(gidafs_ret!=eGidaFsError_None || read!=blockindex_size) {
    LOG_ERROR("Unable to read block cache index: Error code %u!\n",
              gidafs_ret);
    XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE_INDEX;
    XMOUNTCACHE_OPEN__CLOSE_BLOCK_CACHE;
    XMOUNTCACHE_OPEN__CLOSE_CACHE;
    XMOUNTCACHE_OPEN__DESTROY_HANDLE;
    return e_XmountCache_Error_FailedReadingIndex;
  }

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
  teGidaFsError gidafs_ret=eGidaFsError_None;
  te_XmountCache_Error final_ret=e_XmountCache_Error_None;
  te_XmountCache_Error ret=e_XmountCache_Error_None;

  // Params check
  if(pp_h==NULL) return e_XmountCache_Error_InvalidHandlePointer;
  if(*pp_h==NULL) return e_XmountCache_Error_InvalidHandle;
  p_h=*pp_h;

  // Close block cache index
  gidafs_ret=GidaFsLib_CloseFile(p_h->h_cache_file,&(p_h->h_block_cache_index));
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to close block cache index file: Error code %u: "
                "Ignoring!\n",
              gidafs_ret);
    if(final_ret==e_XmountCache_Error_None) {
      final_ret=e_XmountCache_Error_FailedClosingIndex;
    }
  }

  // Close block cache
  gidafs_ret=GidaFsLib_CloseFile(p_h->h_cache_file,&(p_h->h_block_cache));
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to close block cache file: Error code %u: "
                "Ignoring!\n",
              gidafs_ret);
    if(final_ret==e_XmountCache_Error_None) {
      final_ret=e_XmountCache_Error_FailedClosingBlockCache;
    }
  }

  // Close cache file
  gidafs_ret=GidaFsLib_CloseFs(&(p_h->h_cache_file));
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to close xmount cache file '%s': Error code %u: "
                "Ignoring!\n",
              p_h->p_cache_file,
              gidafs_ret);
    if(final_ret==e_XmountCache_Error_None) {
      final_ret=e_XmountCache_Error_FailedClosingCache;
    }
  }

  // Destroy handle
  ret=XmountCache_DestroyHandle(&p_h);
  if(ret!=e_XmountCache_Error_None) {
    LOG_ERROR("Unable to destroy cache handle: Error code %u: Ignoring!\n",
              ret);
    if(final_ret==e_XmountCache_Error_None) {
      final_ret=ret;
    }
  }

  *pp_h=NULL;
  return final_ret;
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
  uint64_t read=0;
  teGidaFsError gidafs_ret=eGidaFsError_None;

  // Params check
  if(p_h==NULL) return e_XmountCache_Error_InvalidHandle;
  if(p_buf==NULL) return e_XmountCache_Error_InvalidBuffer;
  if(block>=p_h->block_cache_index_len) return e_XmountCache_Error_InvalidIndex;
  if(block_offset>XMOUNT_CACHE_BLOCK_SIZE ||
     block_offset+count>XMOUNT_CACHE_BLOCK_SIZE)
  {
    return e_XmountCache_Error_ReadBeyondBlockBounds;
  }
  if(p_h->p_block_cache_index[block]==XMOUNT_CACHE_INVALID_INDEX) {
    return e_XmountCache_Error_UncachedBlock;
  }

  // Read requested data from block cache file
  gidafs_ret=GidaFsLib_ReadFile(p_h->h_cache_file,
                                p_h->h_block_cache,
                                p_h->p_block_cache_index[block]+block_offset,
                                count,
                                p_buf,
                                &read);
  if(gidafs_ret!=eGidaFsError_None || read!=count) {
    LOG_ERROR("Unable to read cached data from block %" PRIu64
                ": Error code %u!\n",
              block,
              gidafs_ret);
    return e_XmountCache_Error_FailedReadingBlockCache;
  }

  return e_XmountCache_Error_None;
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
  uint64_t written=0;
  teGidaFsError gidafs_ret=eGidaFsError_None;

  // Params check
  if(p_h==NULL) return e_XmountCache_Error_InvalidHandle;
  if(p_buf==NULL) return e_XmountCache_Error_InvalidBuffer;
  if(block>=p_h->block_cache_index_len) return e_XmountCache_Error_InvalidIndex;
  if(block_offset>XMOUNT_CACHE_BLOCK_SIZE ||
     block_offset+count>XMOUNT_CACHE_BLOCK_SIZE)
  {
    return e_XmountCache_Error_ReadBeyondBlockBounds;
  }
  if(p_h->p_block_cache_index[block]==XMOUNT_CACHE_INVALID_INDEX) {
    return e_XmountCache_Error_UncachedBlock;
  }

  gidafs_ret=GidaFsLib_WriteFile(p_h->h_cache_file,
                                 p_h->h_block_cache,
                                 p_h->p_block_cache_index[block]+block_offset,
                                 count,
                                 p_buf,
                                 &written);
  if(gidafs_ret!=eGidaFsError_None || written!=count) {
    LOG_ERROR("Unable to write data to cached block %" PRIu64
                ": Error code %u!\n",
              block,
              gidafs_ret);
    return e_XmountCache_Error_FailedWritingBlockCache;
  }

  return e_XmountCache_Error_None;
}

/*
 * XmountCache_BlockCacheAppend
 */
te_XmountCache_Error XmountCache_BlockCacheAppend(pts_XmountCacheHandle p_h,
                                                  char *p_buf,
                                                  uint64_t block)
{
  uint64_t written=0;
  teGidaFsError gidafs_ret=eGidaFsError_None;
  te_XmountCache_Error ret=e_XmountCache_Error_None;

  // Params check
  if(p_h==NULL) return e_XmountCache_Error_InvalidHandle;
  if(p_buf==NULL) return e_XmountCache_Error_InvalidBuffer;
  if(block>=p_h->block_cache_index_len) return e_XmountCache_Error_InvalidIndex;

  // Get current block cache size
  gidafs_ret=GidaFsLib_GetFileSize(p_h->h_cache_file,
                                   p_h->h_block_cache,
                                   &(p_h->p_block_cache_index[block]));
  if(gidafs_ret!=eGidaFsError_None) {
    LOG_ERROR("Unable to get current block cache size: Error code %u!\n",
              gidafs_ret);
    return e_XmountCache_Error_FailedGettingIndexSize;
  }

  // Append new block
  gidafs_ret=GidaFsLib_WriteFile(p_h->h_cache_file,
                                 p_h->h_block_cache,
                                 p_h->p_block_cache_index[block],
                                 XMOUNT_CACHE_BLOCK_SIZE,
                                 p_buf,
                                 &written);
  if(gidafs_ret!=eGidaFsError_None || written!=XMOUNT_CACHE_BLOCK_SIZE) {
    LOG_ERROR("Unable to write data to cached block %" PRIu64
                ": Error code %u!\n",
              block,
              gidafs_ret);
    return e_XmountCache_Error_FailedWritingBlockCache;
  }

  // Update on-disk block cache index
  ret=XmountCache_UpdateIndex(p_h,block);
  if(ret!=e_XmountCache_Error_None) {
    LOG_ERROR("Unable to update block cache index %" PRIu64
                ": Error code %u!\n",
              block,
              ret);
    return ret;
  }

  return e_XmountCache_Error_None;
}

/*
 * XmountCache_IsBlockCached
 */
te_XmountCache_Error XmountCache_IsBlockCached(pts_XmountCacheHandle p_h,
                                               uint64_t block)
{
  // Params check
  if(p_h==NULL) return e_XmountCache_Error_InvalidHandle;
  if(block>=p_h->block_cache_index_len) return e_XmountCache_Error_InvalidIndex;

  // Check if given block has been cached previously
  if(p_h->p_block_cache_index[block]==XMOUNT_CACHE_INVALID_INDEX) {
    // Block is not in cache
    return e_XmountCache_Error_UncachedBlock;
  }

  return e_XmountCache_Error_None;
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
  pts_XmountCacheHandle p_h=*pp_h;

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

/*
 * XmountCache_UpdateIndex
 */
te_XmountCache_Error XmountCache_UpdateIndex(pts_XmountCacheHandle p_h,
                                             uint64_t entry)
{
  uint64_t update_size=0;
  uint64_t written=0;
  uint64_t *p_buf;
  teGidaFsError gidafs_ret=eGidaFsError_None;

  if(entry!=XMOUNT_CACHE_INVALID_INDEX) {
    // Update specific index entry in cache file
    p_buf=p_h->p_block_cache_index+entry;
    update_size=sizeof(uint64_t);
  } else {
    // Dump whole block cache index to cache file
    p_buf=p_h->p_block_cache_index;
    update_size=p_h->block_cache_index_len*sizeof(uint64_t);
  }

  // Update cache file
  gidafs_ret=GidaFsLib_WriteFile(p_h->h_cache_file,
                                 p_h->h_block_cache_index,
                                 0,
                                 update_size,
                                 p_buf,
                                 &written);
  if(gidafs_ret!=eGidaFsError_None || written!=update_size) {
    LOG_ERROR("Unable to update block cache index: Error code %u!\n",
              gidafs_ret)
    return e_XmountCache_Error_FailedUpdatingIndex;
  }

  return e_XmountCache_Error_None;
}
