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

#include "xmount_output.h"
#include "../libxmount/libxmount.h"
#include "../libxmount_output/libxmount_output.h"
#include "macros.h"

#include <string.h> // For memcpy
#include <dlfcn.h> // For dlopen, dlclose, dlsym

/*******************************************************************************
 * Private definitions / macros
 ******************************************************************************/

#define LOG_WARNING(...) do {         \
  LIBXMOUNT_LOG_WARNING(__VA_ARGS__); \
} while(0)

#define LOG_ERROR(...) do {         \
  LIBXMOUNT_LOG_ERROR(__VA_ARGS__); \
} while(0)

#define LOG_DEBUG(...) do {                    \
  LIBXMOUNT_LOG_DEBUG(p_h->debug,__VA_ARGS__); \
} while(0)

/*******************************************************************************
 * Private types / structures / enums
 ******************************************************************************/

//! Structure containing infos about output libs
typedef struct s_XmountOutputLib {
  //! Filename of lib (without path)
  char *p_name;
  //! Handle to the loaded lib
  void *p_lib;
  //! Array of supported output formats
  char *p_supported_output_formats;
  //! Struct containing lib functions
  ts_LibXmountOutput_Functions lib_functions;
} ts_XmountOutputLib, *pts_XmountOutputLib;

//! Structure containing infos about output image
typedef struct s_XmountOutputHandle {
  //! Loaded output lib count
  uint32_t libs_count;
  //! Array containing infos about loaded output libs
  pts_XmountOutputLib *pp_libs;
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
  //! Output image functions passed to output lib
  ts_LibXmountOutput_Functions output_functions;
  //! Size
  uint64_t image_size;
  //! Path of virtual image file
  char *p_virtual_image_path;
  //! Debug
  uint8_t debug;
} ts_XmountOutputHandle;

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
 * \param p_h Output handle
 * \param p_input_image Output image to search input lib for
 * \return e_XmountOutputError_None on success
 */
te_XmountOutputError XmountOutput_FindLib(pts_XmountOutputHandle p_h);

/*******************************************************************************
 * Public functions implementations
 ******************************************************************************/
/*
 * XmountOutput_CreateHandle
 */
te_XmountOutputError
  XmountOutput_CreateHandle(pts_XmountOutputHandle *pp_h,
                            tfun_XmountOutput_InputImageSize p_img_size,
                            tfun_XmountOutput_InputImageRead p_img_read,
                            tfun_XmountOutput_InputImageWrite p_img_write)
{
  pts_XmountOutputHandle p_h=NULL;

  // Params check
  if(pp_h==NULL) return e_XmountOutputError_InvalidHandlePointer;

  // Alloc new handle
  p_h=(pts_XmountOutputHandle)calloc(1,sizeof(ts_XmountOutputHandle));
  if(p_h==NULL) {
    return e_XmountOutputError_Alloc;
  }

  // Init values
  p_h->pp_libs=NULL;
  p_h->p_output_format=NULL;
  p_h->pp_lib_params=NULL;
  p_h->p_handle=NULL;
  p_h->p_functions=NULL;
  p_h->output_functions.Size=p_img_size;
  p_h->output_functions.Read=p_img_read;
  p_h->output_functions.Write=p_img_write;
  p_h->p_virtual_image_path=NULL;

  *pp_h=p_h;
  return e_XmountOutputError_None;
}

/*
 * XmountOutput_DestroyHandle
 */
