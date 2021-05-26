/*******************************************************************************
* xmount Copyright (c) 2024 by SITS Sarl                                       *
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
#include "libxmount_morphing_swab.h"

#define LOG_DEBUG(...) {                                    \
  LIBXMOUNT_LOG_DEBUG(p_swab_handle->debug,__VA_ARGS__); \
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
  return "swab\0\0";
}

/*
 * LibXmount_Morphing_GetFunctions
 */
void LibXmount_Morphing_GetFunctions(ts_LibXmountMorphingFunctions *p_functions)
{
  p_functions->CreateHandle=&SwabCreateHandle;
  p_functions->DestroyHandle=&SwabDestroyHandle;
  p_functions->Morph=&SwabMorph;
  p_functions->Size=&SwabSize;
  p_functions->Read=&SwabRead;
  p_functions->OptionsHelp=&SwabOptionsHelp;
  p_functions->OptionsParse=&SwabOptionsParse;
  p_functions->GetInfofileContent=&SwabGetInfofileContent;
  p_functions->GetErrorMessage=&SwabGetErrorMessage;
  p_functions->FreeBuffer=&SwabFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
/*
 * SwabCreateHandle
 */
static int SwabCreateHandle(void **pp_handle,
                               const char *p_format,
                               uint8_t debug)
{
  pts_SwabHandle p_swab_handle;

  // Alloc new handle
  p_swab_handle=malloc(sizeof(ts_SwabHandle));
  if(p_swab_handle==NULL) return SWAB_MEMALLOC_FAILED;

  // Init handle values
  p_swab_handle->debug=debug;
  p_swab_handle->input_images_count=0;
  p_swab_handle->p_input_functions=NULL;
  p_swab_handle->morphed_image_size=0;

  LOG_DEBUG("Created new LibXmount_Morphing_Swab handle\n");

  // Return new handle
  *pp_handle=p_swab_handle;
  return SWAB_OK;
}

/*
 * SwabDestroyHandle
 */
static int SwabDestroyHandle(void **pp_handle) {
  pts_SwabHandle p_swab_handle=(pts_SwabHandle)*pp_handle;

  LOG_DEBUG("Destroying LibXmount_Morphing_Swab handle\n");

  // Free handle
  free(p_swab_handle);

  *pp_handle=NULL;
  return SWAB_OK;
}

/*
 * SwabMorph
 */
static int SwabMorph(void *p_handle,
                        pts_LibXmountMorphingInputFunctions p_input_functions)
{
  pts_SwabHandle p_swab_handle=(pts_SwabHandle)p_handle;
  int ret;
  uint64_t input_image_size;

  LOG_DEBUG("Initializing LibXmount_Morphing_Swab\n");

  // Set input functions and get image count
  p_swab_handle->p_input_functions=p_input_functions;
  if(p_swab_handle->
       p_input_functions->
         ImageCount(&p_swab_handle->input_images_count)!=0)
  {
    return SWAB_CANNOT_GET_IMAGECOUNT;
  }

  // Calculate morphed image size
  for(uint64_t i=0;i<p_swab_handle->input_images_count;i++) {
    ret=p_swab_handle->
          p_input_functions->
            Size(i,&input_image_size);
    if(ret!=0) return SWAB_CANNOT_GET_IMAGESIZE;

    LOG_DEBUG("Adding %" PRIu64 " bytes from image %" PRIu64 "\n",
              input_image_size,
              i);

    p_swab_handle->morphed_image_size+=input_image_size;
  }

  LOG_DEBUG("Total morphed image size is %" PRIu64 " bytes\n",
            p_swab_handle->morphed_image_size);

  return SWAB_OK;
}

/*
 * SwabSize
 */
static int SwabSize(void *p_handle, uint64_t *p_size) {
  *p_size=((pts_SwabHandle)(p_handle))->morphed_image_size;
  return SWAB_OK;
}

/*
 * SwabRead
 */
static int SwabRead(void *p_handle,
                       char *p_buf,
                       off_t offset,
                       size_t count,
                       size_t *p_read)
{
  pts_SwabHandle p_swab_handle=(pts_SwabHandle)p_handle;
  uint64_t cur_input_image=0;
  off_t cur_offset=offset;
  int ret;
  size_t cur_count=count;
  size_t read;

  LOG_DEBUG("Reading %zu bytes at offset %zu from morphed image\n",
            count,
            offset);

  // Make sure read parameters are within morphed image bounds
  if(offset>=p_swab_handle->morphed_image_size ||
     offset+count>p_swab_handle->morphed_image_size)
  {
    return SWAB_READ_BEYOND_END_OF_IMAGE;
  }

	// If offset is uneven, the previous byte must be read, too
	if (offset & 1) {
		cur_offset-=1;
		cur_count+=1;
	}

  // Alloc read buffer
  char *data=malloc(cur_count);
  if(data==NULL) return SWAB_MEMALLOC_FAILED;

	// Read bytes
	ret=p_swab_handle->p_input_functions->
				Read(cur_input_image,
						 data,
						 cur_offset,
						 cur_count,
						 &read);
	if(ret!=0 || read!=cur_count) return SWAB_CANNOT_READ_DATA;
	*p_read = count;

	// do the byte swap
	char *cp = p_buf;
	for (size_t i=0; i<cur_count; i+=2) {
		*(cp+i) = *(data+i+1);
		if (i+1<count) {
			*(cp+i+1) = *(data+i);
		}
	}

	// release read buffer
	free(data);

  return SWAB_OK;
}

/*
 * SwabOptionsHelp
 */
static int SwabOptionsHelp(const char **pp_help) {
  *pp_help=NULL;
  return SWAB_OK;
}

/*
 * SwabOptionsParse
 */
static int SwabOptionsParse(void *p_handle,
                               uint32_t options_count,
                               const pts_LibXmountOptions *pp_options,
                               const char **pp_error)
{
  return SWAB_OK;
}

/*
 * SwabGetInfofileContent
 */
static int SwabGetInfofileContent(void *p_handle,
                                     const char **pp_info_buf)
{
  *pp_info_buf=NULL;
  return SWAB_OK;
}

/*
 * SwabGetErrorMessage
 */
static const char* SwabGetErrorMessage(int err_num) {
  switch(err_num) {
    case SWAB_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case SWAB_CANNOT_GET_IMAGECOUNT:
      return "Unable to get input image count";
      break;
    case SWAB_CANNOT_GET_IMAGESIZE:
      return "Unable to get input image size";
      break;
    case SWAB_READ_BEYOND_END_OF_IMAGE:
      return "Unable to read data: Attempt to read past EOF";
      break;
    case SWAB_CANNOT_READ_DATA:
      return "Unable to read data";
      break;
    default:
      return "Unknown error";
  }
}

/*
 * SwabFreeBuffer
 */
static void SwabFreeBuffer(void *p_buf) {
  free(p_buf);
}

