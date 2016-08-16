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

#include "xmount_input.h"
#include "xmount.h"
#include "macros.h"
#include "../libxmount_input/libxmount_input.h"

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
  //! Enable debugging
  uint8_t debug;
} ts_XmountInputHandle;

/*******************************************************************************
 * Private functions declarations
 ******************************************************************************/
/*!
 * \brief Find an input lib for a given input image
 *
 * Searches trough the list of loaded input libraries to find one that supports
 * the given input image's format. On success, that library is associated with
 * the given image.
 *
 * \param p_h Input handle
 * \param p_input_image Input image to search input lib for
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_FindLib(pts_XmountInputHandle p_h,
                                         pts_XmountInputImage p_input_image);

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
  if(p_h->images_count!=0 && p_h->pp_images!=NULL) {
    // TODO: Close images
  }
  if(p_h->libs_count>0 && p_h->pp_libs!=NULL) {
    // Unload all input libs
    for(uint32_t i=0;i<p_h->libs_count;i++) {
      if(p_h->pp_libs[i]->p_supported_input_types!=NULL) {
        XMOUNT_FREE(p_h->pp_libs[i]->p_supported_input_types);
      }
      if(p_h->pp_libs[i]->p_lib!=NULL) {
        dlclose(p_h->pp_libs[i]->p_lib);
        p_h->pp_libs[i]->p_lib=NULL;
      }
      if(p_h->pp_libs[i]->p_name!=NULL) {
        XMOUNT_FREE(p_h->pp_libs[i]->p_name);
      }
      XMOUNT_FREE(p_h->pp_libs[i]);
    }
    XMOUNT_FREE(p_h->pp_libs);
    p_h->libs_count=0;
  }
  if(p_h->lib_params_count!=0 && p_h->pp_lib_params!=NULL) {
    // Free library parameter array
    for(uint32_t i=0;i<p_h->lib_params_count;i++) {
      XMOUNT_FREE(p_h->pp_lib_params[i]);
    }
    XMOUNT_FREE(p_h->pp_lib_params);
    p_h->lib_params_count=0;
  }

  return e_XmountInput_Error_None;
}

/*
 * XmountInput_EnableDebugging
 */
