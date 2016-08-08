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

#ifndef XMOUNT_INPUT_H
#define XMOUNT_INPUT_H

#include "../libxmount_input/libxmount_input.h"

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

typedef struct s_InputData {
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
  //! MD5 hash of partial input image (lower 64 bit) (after morph)
  uint64_t image_hash_lo;
  //! MD5 hash of partial input image (higher 64 bit) (after morph)
  uint64_t image_hash_hi;
} ts_InputData;

int ReadInputImageData(pts_InputImage, char*, off_t, size_t, size_t*);

#endif // XMOUNT_INPUT_H
