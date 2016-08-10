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

/*
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
*/

/*******************************************************************************
 * Public definitions / macros
 ******************************************************************************/


/*******************************************************************************
 * Public types / structures / enums
 ******************************************************************************/
typedef struct s_XmountInputHandle *pts_XmountInputHandle;

typedef enum e_XmountInput_Error {
  //! No error
  e_XmountInput_Error_None=0,
  //! Error to allocate memory
  e_XmountInput_Error_Alloc,
  //! Invalid input handle
  e_XmountInput_Error_InvalidHandle,
  //! Invalid pointer to an input handle
  e_XmountInput_Error_InvalidHandlePointer,
  //! A given buffer is invalid
  e_XmountInput_Error_InvalidBuffer,
  //! A given string is invalid
  e_XmountInput_Error_InvalidString,
  //! A given array is invalid
  e_XmountInput_Error_InvalidArray,
/*

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
*/
} te_XmountInput_Error;

/*******************************************************************************
 * Public functions declarations
 ******************************************************************************/
/*!
 * \brief Create new input handle
 *
 * Creates a new input handle.
 *
 * \param pp_h Pointer to input handle
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_CreateHandle(pts_XmountInputHandle *pp_h);

/*!
 * \brief Destroy input handle
 *
 * Invalidates the given handle and frees all used resources.
 *
 * \param pp_h Pointer to input handle
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_DestroyHandle(pts_XmountInputHandle *pp_h);

/*!
 * \brief XXX
 *
 * XXX
 *
 * \param p_h Input handle
 */
te_XmountInput_Error XmountInput_LoadLibs(pts_XmountInputHandle p_h);

/*!
 * \brief Return all supported formats
 *
 * Returns a null-terminated vector of all supported input formats.
 *
 * The returned vector must be freed by the caller.
 *
 * \param p_h Input handle
 * \param pp_formats Supported formats vector is returned in this var
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_GetSupportedFormats(pts_XmountInputHandle p_h,
                                                     char **pp_formats);

/*!
 * \brief Return all library specific option help texts
 *
 * Returns a string containing help messages for all loaded input lib options.
 * The string is pre-formated to be used in xmount's help output.
 *
 * The caller must free the returned string.
 *
 * \param p_h Input handle
 * \param pp_help_text Help text is returned in this parameter
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_GetOptionsHelpText(pts_XmountInputHandle p_h,
                                                    char **pp_help_text);

/*!
 * \brief Add an input image
 *
 * Adds the given input image to the list of available input images.
 *
 * \param p_h Input handle
 * \param p_format Input image format string as given by the user
 * \param files_count Amount of files specified by the user
 * \param pp_files Array containing all specified files
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_AddImage(pts_XmountInputHandle p_h,
                                          const char *p_format,
                                          uint64_t files_count,
                                          const char **pp_files);

te_XmountInput_Error XmountInput_SetInputOffset(pts_XmountInputHandle p_h,
                                                uint64_t offset);

te_XmountInput_Error XmountInput_SetInputSizeLimit(pts_XmountInputHandle p_h,
                                                   uint64_t size_limit);

te_XmountInput_Error XmountInput_GetSize(pts_XmountInputHandle p_h,
                                         uint64_t image_nr,
                                         uint64_t *p_size);

te_XmountInput_Error XmountInput_ReadData(pts_XmountInputHandle p_h,
                                          uint64_t image_nr,
                                          char *p_buf,
                                          uint64_t offset,
                                          uint64_t count);

te_XmountInput_Error XmountInput_WriteData(pts_XmountInputHandle p_h,
                                           uint64_t image_nr,
                                           const char *p_buf,
                                           uint64_t offset,
                                           uint64_t count);

#endif // XMOUNT_INPUT_H
