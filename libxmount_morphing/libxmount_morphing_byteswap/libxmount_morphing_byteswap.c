/*******************************************************************************
* xmount Copyright (c) 2024 - 2025 by SITS Sarl                                *
*                                                                              *
* Author(s):                                                                   *
*   Gillen Daniel <development@sits.lu>                                        *
*   Ingo Lafrenz <ich+xmount@der-ingo.de>                                      *
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

#include "../libxmount_morphing.h"
#include "libxmount_morphing_byteswap.h"

#define LOG_DEBUG(...) {                                     \
  LIBXMOUNT_LOG_DEBUG(p_byteswap_handle->debug,__VA_ARGS__); \
}

/*******************************************************************************
 * LibXmount_Morphing API implementation
 ******************************************************************************/
/*
 * LibXmount_Morphing_GetApiVersion
 */
uint8_t LibXmount_Morphing_GetApiVersion() {
  return LIBXMOUNT_MORPHING_API_VERSION;
}

/*
 * LibXmount_Morphing_GetSupportedFormats
 */
const char* LibXmount_Morphing_GetSupportedTypes() {
  return "byteswap\0\0";
}

/*
 * LibXmount_Morphing_GetFunctions
 */
void LibXmount_Morphing_GetFunctions(ts_LibXmountMorphingFunctions *p_functions)
{
  p_functions->CreateHandle=&ByteswapCreateHandle;
  p_functions->DestroyHandle=&ByteswapDestroyHandle;
  p_functions->Morph=&ByteswapMorph;
  p_functions->Size=&ByteswapSize;
  p_functions->Read=&ByteswapRead;
  p_functions->OptionsHelp=&ByteswapOptionsHelp;
  p_functions->OptionsParse=&ByteswapOptionsParse;
  p_functions->GetInfofileContent=&ByteswapGetInfofileContent;
  p_functions->GetErrorMessage=&ByteswapGetErrorMessage;
  p_functions->FreeBuffer=&ByteswapFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
/*
 * ByteswapCreateHandle
 */
static int ByteswapCreateHandle(void **pp_handle,
                                const char *p_format,
                                uint8_t debug)
{
  pts_ByteswapHandle p_byteswap_handle;

  // Alloc new handle
  p_byteswap_handle=malloc(sizeof(ts_ByteswapHandle));
  if(p_byteswap_handle==NULL) return BYTESWAP_MEMALLOC_FAILED;

  // Init handle values
  p_byteswap_handle->debug=debug;
  p_byteswap_handle->input_images_count=0;
  p_byteswap_handle->p_input_functions=NULL;
  p_byteswap_handle->morphed_image_size=0;

  LOG_DEBUG("Created new LibXmount_Morphing_Byteswap handle\n");

  // Return new handle
  *pp_handle=p_byteswap_handle;
  return BYTESWAP_OK;
}

/*
 * ByteswapDestroyHandle
 */
static int ByteswapDestroyHandle(void **pp_handle) {
  pts_ByteswapHandle p_byteswap_handle=(pts_ByteswapHandle)*pp_handle;

  LOG_DEBUG("Destroying LibXmount_Morphing_Byteswap handle\n");

  // Free handle
  free(p_byteswap_handle);

  *pp_handle=NULL;
  return BYTESWAP_OK;
}

/*
 * ByteswapMorph
 */
static int ByteswapMorph(void *p_handle,
                         pts_LibXmountMorphingInputFunctions p_input_functions)
{
  pts_ByteswapHandle p_byteswap_handle=(pts_ByteswapHandle)p_handle;
  int ret;
  uint64_t input_image_size;

  LOG_DEBUG("Initializing LibXmount_Morphing_Byteswap\n");

  // Set input functions and get image count
  p_byteswap_handle->p_input_functions=p_input_functions;
  if(p_byteswap_handle->
       p_input_functions->
         ImageCount(&p_byteswap_handle->input_images_count)!=0)
  {
    return BYTESWAP_CANNOT_GET_IMAGECOUNT;
  }

  // Calculate morphed image size
  for(uint64_t i=0;i<p_byteswap_handle->input_images_count;i++) {
    ret=p_byteswap_handle->
          p_input_functions->
            Size(i,&input_image_size);
    if(ret!=0) return BYTESWAP_CANNOT_GET_IMAGESIZE;

    LOG_DEBUG("Adding %" PRIu64 " bytes from image %" PRIu64 "\n",
              input_image_size,
              i);

    p_byteswap_handle->morphed_image_size+=input_image_size;
  }

  if (p_byteswap_handle->morphed_image_size & 0x01) {
    LOG_DEBUG("Total morphed image size (%" PRIu64 " bytes) is odd!\n",
              p_byteswap_handle->morphed_image_size);
    return BYTESWAP_UNSUPPORTED_IMAGE_SIZE;
  }

  LOG_DEBUG("Total morphed image size is %" PRIu64 " bytes\n",
            p_byteswap_handle->morphed_image_size);

  return BYTESWAP_OK;
}

/*
 * ByteswapSize
 */
static int ByteswapSize(void *p_handle, uint64_t *p_size) {
  *p_size=((pts_ByteswapHandle)(p_handle))->morphed_image_size;
  return BYTESWAP_OK;
}

/*
 * ByteswapRead
 */
static int ByteswapRead(void *p_handle,
                        char *p_buf,
                        off_t offset,
                        size_t count,
                        size_t *p_read)
{
  pts_ByteswapHandle p_byteswap_handle=(pts_ByteswapHandle)p_handle;
  uint64_t cur_input_image=0;
  off_t cur_offset=offset;
  int ret;
  size_t cur_count=count;
  size_t read;

  LOG_DEBUG("Reading %zu bytes at offset %zu from morphed image\n",
            count,
            offset);

  // Make sure read parameters are within morphed image bounds
  if(offset>=p_byteswap_handle->morphed_image_size ||
     offset+count>p_byteswap_handle->morphed_image_size)
  {
    return BYTESWAP_READ_BEYOND_END_OF_IMAGE;
  }

  // To be able to correctly byteswap, we might need to read more bytes
  // than requested. This will never read beyond input image as long as input
  // image size is even (checked in ByteswapMorph()) and we pass the check above
  if ((count & 0x01) && (offset & 0x01)) {
    // Request to read odd amount of bytes and to start @ an odd offset.
    // We need the byte @ offset - 1 too.
    cur_offset--;
    cur_count++;
  } else if ((count & 0x01) && !(offset & 0x01)) {
    // Request to read odd amount of bytes and to start @ an even offset.
    // We need the byte @ offset + count + 1 too.
    cur_count++;
  } else if (offset & 0x01) {
    // Request to read even amount of bytes and to start @ an odd offset.
    // We need the bytes @ offset - 1 and @ offset + count + 1 too.
    cur_offset--;
    cur_count+=2;
  }

  // Alloc read buffer
  uint8_t *p_data = (uint8_t*)malloc(cur_count);
  if (p_data == NULL) return BYTESWAP_MEMALLOC_FAILED;

  // Read bytes
  ret=p_byteswap_handle->p_input_functions->Read(cur_input_image,
                                                 (char *)p_data,
                                                 cur_offset,
                                                 cur_count,
                                                 &read);
  if(ret!=0 || read!=cur_count) return BYTESWAP_CANNOT_READ_DATA;
  *p_read = count;

  // Do the byte swap
  uint8_t *p_cp = (uint8_t*)p_buf;
  for (size_t i = 0; i < cur_count; i += 2) {
    if (i == 0 && (offset & 0x01)) {
      *p_cp++ = p_data[0];
      continue;
    } else *p_cp++ = p_data[i+1];
    if (i+1 < count) *p_cp++ = p_data[i];
  }

  // release read buffer
  free(p_data);

  return BYTESWAP_OK;
}

/*
 * ByteswapOptionsHelp
 */
static int ByteswapOptionsHelp(const char **pp_help) {
  *pp_help=NULL;
  return BYTESWAP_OK;
}

/*
 * ByteswapOptionsParse
 */
static int ByteswapOptionsParse(void *p_handle,
                                uint32_t options_count,
                                const pts_LibXmountOptions *pp_options,
                                const char **pp_error)
{
  return BYTESWAP_OK;
}

/*
 * ByteswapGetInfofileContent
 */
static int ByteswapGetInfofileContent(void *p_handle,
                                      const char **pp_info_buf)
{
  *pp_info_buf=NULL;
  return BYTESWAP_OK;
}

/*
 * ByteswapGetErrorMessage
 */
static const char* ByteswapGetErrorMessage(int err_num) {
  switch(err_num) {
    case BYTESWAP_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case BYTESWAP_CANNOT_GET_IMAGECOUNT:
      return "Unable to get input image count";
      break;
    case BYTESWAP_CANNOT_GET_IMAGESIZE:
      return "Unable to get input image size";
      break;
    case BYTESWAP_READ_BEYOND_END_OF_IMAGE:
      return "Unable to read data: Attempt to read past EOF";
      break;
    case BYTESWAP_CANNOT_READ_DATA:
      return "Unable to read data";
      break;
    case BYTESWAP_UNSUPPORTED_IMAGE_SIZE:
      return "Total input image size must be even to support byte swapping";
      break;
    default:
      return "Unknown error";
  }
}

/*
 * ByteswapFreeBuffer
 */
static void ByteswapFreeBuffer(void *p_buf) {
  free(p_buf);
}

