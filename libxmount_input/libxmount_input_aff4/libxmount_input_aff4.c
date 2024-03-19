/*******************************************************************************
* xmount Copyright (c) 2024 by SITS Sarl                                       *
*                                                                              *
* Author(s):                                                                   *
*   Gillen Daniel <development@sits.lu>                                        *
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
#include <string.h>
#include <fcntl.h> // For O_RDONLY

#include "../libxmount_input.h"

#include <aff4-c.h>

#include "libxmount_input_aff4.h"

/*******************************************************************************
 * LibXmount_Input API implementation
 ******************************************************************************/
/*
 * LibXmount_Input_GetApiVersion
 */
uint8_t LibXmount_Input_GetApiVersion() {
  return LIBXMOUNT_INPUT_API_VERSION;
}

/*
 * LibXmount_Input_GetSupportedFormats
 */
const char* LibXmount_Input_GetSupportedFormats() {
  return "aff4\0\0";
}

/*
 * LibXmount_Input_GetFunctions
 */
void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions *p_functions) {
  p_functions->Init = &Aff4Init;
  p_functions->DeInit = &Aff4DeInit;
  p_functions->CreateHandle = &Aff4CreateHandle;
  p_functions->DestroyHandle = &Aff4DestroyHandle;
  p_functions->Open = &Aff4Open;
  p_functions->Close = &Aff4Close;
  p_functions->Size = &Aff4Size;
  p_functions->Read = &Aff4Read;
  p_functions->OptionsHelp = &Aff4OptionsHelp;
  p_functions->OptionsParse = &Aff4OptionsParse;
  p_functions->GetInfofileContent = &Aff4GetInfofileContent;
  p_functions->GetErrorMessage = &Aff4GetErrorMessage;
  p_functions->FreeBuffer = &Aff4FreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
/*
 * Aff4Init
 */
static int Aff4Init(void **pp_init_handle)
{
  AFF4_init();

  *pp_init_handle = NULL;

  return AFF4_OK;
}

/*
 * Aff4DeInit
 */
static int Aff4DeInit(void **pp_init_handle)
{
  return AFF4_OK;
}

/*
 * Aff4CreateHandle
 */
static int Aff4CreateHandle(void **pp_handle,
                            void *p_init_handle,
                            const char *p_format,
                            uint8_t debug)
{
  (void)p_format;
  pts_Aff4Handle p_aff4_handle;

  // Alloc new lib handle
  p_aff4_handle = (pts_Aff4Handle)malloc(sizeof(ts_Aff4Handle));
  if (p_aff4_handle == NULL) return AFF4_MEMALLOC_FAILED;

  // Init lib handle  
  p_aff4_handle->h_aff4 = -1;

  *pp_handle = p_aff4_handle;
  return AFF4_OK;
}

/*
 * Aff4DestroyHandle
 */
static int Aff4DestroyHandle(void **pp_handle) {
  pts_Aff4Handle p_aff4_handle = (pts_Aff4Handle)*pp_handle;

  // Free lib handle
  if (p_aff4_handle != NULL) free(p_aff4_handle);

  *pp_handle = NULL;
  return AFF4_OK;
}

/*
 * Aff4Open
 */
static int Aff4Open(void *p_handle,
                    const char **pp_filename_arr,
                    uint64_t filename_arr_len)
{
  pts_Aff4Handle p_aff4_handle = (pts_Aff4Handle)p_handle;

  // We need exactly one file
  if (filename_arr_len == 0) return AFF4_NO_INPUT_FILES;
  if (filename_arr_len > 1) return AFF4_TOO_MANY_INPUT_FILES;

  // Open AFF file
  p_aff4_handle->h_aff4 = AFF4_open(pp_filename_arr[0]);
  if (p_aff4_handle->h_aff4 == -1) {
    return AFF4_OPEN_FAILED;
  }

  // TODO: Reject encrypted / logical input images
  /*
  if(af_cannot_decrypt(p_aff_handle->h_aff)) {
    af_close(p_aff_handle->h_aff);
    return AFF4_ENCRYPTION_UNSUPPORTED;
  }
  */

  return AFF4_OK;
}

/*
 * Aff4Close
 */
static int Aff4Close(void *p_handle) {
  pts_Aff4Handle p_aff4_handle = (pts_Aff4Handle)p_handle;

  // Close AFF handle
  if (AFF4_close(p_aff4_handle->h_aff4) != 0)
  {
    return AFF4_CLOSE_FAILED;
  }

  return AFF4_OK;
}

/*
 * Aff4Size
 */
static int Aff4Size(void *p_handle, uint64_t *p_size) {
  pts_Aff4Handle p_aff4_handle = (pts_Aff4Handle)p_handle;
  int64_t object_size;

  if ((object_size = AFF4_object_size(p_aff4_handle->h_aff4)) == -1)
  {
    return AFF4_GETSIZE_FAILED;
  }
  *p_size = (uint64_t)object_size;

  return AFF4_OK;
}

/*
 * Aff4Read
 */
static int Aff4Read(void *p_handle,
                    char *p_buf,
                    off_t offset,
                    size_t count,
                    size_t *p_read,
                    int *p_errno)
{
  pts_Aff4Handle p_aff4_handle = (pts_Aff4Handle)p_handle;
  ssize_t bytes_read;

  // Read data
  // TODO: Check for errors
  bytes_read = AFF4_read(p_aff4_handle->h_aff4, (uint64_t)offset, p_buf, count);
  if ((size_t)bytes_read != count) return AFF4_READ_FAILED;

  *p_read = (size_t)bytes_read;
  return AFF4_OK;
}

/*
 * Aff4OptionsHelp
 */
static int Aff4OptionsHelp(const char **pp_help) {
  *pp_help = NULL;
  return AFF4_OK;
}

/*
 * Aff4OptionsParse
 */
static int Aff4OptionsParse(void *p_handle,
                            uint32_t options_count,
                            const pts_LibXmountOptions *pp_options,
                            const char **pp_error)
{
  return AFF4_OK;
}

/*
 * Aff4GetInfofileContent
 */
static int Aff4GetInfofileContent(void *p_handle, const char **pp_info_buf) {
  // TODO
  *pp_info_buf = NULL;
  return AFF4_OK;
}

/*
 * Aff4GetErrorMessage
 */
static const char* Aff4GetErrorMessage(int err_num) {
  switch(err_num) {
    case AFF4_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case AFF4_NO_INPUT_FILES:
      return "No input file specified";
      break;
    case AFF4_TOO_MANY_INPUT_FILES:
      return "Too many input files specified";
      break;
    case AFF4_OPEN_FAILED:
      return "Unable to open AFF4 image";
      break;
    case AFF4_CLOSE_FAILED:
      return "Unable to close AFF4 image";
      break;
    case AFF4_GETSIZE_FAILED:
      return "Unable to get size of AFF4 image";
      break;
    case AFF4_READ_FAILED:
      return "Unable to read AFF4 data";
      break;
    default:
      return "Unknown error";
  }
}

/*
 * Aff4FreeBuffer
 */
static int Aff4FreeBuffer(void *p_buf) {
  free(p_buf);
  return AFF4_OK;
}

/*
  ----- Change log -----
  20240318: * Initial version
*/

