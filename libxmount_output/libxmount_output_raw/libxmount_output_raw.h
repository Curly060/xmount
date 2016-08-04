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

#ifndef LIBXMOUNT_OUTPUT_RAW_H
#define LIBXMOUNT_OUTPUT_RAW_H

/*******************************************************************************
 * Enums, type defs, etc...
 ******************************************************************************/
enum {
  RAW_OK=0,
  RAW_MEMALLOC_FAILED,
  RAW_CANNOT_GET_IMAGESIZE,
  RAW_READ_BEYOND_END_OF_IMAGE,
  RAW_WRITE_BEYOND_END_OF_IMAGE,
  RAW_CANNOT_READ_DATA,
  RAW_CANNOT_WRITE_DATA
};

typedef struct s_RawHandle {
  uint8_t debug;
  pts_LibXmountOutput_InputFunctions p_input_functions;
  uint64_t output_image_size;
} ts_RawHandle, *pts_RawHandle;

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int RawCreateHandle(void **pp_handle,
                           const char *p_format,
                           uint8_t debug);
static int RawDestroyHandle(void **pp_handle);
static int RawTransform(void *p_handle,
                        pts_LibXmountOutput_InputFunctions p_input_functions);
static int RawSize(void *p_handle,
                   uint64_t *p_size);
static int RawRead(void *p_handle,
                   char *p_buf,
                   off_t offset,
                   size_t count,
                   size_t *p_read);
static int RawWrite(void *p_handle,
                    char *p_buf,
                    off_t offset,
                    size_t count,
                    size_t *p_written);
static int RawOptionsHelp(const char **pp_help);
static int RawOptionsParse(void *p_handle,
                           uint32_t options_count,
                           const pts_LibXmountOptions *pp_options,
                           const char **pp_error);
static int RawGetInfofileContent(void *p_handle,
                                 const char **pp_info_buf);
static const char* RawGetErrorMessage(int err_num);
static void RawFreeBuffer(void *p_buf);

#endif // LIBXMOUNT_OUTPUT_RAW_H
