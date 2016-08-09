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
//#include "xmount.h"

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

/*******************************************************************************
 * Private types / structures / enums
 ******************************************************************************/

//! Structure containing infos about input libs
typedef struct s_InputLib {
  //! Filename of lib (without path)
  char *p_name;
  //! Handle to the loaded lib
  void *p_lib;
  //! Array of supported input types
  char *p_supported_input_types;
  //! Struct containing lib functions
  ts_LibXmountInputFunctions lib_functions;
} ts_InputLib, *pts_InputLib;

//! Structure containing infos about input images
typedef struct s_InputImage {
  //! Image type
  char *p_type;
  //! Image source file count
  uint64_t files_count;
  //! Image source files
  char **pp_files;
  //! Input lib functions for this image
  pts_LibXmountInputFunctions p_functions;
  //! Image handle
  void *p_handle;
  //! Image size
  uint64_t size;
} ts_InputImage, *pts_InputImage;

typedef struct s_XmountInputHandle {
  //! Loaded input lib count
  uint32_t libs_count;
  //! Array containing infos about loaded input libs
  pts_InputLib *pp_libs;
  //! Amount of input lib params (--inopts)
  uint32_t lib_params_count;
  //! Input lib params (--inopts)
  pts_LibXmountOptions *pp_lib_params;
  //! Input image count
  uint64_t images_count;
  //! Input images
  pts_InputImage *pp_images;
  //! Input image offset (--offset)
  uint64_t image_offset;
  //! Input image size limit (--sizelimit)
  uint64_t image_size_limit;

  // TODO: Move
  //! MD5 hash of partial input image (lower 64 bit) (after morph)
  //uint64_t image_hash_lo;
  //! MD5 hash of partial input image (higher 64 bit) (after morph)
  //uint64_t image_hash_hi;
} ts_XmountInputHandle;

/*******************************************************************************
 * Private functions declarations
 ******************************************************************************/


/*******************************************************************************
 * Public functions implementations
 ******************************************************************************/


/*******************************************************************************
 * Private functions implementations
 ******************************************************************************/

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