te_XmountOutputError XmountOutput_DestroyHandle(pts_XmountOutputHandle *pp_h) {
  int ret=0;
  pts_XmountOutputHandle p_h=NULL;

  // Params check
  if(pp_h==NULL) return e_XmountOutputError_InvalidHandlePointer;
  if(*pp_h==NULL) return e_XmountOutputError_InvalidHandle;
  p_h=*pp_h;

  // Free resources
  if(p_h->p_functions!=NULL) {
    if(p_h->p_handle!=NULL) {
      // Destroy output handle
      ret=p_h->p_functions->DestroyHandle(&(p_h->p_handle));
      if(ret!=0) {
        LOG_ERROR("Unable to destroy output handle: %s!\n",
                  p_h->p_functions->GetErrorMessage(ret));
      }
    }
  }
  if(p_h->pp_lib_params!=NULL) {
    for(uint32_t i=0;i<p_h->lib_params_count;i++) {
      XMOUNT_FREE(p_h->pp_lib_params[i]);
    }
    XMOUNT_FREE(p_h->pp_lib_params);
  }
  if(p_h->p_output_format!=NULL) XMOUNT_FREE(p_h->p_output_format);
  if(p_h->pp_libs!=NULL) {
    // Unload output libs
    for(uint32_t i=0;i<p_h->libs_count;i++) {
      if(p_h->pp_libs[i]==NULL) continue;
      if(p_h->pp_libs[i]->p_supported_output_formats!=NULL) {
        XMOUNT_FREE(p_h->pp_libs[i]->p_supported_output_formats);
      }
      if(p_h->pp_libs[i]->p_lib!=NULL) dlclose(p_h->pp_libs[i]->p_lib);
      if(p_h->pp_libs[i]->p_name!=NULL) XMOUNT_FREE(p_h->pp_libs[i]->p_name);
      XMOUNT_FREE(p_h->pp_libs[i]);
    }
    XMOUNT_FREE(p_h->pp_libs);
  }
  if(p_h->p_virtual_image_path!=NULL) XMOUNT_FREE(p_h->p_virtual_image_path);

  *pp_h=NULL;
  return e_XmountOutputError_None;
}

/*
 * XmountOutput_EnableDebugging
 */
te_XmountOutputError XmountOutput_EnableDebugging(pts_XmountOutputHandle p_h) {
  // Params check
  if(p_h==NULL) return e_XmountOutputError_InvalidHandle;

  // Enable debugging
  p_h->debug=1;
  return e_XmountOutputError_None;
}

/*
 * XmountOutput_AddLibrary
 */
