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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../libxmount_output.h"
#include "libxmount_output_raw.h"

#define LOG_DEBUG(...) {                                \
  LIBXMOUNT_LOG_DEBUG(p_raw_handle->debug,__VA_ARGS__); \
}

/*******************************************************************************
 * LibXmount_Output API implementation
 ******************************************************************************/
/*
 * LibXmount_Output_GetApiVersion
 */
uint8_t LibXmount_Output_GetApiVersion() {
  return LIBXMOUNT_OUTPUT_API_VERSION;
}

/*
 * LibXmount_Output_GetSupportedFormats
 */
const char* LibXmount_Output_GetSupportedFormats() {
  return "raw\0dmg\0\0";
}

/*
 * LibXmount_Output_GetFunctions
 */
void LibXmount_Output_GetFunctions(ts_LibXmountOutput_Functions *p_functions) {
  p_functions->CreateHandle=&RawCreateHandle;
  p_functions->DestroyHandle=&RawDestroyHandle;
  p_functions->Transform=&RawTransform;
  p_functions->Size=&RawSize;
  p_functions->Read=&RawRead;
  p_functions->Write=&RawWrite;
  p_functions->OptionsHelp=&RawOptionsHelp;
  p_functions->OptionsParse=&RawOptionsParse;
  p_functions->GetInfofileContent=&RawGetInfofileContent;
  p_functions->GetErrorMessage=&RawGetErrorMessage;
  p_functions->FreeBuffer=&RawFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
/*
 * RawCreateHandle
 */
static int RawCreateHandle(void **pp_handle,
                           const char *p_format,
                           uint8_t debug)
{
  pts_RawHandle p_raw_handle;

  // Alloc new handle
  p_raw_handle=malloc(sizeof(ts_RawHandle));
  if(p_raw_handle==NULL) return RAW_MEMALLOC_FAILED;

  // Init handle values
  p_raw_handle->debug=debug;
  p_raw_handle->p_input_functions=NULL;
  p_raw_handle->output_image_size=0;

  LOG_DEBUG("Created new LibXmount_Output_Raw handle\n");

  // Return new handle
  *pp_handle=p_raw_handle;
  return RAW_OK;
}

/*
 * RawDestroyHandle
 */
static int RawDestroyHandle(void **pp_handle) {
  pts_RawHandle p_raw_handle=(pts_RawHandle)*pp_handle;

  LOG_DEBUG("Destroying LibXmount_Output_Raw handle\n");

  // Free handle
  free(p_raw_handle);

  *pp_handle=NULL;
  return RAW_OK;
}

/*
 * RawTransform
 */
static int RawTransform(void *p_handle,
                        pts_LibXmountOutput_InputFunctions p_input_functions)
{
  pts_RawHandle p_raw_handle=(pts_RawHandle)p_handle;
  int ret;

  LOG_DEBUG("Initializing LibXmount_Ouptut_Raw\n");

  // Set input functions and get image count
  p_raw_handle->p_input_functions=p_input_functions;

  // Output image size == morphed image size
  ret=p_raw_handle->
        p_input_functions->
          Size(&p_raw_handle->output_image_size);
  if(ret!=0) return RAW_CANNOT_GET_IMAGESIZE;

  LOG_DEBUG("Total output image size is %" PRIu64 " bytes\n",
            p_raw_handle->output_image_size);

  return RAW_OK;
}

/*
 * RawSize
 */
static int RawSize(void *p_handle, uint64_t *p_size) {
  *p_size=((pts_RawHandle)(p_handle))->output_image_size;
  return RAW_OK;
}

/*
 * RawRead
 */
static int RawRead(void *p_handle,
                   char *p_buf,
                   off_t offset,
                   size_t count,
                   size_t *p_read)
{
  pts_RawHandle p_raw_handle=(pts_RawHandle)p_handle;
  int ret;

  LOG_DEBUG("Reading %zu bytes at offset %zu from output image\n",
            count,
            offset);

  // Make sure read parameters are within output image bounds
  if(offset>=p_raw_handle->output_image_size ||
     offset+count>p_raw_handle->output_image_size)
  {
    return RAW_READ_BEYOND_END_OF_IMAGE;
  }

  // Read data
  ret=p_raw_handle->p_input_functions->Read(p_buf,
                                            offset,
                                            count,
                                            p_read);
  if(ret!=0 || *p_read!=count) return RAW_CANNOT_READ_DATA;

  return RAW_OK;
}

/*
 * RawWrite
 */
static int RawWrite(void *p_handle,
                    char *p_buf,
                    off_t offset,
                    size_t count,
                    size_t *p_written)
{
  pts_RawHandle p_raw_handle=(pts_RawHandle)p_handle;
  int ret;

  LOG_DEBUG("Writing %zu bytes at offset %zu to output image\n",
            count,
            offset);

  // Make sure write parameters are within output image bounds
  if(offset>=p_raw_handle->output_image_size ||
     offset+count>p_raw_handle->output_image_size)
  {
    return RAW_WRITE_BEYOND_END_OF_IMAGE;
  }

  // Write data
  ret=p_raw_handle->p_input_functions->Write(p_buf,
                                             offset,
                                             count,
                                             p_written);
  if(ret!=0 || *p_written!=count) return RAW_CANNOT_WRITE_DATA;

  return RAW_OK;
}

/*
 * RawOptionsHelp
 */
static int RawOptionsHelp(const char **pp_help) {
  *pp_help=NULL;
  return RAW_OK;
}

/*
 * RawOptionsParse
 */
static int RawOptionsParse(void *p_handle,
                           uint32_t options_count,
                           const pts_LibXmountOptions *pp_options,
                           const char **pp_error)
{
  return RAW_OK;
}

/*
 * RawGetInfofileContent
 */
static int RawGetInfofileContent(void *p_handle,
                                 const char **pp_info_buf)
{
  *pp_info_buf=NULL;
  return RAW_OK;
}

/*
 * RawGetErrorMessage
 */
static const char* RawGetErrorMessage(int err_num) {
  switch(err_num) {
    case RAW_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case RAW_CANNOT_GET_IMAGESIZE:
      return "Unable to get input image size";
      break;
    case RAW_READ_BEYOND_END_OF_IMAGE:
      return "Unable to read data: Attempt to read past EOF";
      break;
    case RAW_WRITE_BEYOND_END_OF_IMAGE:
      return "Unable to write data: Attempt to write past EOF";
      break;
    case RAW_CANNOT_READ_DATA:
      return "Unable to read data";
      break;
    case RAW_CANNOT_WRITE_DATA:
      return "Unable to write data";
      break;
    default:
      return "Unknown error";
  }
}

/*
 * RawFreeBuffer
 */
static void RawFreeBuffer(void *p_buf) {
  free(p_buf);
}
