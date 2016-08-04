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

#ifndef LIBXMOUNT_OUTPUT_H
#define LIBXMOUNT_OUTPUT_H

#define LIBXMOUNT_OUTPUT_API_VERSION 1

#include <config.h>

#include <stdlib.h> // For alloc, calloc, free
#include <stdio.h>  // For printf
#include <stdint.h> // For int*_t and uint*_t
#include <stdarg.h> // For va_*, vprintf
#include <inttypes.h> // For PRI*

#include "../libxmount/libxmount.h"

/*******************************************************************************
 * Type defs
 ******************************************************************************/
//! Struct containing pointers to input image functions
typedef struct s_LibXmountOutput_InputFunctions {
  //! Function to get the size of the morphed image
  /*!
   * \param p_size Pointer to store input image's size to
   * \return 0 on success
   */
  int (*Size)(uint64_t *p_size);

  //! Function to read data from the morphed image
  /*!
   * \param p_buf Buffer to store read data to
   * \param offset Position at which to start reading
   * \param count Amount of bytes to read
   * \param p_read Number of read bytes on success
   * \return 0 on success or negated error code on error
   */
  int (*Read)(char *p_buf,
              off_t offset,
              size_t count,
              size_t *p_read);

  //! Function to write data to the morphed image
  /*!
   * \param p_buf Buffer with data tp write
   * \param offset Position at which to start writing
   * \param count Amount of bytes to write
   * \param p_written Number of written bytes on success
   * \return 0 on success or negated error code on error
   */
  int (*Write)(char *p_buf,
               off_t offset,
               size_t count,
               size_t *p_written);
} ts_LibXmountOutput_InputFunctions, *pts_LibXmountOutput_InputFunctions;

