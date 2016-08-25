/*******************************************************************************
* xmount Copyright (c) 2008-2016 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* xmount is a small tool to "fuse mount" various image formats and enable      *
* virtual write access.                                                        *
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

#ifndef XMOUNT_H
#define XMOUNT_H

#include "../libxmount/libxmount.h"
#include "xmount_input.h"
#include "xmount_morphing.h"
#include "xmount_cache.h"
#include "xmount_output.h"

#undef FALSE
#undef TRUE
#define FALSE 0
#define TRUE 1

/*
 * Constants
 */

#define XMOUNT_
/*******************************************************************************
 * Xmount specific structures
 ******************************************************************************/
//! Return codes
typedef enum e_XmountError {
  //! No error
  e_XmountError_None=0,

} te_XmountError;

//! Structure containing xmount arguments
typedef struct s_XmountArgs {
  //! Cache file
  char *p_cache_file;
  //! If set to 1, overwrite any existing cache file
  uint8_t overwrite_cache;
  //! Writable? (Set to 1 if --cache was specified)
  uint8_t writable;
} ts_XmountArgs;

//! Structure containing global xmount runtime infos
typedef struct s_XmountData {
  //! Parsed xmount arguments
  ts_XmountArgs args;
  //! Input image related data
  pts_XmountInputHandle h_input;
  //! Morphing related data
  pts_XmountMorphHandle h_morphing;
  //! Cache file handle
  pts_XmountCacheHandle h_cache;
  //! Output image related data
  pts_XmountOutputHandle h_output;
  //! Enable debug output
  uint8_t debug;
  //! Set if we are allowed to set fuse's allow_other option
  uint8_t may_set_fuse_allow_other;
  //! Argv for FUSE
  int fuse_argc;
  //! Argv for FUSE
  char **pp_fuse_argv;
  //! Mount point
  char *p_mountpoint;
  //! First input image's path/name
  char *p_first_input_image_name;
  //! Path of virtual image info file
  char *p_info_path;
  //! Pointer to virtual info file
  char *p_info_file;
  //! MD5 hash of partial input image (lower 64 bit) (after morph)
  uint64_t image_hash_lo;
  //! MD5 hash of partial input image (higher 64 bit) (after morph)
  uint64_t image_hash_hi;
  //! Mutex to control concurrent read & write access on output image
  pthread_mutex_t mutex_image_rw;
  //! Mutex to control concurrent read access on info file
  pthread_mutex_t mutex_info_read;
} ts_XmountData;

/*******************************************************************************
 * Global vars
 ******************************************************************************/
//! Struct that contains various runtime configuration options
ts_XmountData glob_xmount;

/*******************************************************************************
 * Xmount functions
 ******************************************************************************/
te_XmountError Xmount_GetFileAttr(const char *p_path, struct stat *p_stat);
te_XmountError Xmount_GetDirListing(const char *p_path,
                                    uint64_t *p_dir_entries,
                                    char ***ppp_dir_entries);

#endif // XMOUNT_H
