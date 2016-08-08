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

#include "xmount_output.h"
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

//! Get size of output image
/*!
 * \param p_size Pointer to an uint64_t to which the size will be written to
 * \return TRUE on success, FALSE on error
 */
int GetOutputImageSize(uint64_t *p_size) {
  int ret;
  uint64_t output_image_size=0;

  if(glob_xmount.output.image_size!=0) {
    *p_size=glob_xmount.output.image_size;
    return TRUE;
  }

  ret=glob_xmount.output.p_functions->Size(glob_xmount.output.p_handle,
                                           &output_image_size);
  if(ret!=0) {
    LOG_ERROR("Couldn't get output image size!\n")
    return FALSE;
  }

  glob_xmount.output.image_size=output_image_size;
  *p_size=output_image_size;
  return TRUE;
}

//! Read data from output image
/*!
 * \param p_buf Pointer to buffer to write read data to
 * \param offset Offset at which data should be read
 * \param size Size of data which should be read
 * \return Number of read bytes on success or negated error code on error
 */
int ReadOutputImageData(char *p_buf, off_t offset, size_t size) {
  uint64_t output_image_size;
  size_t read=0;
  int ret;

  // Get output image size
  if(GetOutputImageSize(&output_image_size)!=TRUE) {
    LOG_ERROR("Couldn't get size of output image!\n")
    return -EIO;
  }

  // Make sure request is within output image
  if(offset>=output_image_size) {
    LOG_DEBUG("Offset %zu is at / beyond size of output image.\n",offset);
    return 0;
  }
  if(offset+size>output_image_size) {
    LOG_DEBUG("Attempt to read data past EOF of output image. Correcting size "
                "from %zu to %zu.\n",
              size,
              output_image_size-offset);
    size=output_image_size-offset;
  }

  // Read data
  ret=glob_xmount.output.p_functions->Read(glob_xmount.output.p_handle,
                                           p_buf,
                                           offset,
                                           size,
                                           &read);
  if(ret!=0) {
    LOG_ERROR("Unable to read %zu bytes at offset %zu from output image!\n",
              size,
              offset)
    return ret;
  } else if(read!=size) {
    LOG_WARNING("Unable to read all requested data from output image!\n")
    return read;
  }

  return size;
}

//! Write data to output image
/*!
 * \param p_buf Buffer with data to write
 * \param offset Offset to write to
 * \param size Amount of bytes to write
 * \return Number of written bytes on success or "-1" on error
 */
int WriteOutputImageData(const char *p_buf, off_t offset, size_t size) {
  uint64_t output_image_size;
  int ret;
  size_t written;

  // Get output image size
  if(!GetOutputImageSize(&output_image_size)) {
    LOG_ERROR("Couldn't get output image size!\n")
    return -1;
  }

  // Make sure write is within output image
  if(offset>=output_image_size) {
    LOG_ERROR("Attempt to write beyond EOF of output image file!\n")
    return -1;
  }
  if(offset+size>output_image_size) {
    LOG_DEBUG("Attempt to write past EOF of output image file. Correcting size "
                "from %zu to %zu.\n",
              size,
              output_image_size-offset);
    size=output_image_size-offset;
  }

  ret=glob_xmount.output.p_functions->Write(glob_xmount.output.p_handle,
                                            p_buf,
                                            offset,
                                            size,
                                            &written);
  if(ret!=0) {
    LOG_ERROR("Unable to write %zu bytes at offset %zu to output image!\n",
              offset,
              size)
    return ret;
  } else if(written!=size) {
    LOG_WARNING("Unable to write all requested data to output image!\n")
  }

  return size;
}
