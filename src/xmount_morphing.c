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
#include <dlfcn.h> // For dlopen, dlclose, dlsym

#include "../libxmount_morphing/libxmount_morphing.h"
#include "xmount_morphing.h"
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

//! Structure containing infos about morphing libs
typedef struct s_XmountMorphingLib {
  //! Filename of lib (without path)
  char *p_name;
  //! Handle to the loaded lib
  void *p_lib;
  //! Array of supported morphing types
  char *p_supported_morphing_types;
  //! Struct containing lib functions
  ts_LibXmountMorphingFunctions lib_functions;
} ts_XmountMorphingLib, *pts_XmountMorphingLib;

//! Structures and vars needed for morph support
typedef struct s_XmountMorphHandle {
  //! Loaded morphing lib count
  uint32_t libs_count;
  //! Array containing infos about loaded morphing libs
  pts_XmountMorphingLib *pp_libs;
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
} ts_XmountMorphHandle;

/*******************************************************************************
 * Private functions declarations
 ******************************************************************************/
/*!
 * \brief Find a morphing lib for a given morph type
 *
 * Searches trough the list of loaded morphing libraries to find one that
 * supports the given morph type. On success, that library is associated with
 * the given handle.
 *
 * \param p_h Morphing handle
 * \return e_XmountInput_Error_None on success
 */
te_XmountMorphError XmountMorphing_FindMorphLib(pts_XmountMorphHandle p_h);

/*******************************************************************************
 * Public functions implementations
 ******************************************************************************/
/*
 * XmountMorphing_CreateHandle
 */
