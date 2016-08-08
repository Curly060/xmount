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

#include <errno.h>
#include <string.h> // For memcpy

#include "xmount_morphing.h"
#include "xmount.h"
#include "macros.h"

#define LOG_WARNING(...) {            \
  LIBXMOUNT_LOG_WARNING(__VA_ARGS__); \
}
#define LOG_ERROR(...) {            \
  LIBXMOUNT_LOG_ERROR(__VA_ARGS__); \
}
#define LOG_DEBUG(...) {                              \
  LIBXMOUNT_LOG_DEBUG(glob_xmount.debug,__VA_ARGS__); \
}

//! Get size of morphed image
/*!
 * \param p_size Buf to save size to
 * \return TRUE on success, FALSE on error
 */
int GetMorphedImageSize(uint64_t *p_size) {
  int ret;

  ret=glob_xmount.morphing.p_functions->Size(glob_xmount.morphing.p_handle,
                                             p_size);
  if(ret!=0) {
    LOG_ERROR("Unable to get morphed image size: %s!\n",
              glob_xmount.morphing.p_functions->GetErrorMessage(ret));
    return FALSE;
  }

  return TRUE;
}

//! Read data from morphed image
/*!
 * \param p_buf Pointer to buffer to write read data to (must be preallocated!)
 * \param offset Offset at which data should be read
 * \param size Size of data which should be read (size of buffer)
 * \param p_read Number of read bytes on success
 * \return TRUE on success, negated error code on error
 */
int ReadMorphedImageData(char *p_buf,
                                off_t offset,
                                size_t size,
                                size_t *p_read)
{
  uint64_t block_off=0;
  uint64_t cur_block=0;
  uint64_t cur_to_read=0;
  uint64_t image_size=0;
  size_t read=0;
  size_t to_read=0;
  int ret;
  uint8_t is_block_cached=FALSE;
  te_XmountCache_Error cache_ret=e_XmountCache_Error_None;

  // Make sure we aren't reading past EOF of image file
  if(GetMorphedImageSize(&image_size)!=TRUE) {
    LOG_ERROR("Couldn't get size of morphed image!\n");
    return -EIO;
  }
  if(offset>=image_size) {
    // Offset is beyond image size
    LOG_DEBUG("Offset %zu is at / beyond size of morphed image.\n",offset);
    *p_read=0;
    return FALSE;
  }
  if(offset+size>image_size) {
    // Attempt to read data past EOF of morphed image file
    to_read=image_size-offset;
    LOG_DEBUG("Attempt to read data past EOF of morphed image. Corrected size "
                "from %zu to %" PRIu64 ".\n",
              size,
              to_read);
  } else to_read=size;

  // Calculate block to start reading data from
  cur_block=offset/XMOUNT_CACHE_BLOCK_SIZE;
  block_off=offset%XMOUNT_CACHE_BLOCK_SIZE;

  // Read image data
  while(to_read!=0) {
    // Calculate how many bytes we have to read from this block
    if(block_off+to_read>XMOUNT_CACHE_BLOCK_SIZE) {
      cur_to_read=XMOUNT_CACHE_BLOCK_SIZE-block_off;
    } else cur_to_read=to_read;

    // Determine if we have to read cached data
    is_block_cached=FALSE;
    if(glob_xmount.output.writable==TRUE) {
      cache_ret=XmountCache_IsBlockCached(glob_xmount.h_cache,cur_block);
      if(cache_ret==e_XmountCache_Error_None) is_block_cached=TRUE;
      else if(cache_ret!=e_XmountCache_Error_UncachedBlock) {
        LOG_ERROR("Unable to determine if block %" PRIu64 " is cached: "
                    "Error code %u!\n",
                  cur_block,
                  cache_ret);
        return -EIO;
      }
    }

    // Check if block is cached
    if(is_block_cached==TRUE) {
      // Write support enabled and need to read altered data from cachefile
      LOG_DEBUG("Reading %zu bytes from block cache file\n",cur_to_read);

      cache_ret=XmountCache_BlockCacheRead(glob_xmount.h_cache,
                                           p_buf,
                                           cur_block,
                                           block_off,
                                           cur_to_read);
      if(cache_ret!=e_XmountCache_Error_None) {
        LOG_ERROR("Unable to read %" PRIu64
                    " bytes of cached data from cache block %" PRIu64
                    " at cache block offset %" PRIu64 ": Error code %u!\n",
                  cur_to_read,
                  cur_block,
                  block_off,
                  cache_ret);
        return -EIO;
      }
    } else {
      // No write support or data not cached
      ret=glob_xmount.morphing.p_functions->Read(glob_xmount.morphing.p_handle,
                                                 p_buf,
                                                 (cur_block*XMOUNT_CACHE_BLOCK_SIZE)+
                                                   block_off,
                                                 cur_to_read,
                                                 &read);
      if(ret!=0 || read!=cur_to_read) {
        LOG_ERROR("Couldn't read %zu bytes at offset %zu from morphed image: "
                    "%s!\n",
                  cur_to_read,
                  offset,
                  glob_xmount.morphing.p_functions->GetErrorMessage(ret));
        return -EIO;
      }
      LOG_DEBUG("Read %" PRIu64 " bytes at offset %" PRIu64
                  " from morphed image file\n",
                cur_to_read,
                (cur_block*XMOUNT_CACHE_BLOCK_SIZE)+block_off);
    }

    cur_block++;
    block_off=0;
    p_buf+=cur_to_read;
    to_read-=cur_to_read;
  }

  *p_read=to_read;
  return TRUE;
}

