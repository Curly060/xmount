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

#ifndef LIBXMOUNT_MORPHING_BYTESWAP_H
#define LIBXMOUNT_MORPHING_BYTESWAP_H

/*******************************************************************************
 * Enums, type defs, etc...
 ******************************************************************************/
enum {
  BYTESWAP_OK=0,
  BYTESWAP_MEMALLOC_FAILED,
  BYTESWAP_CANNOT_GET_IMAGECOUNT,
  BYTESWAP_CANNOT_GET_IMAGESIZE,
  BYTESWAP_READ_BEYOND_END_OF_IMAGE,
  BYTESWAP_CANNOT_READ_DATA,
  BYTESWAP_UNSUPPORTED_IMAGE_SIZE
};

typedef struct s_ByteswapHandle {
  uint8_t debug;
  uint64_t input_images_count;
  pts_LibXmountMorphingInputFunctions p_input_functions;
  uint64_t morphed_image_size;
} ts_ByteswapHandle, *pts_ByteswapHandle;

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int ByteswapCreateHandle(void **pp_handle,
                                const char *p_format,
                                uint8_t debug);
static int ByteswapDestroyHandle(void **pp_handle);
static int ByteswapMorph(void *p_handle,
                         pts_LibXmountMorphingInputFunctions p_input_functions);
static int ByteswapSize(void *p_handle,
                        uint64_t *p_size);
static int ByteswapRead(void *p_handle,
                        char *p_buf,
                        off_t offset,
                        size_t count,
                        size_t *p_read);
static int ByteswapOptionsHelp(const char **pp_help);
static int ByteswapOptionsParse(void *p_handle,
                                uint32_t options_count,
                                const pts_LibXmountOptions *pp_options,
                                const char **pp_error);
static int ByteswapGetInfofileContent(void *p_handle,
                                      const char **pp_info_buf);
static const char* ByteswapGetErrorMessage(int err_num);
static void ByteswapFreeBuffer(void *p_buf);

#endif // LIBXMOUNT_MORPHING_BYTESWAP_H