te_XmountMorphError XmountMorphing_CreateHandle(pts_XmountMorphHandle *pp_h) {
  pts_XmountMorphHandle p_h=NULL;

  // Params check
  if(pp_h==NULL) return e_XmountMorphError_InvalidHandlePointer;

  // Alloc new handle
  p_h=(pts_XmountMorphHandle)calloc(1,sizeof(ts_XmountMorphHandle));
  if(p_h==NULL) return e_XmountMorphError_Alloc;

  // Init values
  p_h->pp_libs=NULL;
  p_h->p_morph_type=NULL;
  p_h->pp_lib_params=NULL;
  p_h->p_handle=NULL;
  p_h->p_functions=NULL;
  p_h->input_image_functions.ImageCount=&LibXmount_Morphing_ImageCount;
  p_h->input_image_functions.Size=&LibXmount_Morphing_Size;
  p_h->input_image_functions.Read=&LibXmount_Morphing_Read;

  *pp_h=p_h;
  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_DestroyHandle
 */
te_XmountMorphError XmountMorphing_DestroyHandle(pts_XmountMorphHandle *pp_h) {
  // Params check
  if(pp_h==NULL) return e_XmountMorphError_InvalidHandlePointer;

  // TODO: Impement

  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_EnableDebugging
 */
te_XmountMorphError XmountMorphing_EnableDebugging(pts_XmountMorphHandle p_h) {
  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;

  // TODO: Impement

  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_AddLibrary
 */
te_XmountMorphError XmountMorphing_AddLibrary(pts_XmountMorphHandle p_h,
                                              const char *p_lib_name)
{
  uint32_t supported_types_len=0;
  t_LibXmount_Morphing_GetApiVersion pfun_morphing_GetApiVersion;
  t_LibXmount_Morphing_GetSupportedTypes pfun_morphing_GetSupportedTypes;
  t_LibXmount_Morphing_GetFunctions pfun_morphing_GetFunctions;
  void *p_libxmount=NULL;
  pts_XmountMorphingLib p_morphing_lib=NULL;
  char *p_buf=NULL;
  char *p_library_path=NULL;
  char *p_supported_types=NULL;

  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;
  if(p_lib_name==NULL) return e_XmountMorphError_InvalidString;

  // Construct full library path
  XMOUNT_STRSET(p_library_path,XMOUNT_LIBRARY_PATH);
  if(p_library_path[strlen(p_library_path)]!='/') {
    XMOUNT_STRAPP(p_library_path,"/");
  }
  XMOUNT_STRAPP(p_library_path,p_lib_name);

#define XMOUNTMORPHING_LOADLIBS__LOAD_SYMBOL(name,pfun) {        \
  if((pfun=dlsym(p_libxmount,name))==NULL) {                     \
    LOG_ERROR("Unable to load symbol '%s' from library '%s'!\n", \
              name,                                              \
              p_library_path);                                   \
    dlclose(p_libxmount);                                        \
    return e_XmountMorphError_FailedLoadingSymbol;               \
  }                                                              \
}

  // Try to load given library
  p_libxmount=dlopen(p_library_path,RTLD_NOW);
  if(p_libxmount==NULL) {
    LOG_ERROR("Unable to load morphing library '%s': %s!\n",
              p_library_path,
              dlerror());
    return e_XmountMorphError_FailedLoadingLibrary;
  }

  // Load library symbols
  XMOUNTMORPHING_LOADLIBS__LOAD_SYMBOL("LibXmount_Morphing_GetApiVersion",
                                       pfun_morphing_GetApiVersion);

  // Check library's API version
  if(pfun_morphing_GetApiVersion()!=LIBXMOUNT_MORPHING_API_VERSION) {
    LOG_DEBUG("Failed! Wrong API version.\n");
    LOG_ERROR("Unable to load morphing library '%s'. Wrong API version\n",
              p_library_path);
    dlclose(p_libxmount);
    return e_XmountMorphError_WrongLibraryApiVersion;
  }

  LIBXMOUNT_LOAD_SYMBOL("LibXmount_Morphing_GetSupportedTypes",
                        pfun_morphing_GetSupportedTypes);
  LIBXMOUNT_LOAD_SYMBOL("LibXmount_Morphing_GetFunctions",
                        pfun_morphing_GetFunctions);

  // Construct new entry for our library list
  XMOUNT_MALLOC(p_morphing_lib,
                pts_XmountMorphingLib,
                sizeof(ts_XmountMorphingLib));
  // Initialize lib_functions structure to NULL
  memset(&(p_morphing_lib->lib_functions),
         0,
         sizeof(ts_LibXmountMorphingFunctions));

  // Set name and handle
  XMOUNT_STRSET(p_morphing_lib->p_name,p_lib_name);
  p_morphing_lib->p_lib=p_libxmount;

  // Get and set supported types
  p_supported_types=pfun_morphing_GetSupportedTypes();
  supported_types_len=0;
  p_buf=p_supported_types;
  while(*p_buf!='\0') {
    supported_types_len+=(strlen(p_buf)+1);
    p_buf+=(strlen(p_buf)+1);
  }
  supported_types_len++;
  XMOUNT_MALLOC(p_morphing_lib->p_supported_morphing_types,
                char*,
                supported_types_len);
  memcpy(p_morphing_lib->p_supported_morphing_types,
         p_supported_types,
         supported_types_len);

  // Get, set and check lib_functions
  pfun_morphing_GetFunctions(&(p_morphing_lib->lib_functions));
  if(p_morphing_lib->lib_functions.CreateHandle==NULL ||
     p_morphing_lib->lib_functions.DestroyHandle==NULL ||
     p_morphing_lib->lib_functions.Morph==NULL ||
     p_morphing_lib->lib_functions.Size==NULL ||
     p_morphing_lib->lib_functions.Read==NULL ||
     p_morphing_lib->lib_functions.OptionsHelp==NULL ||
     p_morphing_lib->lib_functions.OptionsParse==NULL ||
     p_morphing_lib->lib_functions.GetInfofileContent==NULL ||
     p_morphing_lib->lib_functions.GetErrorMessage==NULL ||
     p_morphing_lib->lib_functions.FreeBuffer==NULL)
  {
    LOG_DEBUG("Missing implemention of one or more functions in lib %s!\n",
              p_lib_name);
    XMOUNT_FREE(p_morphing_lib->p_supported_morphing_types);
    XMOUNT_FREE(p_morphing_lib->p_name);
    XMOUNT_FREE(p_morphing_lib);
    dlclose(p_libxmount);
    return e_XmountMorphError_MissingLibraryFunction;
  }

  // Add entry to the input library list
  XMOUNT_REALLOC(p_h->pp_libs,
                 pts_XmountMorphingLib*,
                 sizeof(pts_XmountMorphingLib)*(p_h->libs_count+1));
  p_h->pp_libs[p_h->libs_count++]=p_morphing_lib;

  LOG_DEBUG("Morphing library '%s' loaded successfully\n",p_lib_name);

  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_GetLibraryCount
 */
te_XmountMorphError XmountMorphing_GetLibraryCount(pts_XmountMorphHandle p_h,
                                                   uint32_t *p_count)
{
  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;

  // TODO: Impement

  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_GetSupportedTypes
 */
te_XmountMorphError XmountMorphing_GetSupportedTypes(pts_XmountMorphHandle p_h,
                                                     char **pp_types)
{
  char *p_buf=NULL;
  char *p_types=NULL;
  uint32_t cur_len=0;
  uint32_t vector_len=0;

  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;
  if(pp_types==NULL) return e_XmountMorphError_InvalidBuffer;

  // Loop over all loaded libs, extract supported types and add to our vector
  // TODO: IMPROVEMENT: Final vector could be sorted
  for(uint32_t i=0;i<p_h->libs_count;i++) {
    p_buf=p_h->pp_libs[i]->p_supported_morphing_types;
    while(p_buf!=NULL && *p_buf!='\0') p_buf+=(strlen(p_buf)+1);
    cur_len=(uint32_t)(p_buf-p_h->pp_libs[i]->p_supported_morphing_types);
    if(cur_len==0) continue;
    p_types=(char*)realloc(p_types,vector_len+cur_len);
    if(p_types==NULL) return e_XmountInput_Error_Alloc;
    memcpy(p_types+vector_len,
           p_h->pp_libs[i]->p_supported_morphing_types,
           cur_len);
    vector_len+=cur_len;
  }

  // Null-terminate vector
  p_types=(char*)realloc(p_types,vector_len+1);
  if(p_types==NULL) return e_XmountInput_Error_Alloc;
  p_types[vector_len]='\0';

  *pp_types=p_types;
  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_SetOptions
 */
te_XmountMorphError XmountMorphing_SetOptions(pts_XmountMorphHandle p_h,
                                              char *p_options)
{
  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;
  if(p_options==NULL) return e_XmountMorphError_InvalidString;

  // Make sure library parameters haven't been set previously
  if(p_h->pp_lib_params!=NULL) {
    LOG_ERROR("Morphing library options already set!\n");
    return e_XmountMorphError_LibOptionsAlreadySet;
  }

  // Save options
  if(XmountLib_SplitLibParams(p_options,
                              &(p_h->lib_params_count),
                              &(p_h->pp_lib_params))!=0)
  {
    LOG_ERROR("Unable to parse morphing library options '%s'!\n",p_options);
    return e_XmountMorphError_FailedParsingOptions;
  }

  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_GetOptionsHelpText
 */
te_XmountMorphError XmountMorphing_GetOptionsHelpText(pts_XmountMorphHandle p_h,
                                                      char **pp_help_text)
{
  const char *p_buf=NULL;
  char *p_help=NULL;
  int ret=0;

  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;
  if(pp_help_text==NULL) return e_XmountMorphError_InvalidBuffer;

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
    p_h->pp_libs[i]->lib_functions.FreeBuffer(p_buf);
  }

  *pp_help_text=p_help;
  return e_XmountMorphError_None;
}

/*
 * XmountMorphingt_GetLibsInfoText
 */
te_XmountMorphError XmountMorphing_GetLibsInfoText(pts_XmountMorphHandle p_h,
                                                   char **pp_info_text)
{
  char *p_buf=NULL;
  char *p_info_text=NULL;
  uint8_t first=0;

  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;
  if(pp_info_text==NULL) return e_XmountMorphError_InvalidBuffer;

  // Loop over all loaded libs, extract name and supported types and add to
  // our text buffer
  // TODO: IMPROVEMENT: Final text should be sorted by lib's name
  for(uint32_t i=0;i<p_h->libs_count;i++) {
    XMOUNT_STRAPP(p_info_text,"    - ");
    XMOUNT_STRAPP(p_info_text,p_h->pp_libs[i]->p_name);
    XMOUNT_STRAPP(p_info_text," supporting ");
    XMOUNT_STRAPP(p_info_text,"    - ");
    XMOUNT_STRAPP(p_info_text,"    - ");
    p_buf=p_h->pp_libs[i]->p_supported_morphing_types;
    first=1;
    while(*p_buf!='\0') {
      if(first==1) {
        XMOUNT_STRAPP(p_info_text,"\"");
        XMOUNT_STRAPP(p_info_text,p_buf);
        XMOUNT_STRAPP(p_info_text,"\"");
        first=0;
      } else {
        XMOUNT_STRAPP(p_info_text,", \"");
        XMOUNT_STRAPP(p_info_text,p_buf);
        XMOUNT_STRAPP(p_info_text,"\"");
      }
      p_buf+=(strlen(p_buf)+1);
    }
    XMOUNT_STRAPP(p_info_text,"\n");
  }

  *pp_info_text=p_info_text;
  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_SetType
 */
te_XmountMorphError XmountMorphing_SetType(pts_XmountMorphHandle p_h,
                                           char *p_type)
{
  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;
  if(p_type==NULL) return e_XmountMorphError_InvalidString;

  XMOUNT_STRSET(p_h->p_morph_type,p_type);
  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_StartMorph
 */
te_XmountMorphError XmountMorphing_StartMorph(pts_XmountMorphHandle p_h) {
  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;

  // TODO: Impement

  return e_XmountMorphError_None;
}

/*
 * XmountInput_StopMorph
 */
te_XmountMorphError XmountInput_StopMorph(pts_XmountMorphHandle p_h) {
  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;

  // TODO: Impement

  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_GetSize
 */
te_XmountMorphError XmountMorphing_GetSize(pts_XmountMorphHandle p_h,
                                           uint64_t *p_size)
{
  int ret=0;

  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;
  if(p_size==NULL) return e_XmountMorphError_InvalidBuffer;

  // Get morphed image size
  ret=p_h->p_functions->Size(p_h->p_handle,p_size);
  if(ret!=0) {
    LOG_ERROR("Unable to get morphed image size: %s!\n",
              p_h->p_functions->GetErrorMessage(ret));
    return e_XmountMorphError_FailedGettingImageSize;
  }

  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_ReadData
 */
te_XmountMorphError XmountMorphing_ReadData(pts_XmountMorphHandle p_h,
                                            char *p_buf,
                                            uint64_t offset,
                                            uint64_t count,
                                            uint64_t *p_read)
{
  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;

  // TODO: Impement

  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_WriteData
 */
te_XmountMorphError XmountMorphing_WriteData(pts_XmountMorphHandle p_h,
                                             const char *p_buf,
                                             uint64_t offset,
                                             uint64_t count)
{
  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;

  // TODO: Impement

  return e_XmountMorphError_None;
}

/*
 * XmountMorphing_GetInfoFileContent
 */
te_XmountMorphError XmountMorphing_GetInfoFileContent(pts_XmountMorphHandle p_h,
                                                      char **pp_content)
{
  int ret=0;
  char *p_buf=NULL;
  char *p_content=NULL;

  // Params check
  if(p_h==NULL) return e_XmountMorphError_InvalidHandle;
  if(pp_content==NULL) return e_XmountMorphError_InvalidBuffer;

  ret=p_h->p_functions->GetInfofileContent(p_h->p_handle,(const char**)&p_buf);
  if(ret!=0) {
    LOG_ERROR("Unable to get info file content from morphing lib: %s!\n",
              p_h->p_functions->GetErrorMessage(ret));
    return e_XmountMorphError_FailedGettingInfoFileContent;
  }
  // Add infos to main buffer and free p_buf
  if(p_buf!=NULL) {
    XMOUNT_STRAPP(p_content,p_buf);
    p_h->p_functions->FreeBuffer(p_buf);
  } else {
    XMOUNT_STRAPP(p_content,"None\n");
  }

  *pp_content=p_content;
  return e_XmountMorphError_None;
}

/*******************************************************************************
 * Private functions implementations
 ******************************************************************************/
/*
 * XmountMorphing_FindMorphLib
 */
te_XmountMorphError XmountMorphing_FindMorphLib(pts_XmountMorphHandle p_h) {
  char *p_buf;

  LOG_DEBUG("Trying to find suitable library for morph type '%s'.\n",
            p_h->p_morph_type);

  // Loop over all loaded libs
  for(uint32_t i=0;i<p_h->libs_count;i++) {
    LOG_DEBUG("Checking morphing library %s\n",p_h->pp_libs[i]->p_name);
    p_buf=p_h->pp_libs[i]->p_supported_morphing_types;
    while(*p_buf!='\0') {
      if(strcmp(p_buf,p_h->p_morph_type)==0) {
        // Library supports morph type, set lib functions
        LOG_DEBUG("Morphing library '%s' pretends to handle that morph type.\n",
                  p_h->pp_libs[i]->p_name);
        p_h->p_functions=&(p_h->pp_libs[i]->lib_functions);
        return e_XmountMorphError_None;
      }
      p_buf+=(strlen(p_buf)+1);
    }
  }

  LOG_DEBUG("Couldn't find any suitable library.\n");

  // No library supporting morph type found
  return e_XmountInput_Error_UnsupportedType;
}


//! Read data from morphed image
/*!
 * \param p_buf Pointer to buffer to write read data to (must be preallocated!)
 * \param offset Offset at which data should be read
 * \param size Size of data which should be read (size of buffer)
 * \param p_read Number of read bytes on success
 * \return TRUE on success, negated error code on error
 */
/*
int ReadMorphedImageData(char *p_buf,
                                off_t offset,
                                size_t size,
                                size_t *p_read)
{
  uint64_t block_off=0;
  uint64_t cur_block=0;
  uint64_t cur_to_read=0;
  uint64_t image_size=0;
  size_t read=0;
  size_t to_read=0;
  int ret;
  uint8_t is_block_cached=FALSE;
  te_XmountCache_Error cache_ret=e_XmountCache_Error_None;

  // Make sure we aren't reading past EOF of image file
  if(GetMorphedImageSize(&image_size)!=TRUE) {
    LOG_ERROR("Couldn't get size of morphed image!\n");
    return -EIO;
  }
  if(offset>=image_size) {
    // Offset is beyond image size
    LOG_DEBUG("Offset %zu is at / beyond size of morphed image.\n",offset);
    *p_read=0;
    return FALSE;
  }
  if(offset+size>image_size) {
    // Attempt to read data past EOF of morphed image file
    to_read=image_size-offset;
    LOG_DEBUG("Attempt to read data past EOF of morphed image. Corrected size "
                "from %zu to %" PRIu64 ".\n",
              size,
              to_read);
  } else to_read=size;

  // Calculate block to start reading data from
  cur_block=offset/XMOUNT_CACHE_BLOCK_SIZE;
  block_off=offset%XMOUNT_CACHE_BLOCK_SIZE;

  // Read image data
  while(to_read!=0) {
    // Calculate how many bytes we have to read from this block
    if(block_off+to_read>XMOUNT_CACHE_BLOCK_SIZE) {
      cur_to_read=XMOUNT_CACHE_BLOCK_SIZE-block_off;
    } else cur_to_read=to_read;

    // Determine if we have to read cached data
    is_block_cached=FALSE;
    if(glob_xmount.output.writable==TRUE) {
      cache_ret=XmountCache_IsBlockCached(glob_xmount.h_cache,cur_block);
      if(cache_ret==e_XmountCache_Error_None) is_block_cached=TRUE;
      else if(cache_ret!=e_XmountCache_Error_UncachedBlock) {
        LOG_ERROR("Unable to determine if block %" PRIu64 " is cached: "
                    "Error code %u!\n",
                  cur_block,
                  cache_ret);
        return -EIO;
      }
    }

    // Check if block is cached
    if(is_block_cached==TRUE) {
      // Write support enabled and need to read altered data from cachefile
      LOG_DEBUG("Reading %zu bytes from block cache file\n",cur_to_read);

      cache_ret=XmountCache_BlockCacheRead(glob_xmount.h_cache,
                                           p_buf,
                                           cur_block,
                                           block_off,
                                           cur_to_read);
      if(cache_ret!=e_XmountCache_Error_None) {
        LOG_ERROR("Unable to read %" PRIu64
                    " bytes of cached data from cache block %" PRIu64
                    " at cache block offset %" PRIu64 ": Error code %u!\n",
                  cur_to_read,
                  cur_block,
                  block_off,
                  cache_ret);
        return -EIO;
      }
    } else {
      // No write support or data not cached
      ret=glob_xmount.morphing.p_functions->Read(glob_xmount.morphing.p_handle,
                                                 p_buf,
                                                 (cur_block*XMOUNT_CACHE_BLOCK_SIZE)+
                                                   block_off,
                                                 cur_to_read,
                                                 &read);
      if(ret!=0 || read!=cur_to_read) {
        LOG_ERROR("Couldn't read %zu bytes at offset %zu from morphed image: "
                    "%s!\n",
                  cur_to_read,
                  offset,
                  glob_xmount.morphing.p_functions->GetErrorMessage(ret));
        return -EIO;
      }
      LOG_DEBUG("Read %" PRIu64 " bytes at offset %" PRIu64
                  " from morphed image file\n",
                cur_to_read,
                (cur_block*XMOUNT_CACHE_BLOCK_SIZE)+block_off);
    }

    cur_block++;
    block_off=0;
    p_buf+=cur_to_read;
    to_read-=cur_to_read;
  }

  *p_read=to_read;
  return TRUE;
}
*/

//! Write data to morphed image
/*!
 * \param p_buf Buffer with data to write
 * \param offset Offset to start writing at
 * \param count Amount of bytes to write
 * \param p_written Amount of successfully written bytes
 * \return TRUE on success, negated error code on error
 */
/*
int WriteMorphedImageData(const char *p_buf,
                                 off_t offset,
                                 size_t count,
                                 size_t *p_written)
{
  uint64_t block_off=0;
  uint64_t cur_block=0;
  uint64_t cur_to_read=0;
  uint64_t cur_to_write=0;
  uint64_t image_size=0;
  uint64_t read=0;
  size_t written=0;
  size_t to_write=0;
  int ret;
  char *p_buf2=NULL;
  uint8_t is_block_cached=FALSE;
  te_XmountCache_Error cache_ret=e_XmountCache_Error_None;

  // Make sure we aren't writing past EOF of image file
  if(GetMorphedImageSize(&image_size)!=TRUE) {
    LOG_ERROR("Couldn't get size of morphed image!\n");
    return -EIO;
  }
  if(offset>=image_size) {
    // Offset is beyond image size
    LOG_DEBUG("Offset %zu is at / beyond size of morphed image.\n",offset);
    *p_written=0;
    return 0;
  }
  if(offset+count>image_size) {
    // Attempt to write data past EOF of morphed image file
    to_write=image_size-offset;
    LOG_DEBUG("Attempt to write data past EOF of morphed image. Corrected size "
                "from %zu to %" PRIu64 ".\n",
              count,
              to_write);
  } else to_write=count;

  // Calculate block to start writing data to
  cur_block=offset/XMOUNT_CACHE_BLOCK_SIZE;
  block_off=offset%XMOUNT_CACHE_BLOCK_SIZE;

  while(to_write!=0) {
    // Calculate how many bytes we have to write to this block
    if(block_off+to_write>XMOUNT_CACHE_BLOCK_SIZE) {
      cur_to_write=XMOUNT_CACHE_BLOCK_SIZE-block_off;
    } else cur_to_write=to_write;

    // Determine if block is already in cache
    is_block_cached=FALSE;
    cache_ret=XmountCache_IsBlockCached(glob_xmount.h_cache,cur_block);
    if(cache_ret==e_XmountCache_Error_None) is_block_cached=TRUE;
    else if(cache_ret!=e_XmountCache_Error_UncachedBlock) {
      LOG_ERROR("Unable to determine if block %" PRIu64 " is cached: "
                  "Error code %u!\n",
                cur_block,
                cache_ret);
      return -EIO;
    }

    // Check if block is cached
    if(is_block_cached==TRUE) {
      // Block is cached
      cache_ret=XmountCache_BlockCacheWrite(glob_xmount.h_cache,
                                            p_buf,
                                            cur_block,
                                            block_off,
                                            cur_to_write);
      if(cache_ret!=e_XmountCache_Error_None) {
        LOG_ERROR("Unable to write %" PRIu64
                    " bytes of data to cache block %" PRIu64
                    " at cache block offset %" PRIu64 ": Error code %u!\n",
                  cur_to_write,
                  cur_block,
                  block_off,
                  cache_ret);
        return -EIO;
      }

      LOG_DEBUG("Wrote %" PRIu64 " bytes to block cache\n",cur_to_write);
    } else {
      // Uncached block. Need to cache entire new block
      // Prepare new write buffer
      XMOUNT_MALLOC(p_buf2,char*,XMOUNT_CACHE_BLOCK_SIZE);
      memset(p_buf2,0x00,XMOUNT_CACHE_BLOCK_SIZE);

      // Read full block from morphed image
      cur_to_read=XMOUNT_CACHE_BLOCK_SIZE;
      if((cur_block*XMOUNT_CACHE_BLOCK_SIZE)+XMOUNT_CACHE_BLOCK_SIZE>image_size) {
        cur_to_read=XMOUNT_CACHE_BLOCK_SIZE-(((cur_block*XMOUNT_CACHE_BLOCK_SIZE)+
                                         XMOUNT_CACHE_BLOCK_SIZE)-image_size);
      }
      ret=glob_xmount.morphing.p_functions->Read(glob_xmount.morphing.p_handle,
                                                 p_buf2,
                                                 cur_block*XMOUNT_CACHE_BLOCK_SIZE,
                                                 cur_to_read,
                                                 &read);
      if(ret!=0 || read!=cur_to_read) {
        LOG_ERROR("Couldn't read %" PRIu64 " bytes at offset %zu "
                    "from morphed image: %s!\n",
                  cur_to_read,
                  offset,
                  glob_xmount.morphing.p_functions->GetErrorMessage(ret));
        XMOUNT_FREE(p_buf2);
        return -EIO;
      }

      // Set changed data
      memcpy(p_buf2+block_off,p_buf,cur_to_write);

      cache_ret=XmountCache_BlockCacheAppend(glob_xmount.h_cache,
                                             p_buf,
                                             cur_block);
      if(cache_ret!=e_XmountCache_Error_None) {
        LOG_ERROR("Unable to append new block cache block %" PRIu64
                    ": Error code %u!\n",
                  cur_block,
                  cache_ret);
        XMOUNT_FREE(p_buf2);
        return -EIO;
      }
      XMOUNT_FREE(p_buf2);

      LOG_DEBUG("Appended new block cache block %" PRIu64 "\n",cur_block);
    }

    block_off=0;
    cur_block++;
    p_buf+=cur_to_write;
    to_write-=cur_to_write;
  }

  *p_written=to_write;
  return TRUE;
}
*/