te_XmountInput_Error XmountInput_EnableDebugging(pts_XmountInputHandle p_h) {
  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;

  p_h->debug=1;
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_AddLibrary
 */
te_XmountInput_Error XmountInput_AddLibrary(pts_XmountInputHandle p_h,
                                            const char *p_lib_name)
{
  uint32_t supported_formats_len=0;
  t_LibXmount_Input_GetApiVersion pfun_input_GetApiVersion;
  t_LibXmount_Input_GetSupportedFormats pfun_input_GetSupportedFormats;
  t_LibXmount_Input_GetFunctions pfun_input_GetFunctions;
  void *p_libxmount=NULL;
  pts_XmountInputLib p_input_lib=NULL;
  char *p_buf=NULL;
  char *p_library_path=NULL;
  char *p_supported_formats=NULL;

  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;
  if(p_lib_name==NULL) return e_XmountInput_Error_InvalidString;

  // Construct full library path
  XMOUNT_STRSET(p_library_path,XMOUNT_LIBRARY_PATH);
  if(p_library_path[strlen(p_library_path)]!='/') {
    XMOUNT_STRAPP(p_library_path,"/");
  }
  XMOUNT_STRAPP(p_library_path,p_lib_name);

#define XMOUNTINPUT_LOADLIBS__LOAD_SYMBOL(name,pfun) {           \
  if((pfun=dlsym(p_libxmount,name))==NULL) {                     \
    LOG_ERROR("Unable to load symbol '%s' from library '%s'!\n", \
              name,                                              \
              p_library_path);                                   \
    dlclose(p_libxmount);                                        \
    return e_XmountInput_Error_FailedLoadingSymbol;              \
  }                                                              \
}

  // Try to load given library
  p_libxmount=dlopen(p_library_path,RTLD_NOW);
  if(p_libxmount==NULL) {
    LOG_ERROR("Unable to load input library '%s': %s!\n",
              p_library_path,
              dlerror());
    return e_XmountInput_Error_FailedLoadingLibrary;
  }

  // Load library symbols
  XMOUNTINPUT_LOADLIBS__LOAD_SYMBOL("LibXmount_Input_GetApiVersion",
                                    pfun_input_GetApiVersion);

  // Check library's API version
  if(pfun_input_GetApiVersion()!=LIBXMOUNT_INPUT_API_VERSION) {
    LOG_DEBUG("Failed! Wrong API version.\n");
    LOG_ERROR("Unable to load input library '%s'. Wrong API version\n",
              p_library_path);
    dlclose(p_libxmount);
    return e_XmountInput_Error_WrongLibraryApiVersion;
  }

  XMOUNTINPUT_LOADLIBS__LOAD_SYMBOL("LibXmount_Input_GetSupportedFormats",
                                    pfun_input_GetSupportedFormats);
  XMOUNTINPUT_LOADLIBS__LOAD_SYMBOL("LibXmount_Input_GetFunctions",
                                    pfun_input_GetFunctions);

  // Construct new entry for our library list
  XMOUNT_MALLOC(p_input_lib,pts_XmountInputLib,sizeof(ts_XmountInputLib));
  // Initialize lib_functions structure to NULL
  memset(&(p_input_lib->lib_functions),
         0,
         sizeof(ts_LibXmountInputFunctions));

  // Set name and handle
  XMOUNT_STRSET(p_input_lib->p_name,p_lib_name);
  p_input_lib->p_lib=p_libxmount;

  // Get and set supported formats
  p_supported_formats=pfun_input_GetSupportedFormats();
  p_buf=p_supported_formats;
  while(*p_buf!='\0') {
    supported_formats_len+=(strlen(p_buf)+1);
    p_buf+=(strlen(p_buf)+1);
  }
  supported_formats_len++;
  XMOUNT_MALLOC(p_input_lib->p_supported_input_types,
                char*,
                supported_formats_len);
  memcpy(p_input_lib->p_supported_input_types,
         p_supported_formats,
         supported_formats_len);

  // Get, set and check lib_functions
  pfun_input_GetFunctions(&(p_input_lib->lib_functions));
  if(p_input_lib->lib_functions.CreateHandle==NULL ||
     p_input_lib->lib_functions.DestroyHandle==NULL ||
     p_input_lib->lib_functions.Open==NULL ||
     p_input_lib->lib_functions.Close==NULL ||
     p_input_lib->lib_functions.Size==NULL ||
     p_input_lib->lib_functions.Read==NULL ||
     p_input_lib->lib_functions.OptionsHelp==NULL ||
     p_input_lib->lib_functions.OptionsParse==NULL ||
     p_input_lib->lib_functions.GetInfofileContent==NULL ||
     p_input_lib->lib_functions.GetErrorMessage==NULL ||
     p_input_lib->lib_functions.FreeBuffer==NULL)
  {
    LOG_DEBUG("Missing implemention of one or more functions in lib %s!\n",
              p_lib_name);
    XMOUNT_FREE(p_input_lib->p_supported_input_types);
    XMOUNT_FREE(p_input_lib->p_name);
    XMOUNT_FREE(p_input_lib);
    dlclose(p_libxmount);
    return e_XmountInput_Error_MissingLibraryFunction;
  }

  // Add entry to the input library list
  XMOUNT_REALLOC(p_h->pp_libs,
                 pts_XmountInputLib*,
                 sizeof(pts_XmountInputLib)*(p_h->libs_count+1));
  p_h->pp_libs[p_h->libs_count++]=p_input_lib;

  LOG_DEBUG("Input library '%s' loaded successfully\n",p_lib_name);

#undef XMOUNTINPUT_LOADLIBS__LOAD_SYMBOL

  return e_XmountInput_Error_None;
}

