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

#include <stdlib.h> // For calloc
#include <string.h> // For memcpy
#include <errno.h>

#include "xmount_input.h"
#include "xmount.h"
#include "macros.h"

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
typedef struct s_XmountInputLib {
  //! Filename of lib (without path)
  char *p_name;
  //! Handle to the loaded lib
  void *p_lib;
  //! Array of supported input types
  char *p_supported_input_types;
  //! Struct containing lib functions
  ts_LibXmountInputFunctions lib_functions;
} ts_XmountInputLib, *pts_XmountInputLib;

//! Structure containing infos about input images
typedef struct s_XmountInputImage {
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
} ts_XmountInputImage, *pts_XmountInputImage;

typedef struct s_XmountInputHandle {
  //! Loaded input lib count
  uint32_t libs_count;
  //! Array containing infos about loaded input libs
  pts_XmountInputLib *pp_libs;
  //! Amount of input lib params (--inopts)
  uint32_t lib_params_count;
  //! Input lib params (--inopts)
  pts_LibXmountOptions *pp_lib_params;
  //! Input image count
  uint64_t images_count;
  //! Input images
  pts_XmountInputImage *pp_images;
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
/*
 * XmountInput_CreateHandle
 */
te_XmountInput_Error XmountInput_CreateHandle(pts_XmountInputHandle *pp_h) {
  pts_XmountInputHandle p_h=NULL;

  // Params check
  if(pp_h==NULL) return e_XmountInput_Error_InvalidHandlePointer;

  // Alloc new handle
  p_h=(pts_XmountInputHandle)calloc(1,sizeof(ts_XmountInputHandle));
  if(p_h==NULL) {
    return e_XmountInput_Error_Alloc;
  }

  // Init values
  p_h->pp_libs=NULL;
  p_h->pp_lib_params=NULL;
  p_h->pp_images=NULL;

  *pp_h=p_h;
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_DestroyHandle
 */
te_XmountInput_Error XmountInput_DestroyHandle(pts_XmountInputHandle *pp_h) {
  pts_XmountInputHandle p_h=NULL;

  // Params check
  if(pp_h==NULL) return e_XmountInput_Error_InvalidHandlePointer;
  if(*pp_h==NULL) return e_XmountInput_Error_InvalidHandle;
  p_h=*pp_h;

  // Free resources
  if(p_h->libs_count>0 && p_h->pp_libs!=NULL) {
    // TODO
  }
  if(p_h->lib_params_count!=0 && p_h->pp_lib_params!=NULL) {
    // Free library parameter array
    for(uint32_t i=0;i<p_h->lib_params_count;i++) {
      free(p_h->pp_lib_params[i]);
    }
    free(p_h->pp_lib_params);
  }
  if(p_h->images_count!=0 && p_h->pp_images!=NULL) {
    // TODO
  }

  return e_XmountInput_Error_None;
}

/*
 * XmountInput_LoadLibs
 */
te_XmountInput_Error XmountInput_LoadLibs(pts_XmountInputHandle p_h) {
  // TODO: Implement
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_GetSupportedFormats
 */
te_XmountInput_Error XmountInput_GetSupportedFormats(pts_XmountInputHandle p_h,
                                                     char **pp_formats)
{
  char *p_buf=NULL;
  char *p_formats=NULL;
  uint32_t cur_len=0;
  uint32_t vector_len=0;

  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;
  if(pp_formats==NULL) return e_XmountInput_Error_InvalidBuffer;

  // Loop over all loaded libs, extract supported formats and add to our vector
  // TODO: IMPROVEMENT: Final vector could be sorted
  for(uint32_t i=0;i<p_h->libs_count;i++) {
    p_buf=p_h->pp_libs[i]->p_supported_input_types;
    while(p_buf!=NULL && *p_buf!='\0') p_buf+=(strlen(p_buf)+1);
    cur_len=(uint32_t)(p_buf-p_h->pp_libs[i]->p_supported_input_types);
    if(cur_len==0) continue;
    p_formats=(char*)realloc(p_formats,vector_len+cur_len);
    if(p_formats==NULL) return e_XmountInput_Error_Alloc;
    memcpy(p_formats+vector_len,
           p_h->pp_libs[i]->p_supported_input_types,
           cur_len);
    vector_len+=cur_len;
  }

  // Null-terminate vector
  p_formats=(char*)realloc(p_formats,vector_len+1);
  if(p_formats==NULL) return e_XmountInput_Error_Alloc;
  p_formats[vector_len]='\0';

  *pp_formats=p_formats;
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_GetOptionsHelpText
 */
te_XmountInput_Error XmountInput_GetOptionsHelpText(pts_XmountInputHandle p_h,
                                                    char **pp_help_text)
{
  const char *p_buf=NULL;
  char *p_help=NULL;
  int ret=0;

  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;
  if(pp_help_text==NULL) return e_XmountInput_Error_InvalidBuffer;

  // Loop over all loaded libs, extract help and add to our text buffer
  // TODO: IMPROVEMENT: Final text should be sorted by lib's name
  for(uint32_t i=0;i<p_h->libs_count;i++) {
    ret=p_h->pp_libs[i]->lib_functions.OptionsHelp(&p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to get options help for library '%s': %s!\n",
                p_h->pp_libs[i]->p_name,
                p_h->pp_libs[i]->lib_functions.GetErrorMessage(ret));
      continue;
    }
    if(p_buf==NULL) continue;
    XMOUNT_STRAPP(p_help,"  - ");
    XMOUNT_STRAPP(p_help,p_h->pp_libs[i]->p_name);
    XMOUNT_STRAPP(p_help,"\n");
    XMOUNT_STRAPP(p_help,p_buf);
    XMOUNT_STRAPP(p_help,"\n");
    ret=p_h->pp_libs[i]->lib_functions.FreeBuffer(p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to free options help text from library '%s': %s!\n",
                p_h->pp_libs[i]->p_name,
                p_h->pp_libs[i]->lib_functions.GetErrorMessage(ret));
    }
  }

  *pp_help_text=p_help;
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_AddImage
 */
te_XmountInput_Error XmountInput_AddImage(pts_XmountInputHandle p_h,
                                          const char *p_format,
                                          uint64_t files_count,
                                          const char **pp_files)
{
  pts_XmountInputImage p_image=NULL;

  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;
  if(p_format==NULL) return e_XmountInput_Error_InvalidString;
  if(pp_files==NULL) return e_XmountInput_Error_InvalidArray;

  // Alloc and fill new image structure
  XMOUNT_CALLOC(p_image,pts_XmountInputImage,sizeof(ts_XmountInputImage));
  XMOUNT_STRSET(p_image->p_type,p_format);
  p_image->files_count=files_count;
  XMOUNT_CALLOC(p_image->pp_files,char**,p_image->files_count*sizeof(char*));
  for(uint64_t i=0;i<p_image->files_count;i++) {
    XMOUNT_STRSET(p_image->pp_files[i],pp_files[i]);
  }
  p_image->p_functions=NULL;
  p_image->p_handle=NULL;

  // TODO: Find lib and open input file

  // Add image to our image array
  XMOUNT_REALLOC(p_h->pp_images,pts_XmountInputImage*,p_h->images_count+1);
  p_h->pp_images[p_h->images_count++]=p_image;

  return e_XmountInput_Error_None;
}

/*
 * XmountInput_SetInputOffset
 */
te_XmountInput_Error XmountInput_SetInputOffset(pts_XmountInputHandle p_h,
                                                uint64_t offset)
{
  // TODO: Implement
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_SetInputSizeLimit
 */
te_XmountInput_Error XmountInput_SetInputSizeLimit(pts_XmountInputHandle p_h,
                                                   uint64_t size_limit)
{
  // TODO: Implement
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_GetSize
 */
te_XmountInput_Error XmountInput_GetSize(pts_XmountInputHandle p_h,
                                         uint64_t image_nr,
                                         uint64_t *p_size)
{
  // TODO: Implement
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_ReadData
 */
te_XmountInput_Error XmountInput_ReadData(pts_XmountInputHandle p_h,
                                          uint64_t image_nr,
                                          char *p_buf,
                                          uint64_t offset,
                                          uint64_t count)
{
  // TODO: Implement
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_WriteData
 */
te_XmountInput_Error XmountInput_WriteData(pts_XmountInputHandle p_h,
                                           uint64_t image_nr,
                                           const char *p_buf,
                                           uint64_t offset,
                                           uint64_t count)
{
  // TODO: Implement
  return e_XmountInput_Error_None;
}

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
/*
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
*/
