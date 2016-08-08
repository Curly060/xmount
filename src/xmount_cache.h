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

#ifndef XMOUNT_CACHE_H
#define XMOUNT_CACHE_H

#include <gidafs.h>

/*******************************************************************************
 * Public definitions / macros
 ******************************************************************************/
//! Default block cache block size to use (1 megabyte)
#define XMOUNT_CACHE_BLOCK_SIZE (1024*1024)

/*******************************************************************************
 * Public types / structures / enums
 ******************************************************************************/
typedef struct s_XmountCacheHandle *pts_XmountCacheHandle;

typedef enum e_XmountCache_Error {
  //! No error
  e_XmountCache_Error_None=0,
  //! Error to allocate memory
  e_XmountCache_Error_Alloc,
  //! Invalid cache handle
  e_XmountCache_Error_InvalidHandle,
  //! Invalid pointer to a cache handle
  e_XmountCache_Error_InvalidHandlePointer,
  //! A given string is invalid
  e_XmountCache_Error_InvalidString,
  //! A given file path / name is invalid
  e_XmountCache_Error_InvalidFile,
  //! A given file does not exist
  e_XmountCache_Error_InexistingFile,
  //! A given file exists
  e_XmountCache_Error_ExistingFile,
  //! Unable to create needed xmount structures inside cache file
  e_XmountCache_Error_FailedCacheInit,
  //! Unable to open xmount cache file
  e_XmountCache_Error_FailedOpeningCache,
  //! Failed to get block cache index size
  e_XmountCache_Error_FailedGettingIndexSize,
  //! Invalid block cache index size
  e_XmountCache_Error_InvalidIndexSize,
  //! Unable to read block cache index
  e_XmountCache_Error_FailedReadingIndex,
  //! Failed closing cache block index
  e_XmountCache_Error_FailedClosingIndex,
  //! Failed closing cache block index
  e_XmountCache_Error_FailedClosingBlockCache,
  //! Failed closing cache block index
  e_XmountCache_Error_FailedClosingCache,
  //! Failed to update block cache index
  e_XmountCache_Error_FailedUpdatingIndex,
  //! Invalid block cache index specified
  e_XmountCache_Error_InvalidIndex,
  //! Block has not yet been cached
  e_XmountCache_Error_UncachedBlock,
  //! Invalid buffer specified
  e_XmountCache_Error_InvalidBuffer,
  //! Request would read beyond a single cache block
  e_XmountCache_Error_ReadBeyondBlockBounds,
  //! Failed reading cached data
  e_XmountCache_Error_FailedReadingBlockCache,
  //! Failed writing cached data
  e_XmountCache_Error_FailedWritingBlockCache,
} te_XmountCache_Error;

/*******************************************************************************
 * Public functions declarations
 ******************************************************************************/
/*!
 * \brief Create new xmount cache file
 *
 * Creates a new xmount cache file in the given file. If the given file already
 * exists, this function will fail except if overwrite is set to 1.
 *
 * \param pp_handle Pointer to an xmount cache handle
 * \param p_file File to use as cache file
 * \param image_size Size of image in bytes for which this cache will be used
 * \param overwrite If set to 1, overwrites existig cache file
 * \return e_XmountCache_Error_None on success
 */
te_XmountCache_Error XmountCache_Create(pts_XmountCacheHandle *pp_h,
                                        const char *p_file,
                                        uint64_t image_size,
                                        uint8_t overwrite);

/*!
 * \brief Open an existing xmount cache file
 *
 * Opens the given xmount cache file.
 *
 * \param pp_handle Pointer to an xmount cache handle
 * \param p_file File to use as cache file
 * \param image_size Size of image in bytes for which this cache will be used
 * \return e_XmountCache_Error_None on success
 */
te_XmountCache_Error XmountCache_Open(pts_XmountCacheHandle *pp_h,
                                      const char *p_file,
                                      uint64_t image_size);

/*!
 * \brief Closes a previously openend xmount cache file
 *
 * Closes the given xmount cache file and frees any used resources.
 *
 * \param pp_handle Pointer to an xmount cache handle
 * \return e_XmountCache_Error_None on success
 */
te_XmountCache_Error XmountCache_Close(pts_XmountCacheHandle *pp_h);

/*!
 * \brief Read data from block cache
 *
 * Reads count bytes at offset block_offset from block number block and writes
 * the read data into the pre-allocated buffer p_buf. The given block has to
 * have been previously cached by a call to XmountCache_BlockCacheAppend().
 *
 * WARNING: This function does only work on single blocks. It is not possible to
 * read beyond a block end.
 *
 * \param p_handle Xmount cache handle
 * \param p_buf Buffer to store read data into
 * \param block Number of block to read data from
 * \param block_offset Offset inside block to start reading from
 * \param count Amount of bytes to read
 * \return e_XmountCache_Error_None on success
 */
te_XmountCache_Error XmountCache_BlockCacheRead(pts_XmountCacheHandle p_h,
                                                char *p_buf,
                                                uint64_t block,
                                                uint64_t block_offset,
                                                uint64_t count);

/*!
 * \brief Write data to block cache
 *
 * Writes count bytes from buffer p_buf to block number block at offset
 * block_offset. The given block has to have been previously cached by a call to
 * XmountCache_BlockCacheAppend().
 *
 * WARNING: This function does only work on single blocks. It is not possible to
 * write beyond a block end.
 *
 * \param p_handle Xmount cache handle
 * \param p_buf Buffer with data to write
 * \param block Number of block to write data to
 * \param block_offset Offset inside block to start writing from
 * \param count Amount of bytes to write
 * \return e_XmountCache_Error_None on success
 */
te_XmountCache_Error XmountCache_BlockCacheWrite(pts_XmountCacheHandle p_h,
                                                 char *p_buf,
                                                 uint64_t block,
                                                 uint64_t block_offset,
                                                 uint64_t count);

/*!
 * \brief Add a new block to the cache
 *
 * Adds the data inside p_buf to the cache file and saves it under the block
 * number block. Every block must contain exactly XMOUNT_CACHE_BLOCK_SIZE bytes.
 * Every block can only be cached once. Appending the same block twice will
 * fail.
 *
 * \param p_handle Xmount cache handle
 * \param p_buf Buffer with block data
 * \param block Number of block under which to save given data
 * \return e_XmountCache_Error_None on success
 */
te_XmountCache_Error XmountCache_BlockCacheAppend(pts_XmountCacheHandle p_h,
                                                  char *p_buf,
                                                  uint64_t block);

/*!
 * \brief Chech if a block has previously been chached
 *
 * Checks if the given block has previously been cached. If it hasn't,
 * e_XmountCache_Error_UncachedBlock is returned.
 *
 * \param p_handle Xmount cache handle
 * \param block Number of block to check
 * \return e_XmountCache_Error_None on success
 */
te_XmountCache_Error XmountCache_IsBlockCached(pts_XmountCacheHandle p_h,
                                               uint64_t block);

#endif // XMOUNT_CACHE_H
