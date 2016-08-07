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

#include <gidafs.h>

#include "../libxmount_input/libxmount_input.h"
#include "../libxmount_morphing/libxmount_morphing.h"
#include "../libxmount_output/libxmount_output.h"

#undef FALSE
#undef TRUE
#define FALSE 0
#define TRUE 1

/*
 * Constants
 */
#define IMAGE_INFO_INPUT_HEADER \
  "------> The following values are supplied by the used input library(ies) " \
    "<------\n"
#define IMAGE_INFO_MORPHING_HEADER \
  "\n------> The following values are supplied by the used morphing library " \
    "<------\n\n"

/*******************************************************************************
 * Xmount specific structures
 ******************************************************************************/

#ifdef __LP64__
  #define CACHE_BLOCK_FREE 0xFFFFFFFFFFFFFFFF
#else
  #define CACHE_BLOCK_FREE 0xFFFFFFFFFFFFFFFFLL
#endif
#ifdef __LP64__
  #define XMOUNT_BLOCK_CACHE_INVALID_INDEX 0xFFFFFFFFFFFFFFFF
#else
  #define XMOUNT_BLOCK_CACHE_INVALID_INDEX 0xFFFFFFFFFFFFFFFFLL
#endif
//! Cache file block index array element
typedef uint64_t t_CacheFileBlockIndex;
// TODO: Remove
typedef struct s_CacheFileBlockIndex {
  //! Set to 1 if block is assigned (this block has data in cache file)
  uint32_t Assigned;
  //! Offset to data in cache file
  uint64_t off_data;
} __attribute__ ((packed)) ts_CacheFileBlockIndex, *pts_CacheFileBlockIndex;

#define CACHE_BLOCK_SIZE (1024*1024) // 1 megabyte
#ifdef __LP64__
  #define CACHE_FILE_SIGNATURE 0xFFFF746E756F6D78 // "xmount\xFF\xFF"
#else
  #define CACHE_FILE_SIGNATURE 0xFFFF746E756F6D78LL
#endif
#define CUR_CACHE_FILE_VERSION 0x00000002 // Current cache file version
#define HASH_AMOUNT (1024*1024)*10 // Amount of data used to construct a
                                   // "unique" hash for every input image
                                   // (10MByte)
//! Cache file header structure
typedef struct s_CacheFileHeader {
  //! Simple signature to identify cache files
  uint64_t FileSignature;
  //! Cache file version
  uint32_t CacheFileVersion;
  //! Cache block size
  uint64_t BlockSize;
  //! Total amount of cache blocks
  uint64_t BlockCount;
  //! Offset to the first block index array element
  uint64_t pBlockIndex;
  //! Set to 1 if VDI file header is cached
  uint32_t VdiFileHeaderCached;
  //! Offset to cached VDI file header
  uint64_t pVdiFileHeader;
  //! Set to 1 if VMDK file is cached
  uint32_t VmdkFileCached;
  //! Size of VMDK file
  uint64_t VmdkFileSize;
  //! Offset to cached VMDK file
  uint64_t pVmdkFile;
  //! Set to 1 if VHD header is cached
  uint32_t VhdFileHeaderCached;
  //! Offset to cached VHD header
  uint64_t pVhdFileHeader;
  //! Padding to get 512 byte alignment and ease further additions
  char HeaderPadding[432];
} __attribute__ ((packed)) ts_CacheFileHeader, *pts_CacheFileHeader;

//! Cache file header structure - Old v1 header
typedef struct s_CacheFileHeader_v1 {
  //! Simple signature to identify cache files
  uint64_t FileSignature;
  //! Cache file version
  uint32_t CacheFileVersion;
  //! Total amount of cache blocks
  uint64_t BlockCount;
  //! Offset to the first block index array element
  uint64_t pBlockIndex;
  //! Set to 1 if VDI file header is cached
  uint32_t VdiFileHeaderCached;
  //! Offset to cached VDI file header
  uint64_t pVdiFileHeader;
  //! Set to 1 if VMDK file is cached
} ts_CacheFileHeader_v1, *pts_CacheFileHeader_v1;

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

//! Structures and vars needed for write access
#define XMOUNT_CACHE_FOLDER "/.xmount"
#define XMOUNT_CACHE_BLOCK_FILE XMOUNT_CACHE_FOLDER "/blocks.data"
#define XMOUNT_CACHE_BLOCK_INDEX_FILE XMOUNT_CACHE_FOLDER "/blocks.index"
typedef struct s_CacheData {
  //! Cache file to save changes to
  char *p_cache_file;
  //! Handle to cache file
  hGidaFs h_cache_file;
  //! Handle to block cache
  hGidaFsFile h_block_cache;
  //! Handle to block cache index
  hGidaFsFile h_block_cache_index;
  //! In-memory copy of cache index
  t_CacheFileBlockIndex *p_block_cache_index;
  //! Length (in elements) of in-memory block cache index
  uint64_t block_cache_index_len;
  // TODO: Move to s_XmountData
  //! Overwrite existing cache
  uint8_t overwrite_cache;
} ts_CacheData;

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

//! Structure containing global xmount runtime infos
typedef struct s_XmountData {
  //! Input image related data
  ts_InputData input;
  //! Morphing related data
  ts_MorphingData morphing;
  //! Cache file related data
  ts_CacheData cache;
  //! Output image related data
  ts_OutputData output;
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
int GetOutputImageSize(uint64_t*);
int ReadOutputImageData(char*, off_t, size_t);
int WriteOutputImageData(const char*, off_t, size_t);

#endif // XMOUNT_H