te_XmountOutputError XmountOutput_AddLibrary(pts_XmountOutputHandle p_h,
                                             const char *p_lib_name)
{
  uint32_t supported_formats_len=0;
  t_LibXmount_Output_GetApiVersion pfun_output_GetApiVersion;
  t_LibXmount_Output_GetSupportedFormats pfun_output_GetSupportedFormats;
  t_LibXmount_Output_GetFunctions pfun_output_GetFunctions;
  void *p_libxmount=NULL;
  pts_XmountOutputLib p_output_lib=NULL;
  char *p_buf=NULL;
  char *p_library_path=NULL;
  char *p_supported_formats=NULL;

  // Params check
  if(p_h==NULL) return e_XmountOutputError_InvalidHandle;
  if(p_lib_name==NULL) return e_XmountOutputError_InvalidString;

  // Construct full library path
  XMOUNT_STRSET(p_library_path,XMOUNT_LIBRARY_PATH);
  if(p_library_path[strlen(p_library_path)]!='/') {
    XMOUNT_STRAPP(p_library_path,"/");
  }
  XMOUNT_STRAPP(p_library_path,p_lib_name);

#define XMOUNTOUTPUT_LOADLIBS__LOAD_SYMBOL(name,pfun) {          \
  if((pfun=dlsym(p_libxmount,name))==NULL) {                     \
    LOG_ERROR("Unable to load symbol '%s' from library '%s'!\n", \
              name,                                              \
              p_library_path);                                   \
    dlclose(p_libxmount);                                        \
    return e_XmountOutputError_FailedLoadingSymbol;              \
  }                                                              \
}

  // Try to load given library
  p_libxmount=dlopen(p_library_path,RTLD_NOW);
  if(p_libxmount==NULL) {
    LOG_ERROR("Unable to load output library '%s': %s!\n",
              p_library_path,
              dlerror());
    return e_XmountOutputError_FailedLoadingLibrary;
  }

  // Load library symbols
  XMOUNTOUTPUT_LOADLIBS__LOAD_SYMBOL("LibXmount_Output_GetApiVersion",
                                     pfun_output_GetApiVersion);

  // Check library's API version
  if(pfun_output_GetApiVersion()!=LIBXMOUNT_OUTPUT_API_VERSION) {
    LOG_DEBUG("Failed! Wrong API version.\n");
    LOG_ERROR("Unable to load output library '%s'. Wrong API version\n",
              p_library_path);
    dlclose(p_libxmount);
    return e_XmountOutputError_WrongLibraryApiVersion;
  }

  XMOUNTOUTPUT_LOADLIBS__LOAD_SYMBOL("LibXmount_Output_GetSupportedFormats",
                                     pfun_output_GetSupportedFormats);
  XMOUNTOUTPUT_LOADLIBS__LOAD_SYMBOL("LibXmount_Output_GetFunctions",
                                     pfun_output_GetFunctions);

  // Construct new entry for our library list
  XMOUNT_MALLOC(p_output_lib,pts_XmountOutputLib,sizeof(ts_XmountOutputLib));
  // Initialize lib_functions structure to NULL
  memset(&(p_output_lib->lib_functions),
         0,
         sizeof(ts_LibXmountOutput_Functions));

  // Set name and handle
  XMOUNT_STRSET(p_output_lib->p_name,p_lib_name);
  p_output_lib->p_lib=p_libxmount;

  // Get and set supported formats
  p_supported_formats=pfun_output_GetSupportedFormats();
  p_buf=p_supported_formats;
  while(*p_buf!='\0') {
    supported_formats_len+=(strlen(p_buf)+1);
    p_buf+=(strlen(p_buf)+1);
  }
  supported_formats_len++;
  XMOUNT_MALLOC(p_output_lib->p_supported_output_formats,
                char*,
                supported_formats_len);
  memcpy(p_output_lib->p_supported_output_formats,
         p_supported_formats,
         supported_formats_len);

  // Get, set and check lib_functions
  pfun_output_GetFunctions(&(p_output_lib->lib_functions));
  if(p_output_lib->lib_functions.CreateHandle==NULL ||
     p_output_lib->lib_functions.DestroyHandle==NULL ||
     p_output_lib->lib_functions.Transform==NULL ||
     p_output_lib->lib_functions.Size==NULL ||
     p_output_lib->lib_functions.Read==NULL ||
     p_output_lib->lib_functions.OptionsHelp==NULL ||
     p_output_lib->lib_functions.OptionsParse==NULL ||
     p_output_lib->lib_functions.GetInfofileContent==NULL ||
     p_output_lib->lib_functions.GetErrorMessage==NULL ||
     p_output_lib->lib_functions.FreeBuffer==NULL)
  {
    LOG_DEBUG("Missing implemention of one or more functions in lib %s!\n",
              p_lib_name);
    XMOUNT_FREE(p_output_lib->p_supported_output_formats);
    XMOUNT_FREE(p_output_lib->p_name);
    XMOUNT_FREE(p_output_lib);
    dlclose(p_libxmount);
    return e_XmountOutputError_MissingLibraryFunction;
  }

  // Add entry to the input library list
  XMOUNT_REALLOC(p_h->pp_libs,
                 pts_XmountOutputLib*,
                 sizeof(pts_XmountOutputLib)*(p_h->libs_count+1));
  p_h->pp_libs[p_h->libs_count++]=p_output_lib;

  LOG_DEBUG("Output library '%s' loaded successfully\n",p_lib_name);

#undef XMOUNTOUTPUT_LOADLIBS__LOAD_SYMBOL

  return e_XmountOutputError_None;
}

