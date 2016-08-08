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

#include "xmount_input.h"
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

//! Read data from input image
/*!
 * \param p_image Image from which to read data
 * \param p_buf Pointer to buffer to write read data to (must be preallocated!)
 * \param offset Offset at which data should be read
 * \param size Size of data which should be read (size of buffer)
 * \param p_read Number of read bytes on success
 * \return 0 on success, negated error code on error
 */
int ReadInputImageData(pts_InputImage p_image,
                       char *p_buf,
                       off_t offset,
                       size_t size,
                       size_t *p_read)
{
  int ret;
  size_t to_read=0;
  int read_errno=0;

  LOG_DEBUG("Reading %zu bytes at offset %zu from input image '%s'\n",
            size,
            offset,
            p_image->pp_files[0]);

  // Make sure we aren't reading past EOF of image file
  if(offset>=p_image->size) {
    // Offset is beyond image size
    LOG_DEBUG("Offset %zu is at / beyond size of input image '%s'\n",
              offset,
              p_image->pp_files[0]);
    *p_read=0;
    return 0;
  }
  if(offset+size>p_image->size) {
    // Attempt to read data past EOF of image file
    to_read=p_image->size-offset;
    LOG_DEBUG("Attempt to read data past EOF of input image '%s'. "
                "Correcting size from %zu to %zu\n",
              p_image->pp_files[0],
              size,
              to_read);
  } else to_read=size;

  // Read data from image file (adding input image offset if one was specified)
  ret=p_image->p_functions->Read(p_image->p_handle,
                                 p_buf,
                                 offset+glob_xmount.input.image_offset,
                                 to_read,
                                 p_read,
                                 &read_errno);
  if(ret!=0) {
    LOG_ERROR("Couldn't read %zu bytes at offset %zu from input image "
                "'%s': %s!\n",
              to_read,
              offset,
              p_image->pp_files[0],
              p_image->p_functions->GetErrorMessage(ret));
    if(read_errno==0) return -EIO;
    else return (read_errno*(-1));
  }

  return 0;
}