te_XmountInput_Error XmountInput_GetLibraryCount(pts_XmountInputHandle p_h,
                                                 uint32_t *p_count)
{
  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;
  if(p_count==NULL) return e_XmountInput_Error_InvalidBuffer;

  *p_count=p_h->libs_count;
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
 * XmountInput_SetOptions
 */
te_XmountInput_Error XmountInput_SetOptions(pts_XmountInputHandle p_h,
                                            char *p_options)
{
  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;
  if(p_options==NULL) return e_XmountInput_Error_InvalidString;

  // Make sure library parameters haven't been set previously
  if(p_h->pp_lib_params!=NULL) {
    LOG_ERROR("Input library options already set!\n");
    return e_XmountInput_Error_LibOptionsAlreadySet;
  }

  // Save options
  if(XmountLib_SplitLibParams(p_options,
                              &(p_h->lib_params_count),
                              &(p_h->pp_lib_params))!=0)
  {
    LOG_ERROR("Unable to parse input library options '%s'!\n",p_options);
    return e_XmountInput_Error_FailedParsingOptions;
  }

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
 * XmountInput_GetLibsInfoText
 */
te_XmountInput_Error XmountInput_GetLibsInfoText(pts_XmountInputHandle p_h,
                                                 char **pp_info_text)
{
  char *p_buf=NULL;
  char *p_info_text=NULL;
  uint8_t first=0;

  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;
  if(pp_info_text==NULL) return e_XmountInput_Error_InvalidBuffer;

  // Loop over all loaded libs, extract name and supported formats and add to
  // our text buffer
  // TODO: IMPROVEMENT: Final text should be sorted by lib's name
  for(uint32_t i=0;i<p_h->libs_count;i++) {
    XMOUNT_STRAPP(p_info_text,"    - ");
    XMOUNT_STRAPP(p_info_text,p_h->pp_libs[i]->p_name);
    XMOUNT_STRAPP(p_info_text," supporting ");
    XMOUNT_STRAPP(p_info_text,"    - ");
    XMOUNT_STRAPP(p_info_text,"    - ");
    p_buf=p_h->pp_libs[i]->p_supported_input_types;
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

/*!
 * \brief Get input image count
 *
 * Get input image count.
 *
 * \param p_h Input handle
 * \param p_count Input image count is returned in this variable
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_GetImageCount(pts_XmountInputHandle p_h,
                                               uint64_t *p_count)
{
  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;
  if(p_count==NULL) return e_XmountInput_Error_InvalidBuffer;

  *p_count=p_h->images_count;
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_SetInputOffset
 */
te_XmountInput_Error XmountInput_SetInputOffset(pts_XmountInputHandle p_h,
                                                uint64_t offset)
{
  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;

  LOG_DEBUG("Setting input image offset to \"%" PRIu64 "\"\n",offset);

  p_h->image_offset=offset;
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_SetInputSizeLimit
 */
te_XmountInput_Error XmountInput_SetInputSizeLimit(pts_XmountInputHandle p_h,
                                                   uint64_t size_limit)
{
  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;

  LOG_DEBUG("Setting input image size limit to \"%" PRIu64 "\"\n",size_limit);

  p_h->image_size_limit=size_limit;
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_Open
 */
te_XmountInput_Error XmountInput_Open(pts_XmountInputHandle p_h) {
  int ret=0;
  const char *p_err_msg;
  te_XmountInput_Error input_ret=e_XmountInput_Error_None;

  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;

  for(uint64_t i=0;i<p_h->images_count;i++) {
    if(p_h->debug==TRUE) {
      if(p_h->pp_images[i]->files_count==1) {
        LOG_DEBUG("Loading image file \"%s\"...\n",
                  p_h->pp_images[i]->pp_files[0]);
      } else {
        LOG_DEBUG("Loading image files \"%s .. %s\"...\n",
                  p_h->pp_images[i]->pp_files[0],
                  p_h->pp_images[i]->
                    pp_files[p_h->pp_images[i]->files_count-1]);
      }
    }

    // Find input lib
    input_ret=XmountInput_FindLib(p_h,p_h->pp_images[i]);
    if(input_ret!=e_XmountInput_Error_None) {
      LOG_ERROR("Unable to find input library for input image format '%s' "
                  "of input image '%s': Error code %u!\n",
                p_h->pp_images[i]->p_type,
                p_h->pp_images[i]->pp_files[0],
                input_ret);
      return input_ret;
    }

    // Init input image handle
    ret=p_h->pp_images[i]->p_functions->
          CreateHandle(&(p_h->pp_images[i]->p_handle),
                       p_h->pp_images[i]->p_type,
                       p_h->debug);
    if(ret!=0) {
      LOG_ERROR("Unable to init input handle for input image '%s': %s!\n",
                p_h->pp_images[i]->pp_files[0],
                p_h->pp_images[i]->p_functions->
                  GetErrorMessage(ret));
      return e_XmountInput_Error_FailedCreatingImageHandle;
    }

    // Parse input lib specific options
    if(p_h->pp_lib_params!=NULL) {
      ret=p_h->pp_images[i]->p_functions->
            OptionsParse(p_h->pp_images[i]->p_handle,
                         p_h->lib_params_count,
                         p_h->pp_lib_params,
                         &p_err_msg);
      if(ret!=0) {
        if(p_err_msg!=NULL) {
          LOG_ERROR("Unable to parse input library specific options for image "
                      "'%s': %s: %s!\n",
                    p_h->pp_images[i]->pp_files[0],
                    p_h->pp_images[i]->p_functions->
                      GetErrorMessage(ret),
                    p_err_msg);
          p_h->pp_images[i]->p_functions->FreeBuffer(p_err_msg);
        } else {
          LOG_ERROR("Unable to parse input library specific options for image "
                      "'%s': %s!\n",
                    p_h->pp_images[i]->pp_files[0],
                    p_h->pp_images[i]->p_functions->
                      GetErrorMessage(ret));
        }
        return e_XmountInput_Error_FailedParsingLibParams;
      }
    }

    // Open input image
    ret=p_h->pp_images[i]->p_functions->Open(p_h->pp_images[i]->p_handle,
                                             (const char**)(p_h->pp_images[i]->
                                               pp_files),
                                             p_h->pp_images[i]->files_count);
    if(ret!=0) {
      LOG_ERROR("Unable to open input image file '%s': %s!\n",
                p_h->pp_images[i]->pp_files[0],
                p_h->pp_images[i]->p_functions->
                  GetErrorMessage(ret));
      return e_XmountInput_Error_FailedOpeningImage;
    }

    // Determine input image size
    ret=p_h->pp_images[i]->p_functions->Size(p_h->pp_images[i]->p_handle,
                                             &(p_h->pp_images[i]->size));
    if(ret!=0) {
      LOG_ERROR("Unable to determine size of input image '%s': %s!\n",
                p_h->pp_images[i]->pp_files[0],
                p_h->pp_images[i]->
                  p_functions->GetErrorMessage(ret));
      return e_XmountInput_Error_FailedGettingImageSize;
    }

    // If an offset was specified, check it against offset and change size
    if(p_h->image_offset!=0) {
      if(p_h->image_offset>p_h->pp_images[i]->size) {
        LOG_ERROR("The specified offset is larger than the size of the input "
                    "image '%s'! (%" PRIu64 " > %" PRIu64 ")\n",
                  p_h->pp_images[i]->pp_files[0],
                  p_h->image_offset,
                  p_h->pp_images[i]->size);
        return e_XmountInput_Error_OffsetExceedsImageSize;
      }
      p_h->pp_images[i]->size-=p_h->image_offset;
    }

    // If a size limit was specified, check it and change size
    if(p_h->image_size_limit!=0) {
      if(p_h->pp_images[i]->size<p_h->image_size_limit) {
        LOG_ERROR("The specified size limit is larger than the size of the "
                    "input image '%s'! (%" PRIu64 " > %" PRIu64 ")\n",
                  p_h->pp_images[i]->pp_files[0],
                  p_h->image_size_limit,
                  p_h->pp_images[i]->size);
        return e_XmountInput_Error_SizelimitExceedsImageSize;
      }
      p_h->pp_images[i]->size=p_h->image_size_limit;
    }

    LOG_DEBUG("Input image loaded successfully\n")
  }

  return e_XmountInput_Error_None;
}

/*
 * XmountInput_Close
 */
te_XmountInput_Error XmountInput_Close(pts_XmountInputHandle p_h) {
  int ret=0;

  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;

  if(p_h->pp_images!=NULL) {
    // Close all input images
    for(uint64_t i=0;i<p_h->images_count;i++) {
      if(p_h->pp_images[i]==NULL) continue;
      if(p_h->pp_images[i]->p_functions!=NULL) {
        if(p_h->pp_images[i]->p_handle!=NULL) {
          ret=p_h->pp_images[i]->p_functions->
            Close(p_h->pp_images[i]->p_handle);
          if(ret!=0) {
            LOG_ERROR("Unable to close input image '%s': %s\n",
                      p_h->pp_images[i]->pp_files[0]!=NULL ?
                        p_h->pp_images[i]->pp_files[0] : "n/a",
                      p_h->pp_images[i]->p_functions->GetErrorMessage(ret));
          }
          ret=p_h->pp_images[i]->p_functions->
                DestroyHandle(&(p_h->pp_images[i]->p_handle));
          if(ret!=0) {
            LOG_ERROR("Unable to destroy handle of input image '%s': %s\n",
                      p_h->pp_images[i]->pp_files[0]!=NULL ?
                        p_h->pp_images[i]->pp_files[0] : "n/a",
                      p_h->pp_images[i]->p_functions->GetErrorMessage(ret));
          }
          p_h->pp_images[i]->p_handle=NULL;
        }
      }
      if(p_h->pp_images[i]->pp_files!=NULL) {
        for(uint64_t ii=0;ii<p_h->pp_images[i]->files_count;ii++) {
          if(p_h->pp_images[i]->pp_files[ii]!=NULL)
            XMOUNT_FREE(p_h->pp_images[i]->pp_files[ii]);
        }
        XMOUNT_FREE(p_h->pp_images[i]->pp_files);
      }
      if(p_h->pp_images[i]->p_type!=NULL) {
        XMOUNT_FREE(p_h->pp_images[i]->p_type);
      }
      XMOUNT_FREE(p_h->pp_images[i]);
    }
    XMOUNT_FREE(p_h->pp_images);
  }

  return e_XmountInput_Error_None;
}

/*
 * XmountInput_GetSize
 */
te_XmountInput_Error XmountInput_GetSize(pts_XmountInputHandle p_h,
                                         uint64_t image_nr,
                                         uint64_t *p_size)
{
  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;
  if(image_nr>=p_h->images_count) return e_XmountInput_Error_NoSuchImage;
  if(p_size==NULL) return e_XmountInput_Error_InvalidBuffer;

  *p_size=p_h->pp_images[image_nr]->size;
  return e_XmountInput_Error_None;
}

/*
 * XmountInput_ReadData
 */
te_XmountInput_Error XmountInput_ReadData(pts_XmountInputHandle p_h,
                                          uint64_t image_nr,
                                          char *p_buf,
                                          uint64_t offset,
                                          uint64_t count,
                                          uint64_t *p_read)
{
  uint64_t to_read=0;
  int ret=0;
  int read_errno=0;
  pts_XmountInputImage p_image=NULL;

  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;
  if(image_nr>=p_h->images_count) return e_XmountInput_Error_NoSuchImage;
  if(p_buf==NULL) return e_XmountInput_Error_InvalidBuffer;
  p_image=p_h->pp_images[image_nr];

  LOG_DEBUG("Reading %zu bytes at offset %zu from input image '%s'\n",
            count,
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
  if(offset+count>p_image->size) {
    // Attempt to read data past EOF of image file
    to_read=p_image->size-offset;
    LOG_DEBUG("Attempt to read data past EOF of input image '%s'. "
                "Correcting size from %zu to %zu\n",
              p_image->pp_files[0],
              count,
              to_read);
  } else to_read=count;

  // Read data from image file (adding input image offset if one was specified)
  ret=p_image->p_functions->Read(p_image->p_handle,
                                 p_buf,
                                 offset+p_h->image_offset,
                                 to_read,
                                 p_read,
                                 &read_errno);
  if(ret!=0) {
    LOG_ERROR("Couldn't read %zu bytes at offset %zu from input image "
                "'%s': %s: Error code %u!\n",
              to_read,
              offset,
              p_image->pp_files[0],
              p_image->p_functions->GetErrorMessage(ret),
              read_errno);
    return e_XmountInput_Error_FailedReadingData;
  }

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

/*
 * XmountInput_GetInfoFileContent
 */
te_XmountInput_Error XmountInput_GetInfoFileContent(pts_XmountInputHandle p_h,
                                                    char **pp_content)
{
  int ret=0;
  char *p_buf=NULL;
  char *p_content=NULL;

  // Params check
  if(p_h==NULL) return e_XmountInput_Error_InvalidHandle;
  if(pp_content==NULL) return e_XmountInput_Error_InvalidBuffer;

  for(uint64_t i=0;i<p_h->images_count;i++) {
    ret=p_h->pp_images[i]->p_functions->
      GetInfofileContent(p_h->pp_images[i]->p_handle,(const char**)&p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to get info file content for image '%s': %s!\n",
                p_h->pp_images[i]->pp_files[0],
                p_h->pp_images[i]->p_functions->GetErrorMessage(ret));
      return e_XmountInput_Error_FailedGettingInfoFileContent;
    }
    // Add infos to main buffer and free p_buf
    XMOUNT_STRAPP(p_content,"\n--> ");
    XMOUNT_STRAPP(p_content,p_h->pp_images[i]->pp_files[0]);
    XMOUNT_STRAPP(p_content," <--\n");
    if(p_buf!=NULL) {
      XMOUNT_STRAPP(p_content,p_buf);
      p_h->pp_images[i]->p_functions->FreeBuffer(p_buf);
    } else {
      XMOUNT_STRAPP(p_content,"None\n");
    }
  }

  *pp_content=p_content;
  return e_XmountInput_Error_None;
}

/*******************************************************************************
 * Private functions implementations
 ******************************************************************************/
/*
 * FindInputLib
 */
te_XmountInput_Error XmountInput_FindLib(pts_XmountInputHandle p_h,
                                         pts_XmountInputImage p_input_image)
{
  char *p_buf;

  LOG_DEBUG("Trying to find suitable library for input type '%s'.\n",
            p_input_image->p_type);

  // Loop over all loaded libs
  for(uint32_t i=0;i<p_h->libs_count;i++) {
    LOG_DEBUG("Checking input library %s\n",p_h->pp_libs[i]->p_name);
    p_buf=p_h->pp_libs[i]->p_supported_input_types;
    while(*p_buf!='\0') {
      if(strcmp(p_buf,p_input_image->p_type)==0) {
        // Library supports input type, set lib functions
        LOG_DEBUG("Input library '%s' pretends to handle that input type.\n",
                  p_h->pp_libs[i]->p_name);
        p_input_image->p_functions=&(p_h->pp_libs[i]->lib_functions);
        return e_XmountInput_Error_None;
      }
      p_buf+=(strlen(p_buf)+1);
    }
  }

  LOG_DEBUG("Couldn't find any suitable library.\n");

  // No library supporting input type found
  return e_XmountInput_Error_UnsupportedFormat;
}