//! Write data to morphed image
/*!
 * \param p_buf Buffer with data to write
 * \param offset Offset to start writing at
 * \param count Amount of bytes to write
 * \param p_written Amount of successfully written bytes
 * \return TRUE on success, negated error code on error
 */
int WriteMorphedImageData(const char *p_buf,
                                 off_t offset,
                                 size_t count,
                                 size_t *p_written)
{
  uint64_t block_off=0;
  uint64_t cur_block=0;
  uint64_t cur_to_read=0;
  uint64_t cur_to_write=0;
  uint64_t image_size=0;
  uint64_t read=0;
  size_t written=0;
  size_t to_write=0;
  int ret;
  char *p_buf2=NULL;
  uint8_t is_block_cached=FALSE;
  te_XmountCache_Error cache_ret=e_XmountCache_Error_None;

  // Make sure we aren't writing past EOF of image file
  if(GetMorphedImageSize(&image_size)!=TRUE) {
    LOG_ERROR("Couldn't get size of morphed image!\n");
    return -EIO;
  }
  if(offset>=image_size) {
    // Offset is beyond image size
    LOG_DEBUG("Offset %zu is at / beyond size of morphed image.\n",offset);
    *p_written=0;
    return 0;
  }
  if(offset+count>image_size) {
    // Attempt to write data past EOF of morphed image file
    to_write=image_size-offset;
    LOG_DEBUG("Attempt to write data past EOF of morphed image. Corrected size "
                "from %zu to %" PRIu64 ".\n",
              count,
              to_write);
  } else to_write=count;

  // Calculate block to start writing data to
  cur_block=offset/XMOUNT_CACHE_BLOCK_SIZE;
  block_off=offset%XMOUNT_CACHE_BLOCK_SIZE;

  while(to_write!=0) {
    // Calculate how many bytes we have to write to this block
    if(block_off+to_write>XMOUNT_CACHE_BLOCK_SIZE) {
      cur_to_write=XMOUNT_CACHE_BLOCK_SIZE-block_off;
    } else cur_to_write=to_write;

    // Determine if block is already in cache
    is_block_cached=FALSE;
    cache_ret=XmountCache_IsBlockCached(glob_xmount.h_cache,cur_block);
    if(cache_ret==e_XmountCache_Error_None) is_block_cached=TRUE;
    else if(cache_ret!=e_XmountCache_Error_UncachedBlock) {
      LOG_ERROR("Unable to determine if block %" PRIu64 " is cached: "
                  "Error code %u!\n",
                cur_block,
                cache_ret);
      return -EIO;
    }

    // Check if block is cached
    if(is_block_cached==TRUE) {
      // Block is cached
      cache_ret=XmountCache_BlockCacheWrite(glob_xmount.h_cache,
                                            p_buf,
                                            cur_block,
                                            block_off,
                                            cur_to_write);
      if(cache_ret!=e_XmountCache_Error_None) {
        LOG_ERROR("Unable to write %" PRIu64
                    " bytes of data to cache block %" PRIu64
                    " at cache block offset %" PRIu64 ": Error code %u!\n",
                  cur_to_write,
                  cur_block,
                  block_off,
                  cache_ret);
        return -EIO;
      }

      LOG_DEBUG("Wrote %" PRIu64 " bytes to block cache\n",cur_to_write);
    } else {
      // Uncached block. Need to cache entire new block
      // Prepare new write buffer
      XMOUNT_MALLOC(p_buf2,char*,XMOUNT_CACHE_BLOCK_SIZE);
      memset(p_buf2,0x00,XMOUNT_CACHE_BLOCK_SIZE);

      // Read full block from morphed image
      cur_to_read=XMOUNT_CACHE_BLOCK_SIZE;
      if((cur_block*XMOUNT_CACHE_BLOCK_SIZE)+XMOUNT_CACHE_BLOCK_SIZE>image_size) {
        cur_to_read=XMOUNT_CACHE_BLOCK_SIZE-(((cur_block*XMOUNT_CACHE_BLOCK_SIZE)+
                                         XMOUNT_CACHE_BLOCK_SIZE)-image_size);
      }
      ret=glob_xmount.morphing.p_functions->Read(glob_xmount.morphing.p_handle,
                                                 p_buf2,
                                                 cur_block*XMOUNT_CACHE_BLOCK_SIZE,
                                                 cur_to_read,
                                                 &read);
      if(ret!=0 || read!=cur_to_read) {
        LOG_ERROR("Couldn't read %" PRIu64 " bytes at offset %zu "
                    "from morphed image: %s!\n",
                  cur_to_read,
                  offset,
                  glob_xmount.morphing.p_functions->GetErrorMessage(ret));
        XMOUNT_FREE(p_buf2);
        return -EIO;
      }

      // Set changed data
      memcpy(p_buf2+block_off,p_buf,cur_to_write);

      cache_ret=XmountCache_BlockCacheAppend(glob_xmount.h_cache,
                                             p_buf,
                                             cur_block);
      if(cache_ret!=e_XmountCache_Error_None) {
        LOG_ERROR("Unable to append new block cache block %" PRIu64
                    ": Error code %u!\n",
                  cur_block,
                  cache_ret);
        XMOUNT_FREE(p_buf2);
        return -EIO;
      }
      XMOUNT_FREE(p_buf2);

      LOG_DEBUG("Appended new block cache block %" PRIu64 "\n",cur_block);
    }

    block_off=0;
    cur_block++;
    p_buf+=cur_to_write;
    to_write-=cur_to_write;
  }

  *p_written=to_write;
  return TRUE;
}