//! Structure containing pointers to the lib's functions
typedef struct s_LibXmountOutput_Functions {
  //! Function to initialize handle
  /*!
   * This function is called once to allow the lib to alloc any needed
   * structures before other functions that rely upon a valid handle are called
   * (for ex. OptionsParse).
   *
   * The p_format parameter specifies one of the output formats returned by
   * LibXmount_Output_GetSupportedFormats() which should be used for this
   * handle.
   *
   * \param pp_handle Pointer to store handle to
   * \param p_format Output format to use
   * \param debug If set to 1, print debugging infos to stdout
   * \return 0 on success or error code
   */
  int (*CreateHandle)(void **pp_handle,
                      const char *p_format,
                      uint8_t debug);

  //! Function to destroy handle
  /*!
   * In this function, any structures allocated with CreateHandle should be
   * freed. It is generally the last function called before unloading of lib
   * happens.
   *
   * By convention, after this function has been called, *pp_handle must be
   * NULL.
   *
   * \param pp_handle Pointer to store handle to
   * \return 0 on success or error code
   */
  int (*DestroyHandle)(void **pp_handle);

  //! Function to transform morphed into output image
  /*!
   * Converts the input (morphed) image into the output (virtual) image.
   *
   * \param p_handle Handle
   * \param p_input_functions ts_LibXmountInputFunctions structure
   * \return 0 on success or error code
   */
  int (*Transform)(void *p_handle,
                   pts_LibXmountOutput_InputFunctions p_input_functions);

  //! Function to get the size of the output image
  /*!
   * \param p_handle Handle
   * \param p_size Pointer to store output image's size to
   * \return 0 on success or error code
   */
  int (*Size)(void *p_handle,
              uint64_t *p_size);

  //! Function to read data from output image
  /*!
   * Reads count bytes at offset from output image and copies them into memory
   * starting at the address of p_buf. Memory is pre-allocated to as much bytes
   * as should be read.
   *
   * \param p_handle Handle
   * \param p_buf Buffer to store read data to
   * \param offset Position at which to start reading
   * \param count Amount of bytes to read
   * \param p_read Number of read bytes on success
   * \return 0 on success or negated error code on error
   */
  int (*Read)(void *p_handle,
              char *p_buf,
              off_t offset,
              size_t count,
              size_t *p_read);

  //! Function to write data to output image
  /*!
   * Writes count bytes from buffer p_buf starting at offset to output image.
   *
   * \param p_handle Handle
   * \param p_buf Buffer with data to write
   * \param offset Position at which to start writing
   * \param count Amount of bytes to read
   * \param p_written Number of read bytes on success
   * \return 0 on success or negated error code on error
   */
  int (*Write)(void *p_handle,
               char *p_buf,
               off_t offset,
               size_t count,
               size_t *p_written);

  //! Function to get a help message for any supported lib-specific options
  /*!
   * Calling this function should return a string containing help messages for
   * any supported lib-specific options. Lines should be formated as follows:
   *
   * "    option : description\n"
   *
   * Returned string will be freed by the caller using FreeBuffer().
   *
   * If there is no help text, this function must return NULL in pp_help.
   *
   * \param Pointer to a string to return help text
   * \return 0 on success or error code on error
   */
  int (*OptionsHelp)(const char **pp_help);

  //! Function to parse any lib-specific options
  /*!
   * This function is called with the option string given after the --outopts
   * parameter. All contained options are for the lib. If errors or unknown
   * options are found, this function should fail and return an error message
   * in pp_error. pp_error will be freed by the caller using FreeBuffer.
   *
   * \param p_handle Handle
   * \param p_options String with specified options
   * \param pp_error Pointer to a string with error message
   * \return 0 on success or error code and error message
   */
  int (*OptionsParse)(void *p_handle,
                      uint32_t options_count,
                      const pts_LibXmountOptions *pp_options,
                      const char **pp_error);

  //! Function to get content to add to the info file
  /*!
   * The returned string is added to xmount's info file. This function is only
   * called once when the info file is generated. The returned string is then
   * freed with a call to FreeBuffer.
   *
   * \param p_handle Handle
   * \param pp_info_buf Pointer to store the null-terminated content
   * \return 0 on success or error code
   */
  int (*GetInfofileContent)(void *p_handle,
                            const char **pp_info_buf);

  //! Function to get an error message
  /*!
   * This function should translate an error code that was previously returned
   * by one of the library functions into a human readable error message.
   *
   * By convention, this function must always return a valid pointer to a
   * NULL-terminated string!
   *
   * \param err_num Error code as returned by lib
   */
  const char* (*GetErrorMessage)(int err_num);

  //! Function to free buffers that were allocated by lib
  /*!
   * \param p_buf Buffer to free
   */
  void (*FreeBuffer)(void *p_buf);
} ts_LibXmountOutput_Functions, *pts_LibXmountOutput_Functions;

/*******************************************************************************
 * API functions
 ******************************************************************************/
//! Get library API version
/*!
 * This function should return the value of LIBXMOUNT_OUTPUT_API_VERSION
 *
 * \return Supported version
 */
uint8_t LibXmount_Output_GetApiVersion();
typedef uint8_t (*t_LibXmount_Output_GetApiVersion)();

//! Get a list of supported output formats
/*!
 * Gets a list of supported output formats. These are the strings
 * specified with xmount's --out <string> command line option. The returned
 * string must be a constant vector of output formats split by \0 chars. To
 * mark the end of the vector, a single \0 must be used.
 *
 * As an example, "first\0second\0\0" would be a correct string to return for
 * a lib supporting two output formats.
 *
 * \return Vector containing supported output formats
 */
const char* LibXmount_Output_GetSupportedFormats();
typedef const char* (*t_LibXmount_Output_GetSupportedFormats)();

//! Get the lib's s_LibXmountOutput_Functions structure
/*!
 * This function should set the members of the given
 * s_LibXmountOutputFunctions structure to the internal lib functions. All
 * members have to be set.
 *
 * \param p_functions s_LibXmountOutput_Functions structure to fill
 */
void LibXmount_Output_GetFunctions(pts_LibXmountOutput_Functions p_functions);
typedef void (*t_LibXmount_Output_GetFunctions)(pts_LibXmountOutput_Functions);

#endif // LIBXMOUNT_OUTPUT_H

