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

#ifndef XMOUNT_OUTPUT_H
#define XMOUNT_OUTPUT_H

#include "../libxmount_output/libxmount_output.h"

//! Structure containing infos about output libs
typedef struct s_OutputLib {
  //! Filename of lib (without path)
  char *p_name;
  //! Handle to the loaded lib
  void *p_lib;
  //! Array of supported output formats
  char *p_supported_output_formats;
  //! Struct containing lib functions
  ts_LibXmountOutput_Functions lib_functions;
} ts_OutputLib, *pts_OutputLib;

//! Structure containing infos about output image
typedef struct s_OutputData {
  //! Loaded output lib count
  uint32_t libs_count;
  //! Array containing infos about loaded output libs
  pts_OutputLib *pp_libs;
  //! Specified output format (--out)
  char *p_output_format;
  //! Amount of specified output lib params
  uint32_t lib_params_count;
  //! Specified output lib params (--outopts)
  pts_LibXmountOptions *pp_lib_params;
  //! Handle to initialized output lib
  void *p_handle;
  //! Transformation functions of initialized lib
  pts_LibXmountOutput_Functions p_functions;
  //! Input image functions passed to output lib
  ts_LibXmountOutput_InputFunctions input_functions;
  //! Size
  uint64_t image_size;
  //! Writable? (Set to 1 if --cache was specified)
  uint8_t writable;
  //! Path of virtual image file
  char *p_virtual_image_path;
  //! Path of virtual image info file
  char *p_info_path;
  //! Pointer to virtual info file
  char *p_info_file;
} ts_OutputData;

int GetOutputImageSize(uint64_t*);
int ReadOutputImageData(char*, off_t, size_t);
int WriteOutputImageData(const char*, off_t, size_t);

#endif // XMOUNT_OUTPUT_H
