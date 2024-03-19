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

#ifndef LIBXMOUNT_INPUT_AFF4_H
#define LIBXMOUNT_INPUT_AFF4_H

/*******************************************************************************
 * Enums, Typedefs, etc...
 ******************************************************************************/
//! Possible error return codes
enum {
  AFF4_OK=0,
  AFF4_MEMALLOC_FAILED,
  AFF4_NO_INPUT_FILES,
  AFF4_TOO_MANY_INPUT_FILES,
  AFF4_OPEN_FAILED,
  AFF4_CLOSE_FAILED,
  AFF4_GETSIZE_FAILED,
  AFF4_READ_FAILED
};

//! Library handle
typedef struct s_Aff4Handle {
  //! AFF4 handle
  int h_aff4;
} ts_Aff4Handle, *pts_Aff4Handle;

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int Aff4Init(void **pp_init_handle);
static int Aff4DeInit(void **pp_init_handle);
static int Aff4CreateHandle(void **pp_handle,
                            void *p_init_handle,
                            const char *p_format,
                            uint8_t debug);
static int Aff4DestroyHandle(void **pp_handle);
static int Aff4Open(void *p_handle,
                    const char **pp_filename_arr,
                    uint64_t filename_arr_len);
static int Aff4Close(void *p_handle);
static int Aff4Size(void *p_handle,
                    uint64_t *p_size);
static int Aff4Read(void *p_handle,
                    char *p_buf,
                    off_t seek,
                    size_t count,
                    size_t *p_read,
                    int *p_errno);
static int Aff4OptionsHelp(const char **pp_help);
static int Aff4OptionsParse(void *p_handle,
                            uint32_t options_count,
                            const pts_LibXmountOptions *pp_options,
                            const char **pp_error);
static int Aff4GetInfofileContent(void *p_handle,
                                  const char **pp_info_buf);
static const char* Aff4GetErrorMessage(int err_num);
static int Aff4FreeBuffer(void *p_buf);

#endif // LIBXMOUNT_INPUT_AFF4_H