te_XmountOutputError XmountOutput_GetLibraryCount(pts_XmountOutputHandle p_h,
                                                  uint32_t *p_count)
{
  // Params check
  if(p_h==NULL) return e_XmountOutputError_InvalidHandle;
  if(p_count==NULL) return e_XmountOutputError_InvalidBuffer;

  // Return library count
  *p_count=p_h->libs_count;
  return e_XmountOutputError_None;
}

/*
 * XmountOutput_GetSupportedFormats
 */
te_XmountOutputError
  XmountOutput_GetSupportedFormats(pts_XmountOutputHandle p_h,
                                   char **pp_formats)
{
  char *p_buf=NULL;
  char *p_formats=NULL;
  uint32_t cur_len=0;
  uint32_t vector_len=0;

  // Params check
  if(p_h==NULL) return e_XmountOutputError_InvalidHandle;
  if(pp_formats==NULL) return e_XmountOutputError_InvalidBuffer;

  // Loop over all loaded libs, extract supported formats and add to our vector
  // TODO: IMPROVEMENT: Final vector could be sorted
  for(uint32_t i=0;i<p_h->libs_count;i++) {
    p_buf=p_h->pp_libs[i]->p_supported_output_formats;
    while(p_buf!=NULL && *p_buf!='\0') p_buf+=(strlen(p_buf)+1);
    cur_len=(uint32_t)(p_buf-p_h->pp_libs[i]->p_supported_output_formats);
    if(cur_len==0) continue;
    p_formats=(char*)realloc(p_formats,vector_len+cur_len);
    if(p_formats==NULL) return e_XmountOutputError_Alloc;
    memcpy(p_formats+vector_len,
           p_h->pp_libs[i]->p_supported_output_formats,
           cur_len);
    vector_len+=cur_len;
  }

  // Null-terminate vector
  p_formats=(char*)realloc(p_formats,vector_len+1);
  if(p_formats==NULL) return e_XmountOutputError_Alloc;
  p_formats[vector_len]='\0';

  *pp_formats=p_formats;
  return e_XmountOutputError_None;
}

/*
 * XmountOutput_SetOptions
 */
te_XmountOutputError XmountOutput_SetOptions(pts_XmountOutputHandle p_h,
                                             char *p_options)
{
  // Params check
  if(p_h==NULL) return e_XmountOutputError_InvalidHandle;
  if(p_options==NULL) return e_XmountOutputError_InvalidString;

  // Make sure library parameters haven't been set previously
  if(p_h->pp_lib_params!=NULL) {
    LOG_ERROR("Output library options already set!\n");
    return e_XmountOutputError_LibOptionsAlreadySet;
  }

  // Save options
  if(XmountLib_SplitLibParams(p_options,
                              &(p_h->lib_params_count),
                              &(p_h->pp_lib_params))!=0)
  {
    LOG_ERROR("Unable to parse input library options '%s'!\n",p_options);
    return e_XmountOutputError_FailedParsingOptions;
  }

  return e_XmountOutputError_None;
}

/*
 * XmountOutput_GetOptionsHelpText
 */
