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

#ifndef XMOUNT_MORPHING_H
#define XMOUNT_MORPHING_H

#include "../libxmount_morphing/libxmount_morphing.h"

//! Structure containing infos about morphing libs
typedef struct s_MorphingLib {
  //! Filename of lib (without path)
  char *p_name;
  //! Handle to the loaded lib
  void *p_lib;
  //! Array of supported morphing types
  char *p_supported_morphing_types;
  //! Struct containing lib functions
  ts_LibXmountMorphingFunctions lib_functions;
} ts_MorphingLib, *pts_MorphingLib;

//! Structures and vars needed for morph support
typedef struct s_MorphingData {
  //! Loaded morphing lib count
  uint32_t libs_count;
  //! Array containing infos about loaded morphing libs
  pts_MorphingLib *pp_libs;
  //! Specified morphing type (--morph)
  char *p_morph_type;
  //! Amount of specified morphing lib params (--morphopts)
  uint32_t lib_params_count;
  //! Specified morphing lib params (--morphopts)
  pts_LibXmountOptions *pp_lib_params;
  //! Handle to initialized morphing lib
  void *p_handle;
  //! Morphing functions of initialized lib
  pts_LibXmountMorphingFunctions p_functions;
  //! Input image functions passed to morphing lib
  ts_LibXmountMorphingInputFunctions input_image_functions;
} ts_MorphingData;

int GetMorphedImageSize(uint64_t*);
int ReadMorphedImageData(char*, off_t, size_t, size_t*);
int WriteMorphedImageData(const char*, off_t, size_t, size_t*);

#endif // XMOUNT_MORPHING_H