te_XmountOutputError XmountOutput_GetOptionsHelpText(pts_XmountOutputHandle p_h,
                                                     char **pp_help_text)
{
  const char *p_buf=NULL;
  char *p_help=NULL;
  int ret=0;

  // Params check
  if(p_h==NULL) return e_XmountOutputError_InvalidHandle;
  if(pp_help_text==NULL) return e_XmountOutputError_InvalidBuffer;

  // Loop over all loaded libs, extract help and add to our text buffer
  // TODO: IMPROVEMENT: Final text should be sorted by lib's name
  for(uint32_t i=0;i<p_h->libs_count;i++) {
    ret=p_h->pp_libs[i]->lib_functions.OptionsHelp((const char**)&p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to get options help for library '%s': %s!\n",
                p_h->pp_libs[i]->p_name,
                p_h->pp_libs[i]->lib_functions.GetErrorMessage(ret));
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
  return e_XmountOutputError_None;
}

/*
 * XmountOutput_GetLibsInfoText
 */
te_XmountOutputError XmountOutput_GetLibsInfoText(pts_XmountOutputHandle p_h,
                                                  char **pp_info_text)
{
  char *p_buf=NULL;
  char *p_info_text=NULL;
  uint8_t first=0;

  // Params check
  if(p_h==NULL) return e_XmountOutputError_InvalidHandle;
  if(pp_info_text==NULL) return e_XmountOutputError_InvalidBuffer;

  // Loop over all loaded libs, extract name and supported formats and add to
  // our text buffer
  // TODO: IMPROVEMENT: Final text should be sorted by lib's name
  for(uint32_t i=0;i<p_h->libs_count;i++) {
    XMOUNT_STRAPP(p_info_text,"    - ");
    XMOUNT_STRAPP(p_info_text,p_h->pp_libs[i]->p_name);
    XMOUNT_STRAPP(p_info_text," supporting ");
    XMOUNT_STRAPP(p_info_text,"    - ");
    XMOUNT_STRAPP(p_info_text,"    - ");
    p_buf=p_h->pp_libs[i]->p_supported_output_formats;
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
  return e_XmountOutputError_None;
}

/*
 * XmountOutput_SetFormat
 */
te_XmountOutputError XmountOutput_SetFormat(pts_XmountOutputHandle p_h,
                                            char *p_format)
{
  // Params check
  if(p_h==NULL) return e_XmountOutputError_InvalidHandle;
  if(p_format==NULL) return e_XmountOutputError_InvalidString;

  // Set output format
  XMOUNT_STRSET(p_h->p_output_format,p_format);

  return e_XmountOutputError_None;
}

/*
 * XmountOutput_Transform
 */
te_XmountOutputError XmountOutput_Transform(pts_XmountOutputHandle p_h) {
  const char *p_err_msg=NULL;
  int ret=0;
  te_XmountOutputError output_ret=e_XmountOutputError_None;

  // Params check
  if(p_h==NULL) return e_XmountOutputError_InvalidHandle;

  // Set default output format if none was set previously
  if(p_h->p_output_format==NULL) {
    XMOUNT_STRSET(p_h->p_output_format,XMOUNT_OUTPUT_DEFAULT_OUTPUT_FORMAT);
  }

  // Find output lib
  output_ret=XmountOutput_FindLib(p_h);
  if(output_ret!=e_XmountOutputError_None) {
    LOG_ERROR("Unable to find a library supporting the output format '%s'!\n",
              p_h->p_output_format);
    return output_ret;
  }

  // Init output
  ret=p_h->p_functions->CreateHandle(&p_h->p_handle,
                                     p_h->p_output_format,
                                     p_h->debug);
  if(ret!=0) {
    LOG_ERROR("Unable to create output handle: %s!\n",
              p_h->p_functions->GetErrorMessage(ret));
    return e_XmountOutputError_FailedCreatingOutputHandle;
  }

  // Parse output lib specific options
  if(p_h->pp_lib_params!=NULL) {
    p_err_msg=NULL;
    ret=p_h->p_functions->OptionsParse(p_h->p_handle,
                                       p_h->lib_params_count,
                                       p_h->pp_lib_params,
                                       &p_err_msg);
    if(ret!=0) {
      if(p_err_msg!=NULL) {
        LOG_ERROR("Unable to parse output library specific options: %s: %s!\n",
                  p_h->p_functions->GetErrorMessage(ret),
                  p_err_msg);
        p_h->p_functions->FreeBuffer(p_err_msg);
      } else {
        LOG_ERROR("Unable to parse output library specific options: %s!\n",
                  p_h->p_functions->GetErrorMessage(ret));
      }
      return e_XmountOutputError_FailedParsingLibParams;
    }
  }

  // TODO: This has to be done somewhere!
/*
  if(!ExtractOutputFileNames(glob_xmount.p_first_input_image_name)) {
    LOG_ERROR("Couldn't extract virtual file names!\n");
    FreeResources();
    return 1;
  }
  LOG_DEBUG("Virtual file names extracted successfully\n")
*/

  return e_XmountOutputError_None;
}

/*
 * XmountOutput_GetOutputFilenames
 */
te_XmountOutputError
  XmountOutput_GetOutputFilenames(pts_XmountOutputHandle p_h,
                                  char ***ppp_output_files)
{
  // TODO: Implement

  return e_XmountOutputError_None;
}

/*
 * XmountOutput_GetSize
 */
te_XmountOutputError XmountOutput_GetSize(pts_XmountOutputHandle p_h,
                                          const char *p_output_filename,
                                          uint64_t *p_size)
{
/*
  // Params check
  if(p_h==NULL) return e_XmountOutputError_InvalidHandle;
  if(image_nr>=p_h->images_count) return e_XmountOutputError_NoSuchImage;
  if(p_size==NULL) return e_XmountOutputError_InvalidBuffer;

  *p_size=p_h->pp_images[image_nr]->size;
*/
  // TODO: Implement
  return e_XmountOutputError_None;
}

/*
 * XmountOutput_ReadData
 */
te_XmountOutputError XmountOutput_ReadData(pts_XmountOutputHandle p_h,
                                           const char *p_output_filename,
                                           char *p_buf,
                                           uint64_t offset,
                                           uint64_t count,
                                           uint64_t *p_read)
{
  // TODO: Implement

/*
  uint64_t to_read=0;
  int ret=0;
  int read_errno=0;
  pts_XmountOutputImage p_image=NULL;

  // Params check
  if(p_h==NULL) return e_XmountOutputError_InvalidHandle;
  if(image_nr>=p_h->images_count) return e_XmountOutputError_NoSuchImage;
  if(p_buf==NULL) return e_XmountOutputError_InvalidBuffer;
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
    return e_XmountOutputError_FailedReadingData;
  }
*/

  return e_XmountOutputError_None;
}

/*
 * XmountOutput_WriteData
 */
te_XmountOutputError XmountOutput_WriteData(pts_XmountOutputHandle p_h,
                                            const char *p_output_filename,
                                            const char *p_buf,
                                            uint64_t offset,
                                            uint64_t count,
                                            uint64_t *p_written)
{
  // TODO: Implement
  return e_XmountOutputError_None;
}

/*
 * XmountOutput_GetInfoFileContent
 */
te_XmountOutputError XmountOutput_GetInfoFileContent(pts_XmountOutputHandle p_h,
                                                    char **pp_content)
{
  char *p_content=NULL;

  // TODO: Implement

  *pp_content=p_content;
  return e_XmountOutputError_None;
}

/*******************************************************************************
 * Private functions implementations
 ******************************************************************************/
/*
 * FindOutputLib
 */
te_XmountOutputError XmountOutput_FindLib(pts_XmountOutputHandle p_h) {
  char *p_buf;

  LOG_DEBUG("Trying to find suitable library for output format '%s'.\n",
            p_h->p_output_format);

  // Loop over all loaded output libs
  for(uint32_t i=0;i<p_h->libs_count;i++) {
    LOG_DEBUG("Checking output library %s\n",p_h->pp_libs[i]->p_name);
    p_buf=p_h->pp_libs[i]->p_supported_output_formats;
    while(*p_buf!='\0') {
      if(strcmp(p_buf,p_h->p_output_format)==0) {
        // Library supports output type, set lib functions
        LOG_DEBUG("Output library '%s' pretends to handle that output format.\n",
                  p_h->pp_libs[i]->p_name);
        p_h->p_functions=&(p_h->pp_libs[i]->lib_functions);
        return e_XmountOutputError_None;
      }
      p_buf+=(strlen(p_buf)+1);
    }
  }

  LOG_DEBUG("Couldn't find any suitable library.\n");

  // No library supporting input type found
  return e_XmountOutputError_UnsupportedFormat;
}






//! Get size of output image
/*!
 * \param p_size Pointer to an uint64_t to which the size will be written to
 * \return TRUE on success, FALSE on error
 */
/*
int GetOutputImageSize(uint64_t *p_size) {
  int ret;
  uint64_t output_image_size=0;

  if(glob_xmount.output.image_size!=0) {
    *p_size=glob_xmount.output.image_size;
    return TRUE;
  }

  ret=glob_xmount.output.p_functions->Size(glob_xmount.output.p_handle,
                                           &output_image_size);
  if(ret!=0) {
    LOG_ERROR("Couldn't get output image size!\n")
    return FALSE;
  }

  glob_xmount.output.image_size=output_image_size;
  *p_size=output_image_size;
  return TRUE;
}
*/

//! Read data from output image
/*!
 * \param p_buf Pointer to buffer to write read data to
 * \param offset Offset at which data should be read
 * \param size Size of data which should be read
 * \return Number of read bytes on success or negated error code on error
 */
/*
int ReadOutputImageData(char *p_buf, off_t offset, size_t size) {
  uint64_t output_image_size;
  size_t read=0;
  int ret;

  // Get output image size
  if(GetOutputImageSize(&output_image_size)!=TRUE) {
    LOG_ERROR("Couldn't get size of output image!\n")
    return -EIO;
  }

  // Make sure request is within output image
  if(offset>=output_image_size) {
    LOG_DEBUG("Offset %zu is at / beyond size of output image.\n",offset);
    return 0;
  }
  if(offset+size>output_image_size) {
    LOG_DEBUG("Attempt to read data past EOF of output image. Correcting size "
                "from %zu to %zu.\n",
              size,
              output_image_size-offset);
    size=output_image_size-offset;
  }

  // Read data
  ret=glob_xmount.output.p_functions->Read(glob_xmount.output.p_handle,
                                           p_buf,
                                           offset,
                                           size,
                                           &read);
  if(ret!=0) {
    LOG_ERROR("Unable to read %zu bytes at offset %zu from output image!\n",
              size,
              offset)
    return ret;
  } else if(read!=size) {
    LOG_WARNING("Unable to read all requested data from output image!\n")
    return read;
  }

  return size;
}
*/

//! Write data to output image
/*!
 * \param p_buf Buffer with data to write
 * \param offset Offset to write to
 * \param size Amount of bytes to write
 * \return Number of written bytes on success or "-1" on error
 */
/*
int WriteOutputImageData(const char *p_buf, off_t offset, size_t size) {
  uint64_t output_image_size;
  int ret;
  size_t written;

  // Get output image size
  if(!GetOutputImageSize(&output_image_size)) {
    LOG_ERROR("Couldn't get output image size!\n")
    return -1;
  }

  // Make sure write is within output image
  if(offset>=output_image_size) {
    LOG_ERROR("Attempt to write beyond EOF of output image file!\n")
    return -1;
  }
  if(offset+size>output_image_size) {
    LOG_DEBUG("Attempt to write past EOF of output image file. Correcting size "
                "from %zu to %zu.\n",
              size,
              output_image_size-offset);
    size=output_image_size-offset;
  }

  ret=glob_xmount.output.p_functions->Write(glob_xmount.output.p_handle,
                                            p_buf,
                                            offset,
                                            size,
                                            &written);
  if(ret!=0) {
    LOG_ERROR("Unable to write %zu bytes at offset %zu to output image!\n",
              offset,
              size)
    return ret;
  } else if(written!=size) {
    LOG_WARNING("Unable to write all requested data to output image!\n")
  }

  return size;
}
*/
