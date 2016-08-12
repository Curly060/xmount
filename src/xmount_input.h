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

#ifndef XMOUNT_INPUT_H
#define XMOUNT_INPUT_H

#include <stdint.h>

/*******************************************************************************
 * Public definitions / macros
 ******************************************************************************/
//! Naming scheme of input libraries
#define XMOUNT_INPUT_LIBRARY_NAMING_SCHEME "libxmount_input_"

/*******************************************************************************
 * Public types / structures / enums
 ******************************************************************************/
typedef struct s_XmountInputHandle *pts_XmountInputHandle;

typedef enum e_XmountInput_Error {
  //! No error
  e_XmountInput_Error_None=0,
  //! Error to allocate memory
  e_XmountInput_Error_Alloc,
  //! Invalid input handle
  e_XmountInput_Error_InvalidHandle,
  //! Invalid pointer to an input handle
  e_XmountInput_Error_InvalidHandlePointer,
  //! A given buffer is invalid
  e_XmountInput_Error_InvalidBuffer,
  //! A given string is invalid
  e_XmountInput_Error_InvalidString,
  //! A given array is invalid
  e_XmountInput_Error_InvalidArray,
  //! Library options have already been set
  e_XmountInput_Error_LibOptionsAlreadySet,
  //! Library options couldn't be parsed
  e_XmountInput_Error_FailedParsingOptions,
  //! Unable to get info file content from library
  e_XmountInput_Error_FailedGettingInfoFileContent,
  //! Unable to load library file
  e_XmountInput_Error_FailedLoadingLibrary,
  //! Library has wrong API version
  e_XmountInput_Error_WrongLibraryApiVersion,
  //! Library is missing a function
  e_XmountInput_Error_MissingLibraryFunction,
  //! Unsupported input image format
  e_XmountInput_Error_UnsupportedFormat,
  //! Specified image number is incorrect
  e_XmountInput_Error_NoSuchImage,
/*

  //! A given file path / name is invalid
  e_XmountCache_Error_InvalidFile,
  //! A given file does not exist
  e_XmountCache_Error_InexistingFile,
  //! A given file exists
  e_XmountCache_Error_ExistingFile,
  //! Unable to create needed xmount structures inside cache file
  e_XmountCache_Error_FailedCacheInit,
  //! Unable to open xmount cache file
  e_XmountCache_Error_FailedOpeningCache,
  //! Failed to get block cache index size
  e_XmountCache_Error_FailedGettingIndexSize,
  //! Invalid block cache index size
  e_XmountCache_Error_InvalidIndexSize,
  //! Unable to read block cache index
  e_XmountCache_Error_FailedReadingIndex,
  //! Failed closing cache block index
  e_XmountCache_Error_FailedClosingIndex,
  //! Failed closing cache block index
  e_XmountCache_Error_FailedClosingBlockCache,
  //! Failed closing cache block index
  e_XmountCache_Error_FailedClosingCache,
  //! Failed to update block cache index
  e_XmountCache_Error_FailedUpdatingIndex,
  //! Invalid block cache index specified
  e_XmountCache_Error_InvalidIndex,
  //! Block has not yet been cached
  e_XmountCache_Error_UncachedBlock,
  //! Invalid buffer specified
  e_XmountCache_Error_InvalidBuffer,
  //! Request would read beyond a single cache block
  e_XmountCache_Error_ReadBeyondBlockBounds,
  //! Failed reading cached data
  e_XmountCache_Error_FailedReadingBlockCache,
  //! Failed writing cached data
  e_XmountCache_Error_FailedWritingBlockCache,
*/
} te_XmountInput_Error;

/*******************************************************************************
 * Public functions declarations
 ******************************************************************************/
/*!
 * \brief Create new input handle
 *
 * Creates a new input handle.
 *
 * \param pp_h Pointer to input handle
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_CreateHandle(pts_XmountInputHandle *pp_h);

/*!
 * \brief Destroy input handle
 *
 * Invalidates the given handle and frees all used resources.
 *
 * \param pp_h Pointer to input handle
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_DestroyHandle(pts_XmountInputHandle *pp_h);

/*!
 * \brief Load an input library
 *
 * Loads a given input library.
 *
 * \param p_h Input handle
 * \param p_lib Library name (without path)
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_AddLibrary(pts_XmountInputHandle p_h,
                                            const char *p_lib_name);

/*!
 * \brief Get loaded input library count
 *
 * Returns the number of successfully loaded input libraries.
 *
 * \param p_h Input handle
 * \param p_count Library count is returned in this variable
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_GetLibraryCount(pts_XmountInputHandle p_h,
                                                 uint32_t *p_count);

/*!
 * \brief Return all supported formats
 *
 * Returns a null-terminated vector of all supported input formats.
 *
 * The returned vector must be freed by the caller.
 *
 * \param p_h Input handle
 * \param pp_formats Supported formats vector is returned in this var
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_GetSupportedFormats(pts_XmountInputHandle p_h,
                                                     char **pp_formats);

/*!
 * \brief Set library options
 *
 * Parses the given library option string (as given after --inopts).
 *
 * \param p_h Input handle
 * \param p_options Library option string
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_SetOptions(pts_XmountInputHandle p_h,
                                            char *p_options);

/*!
 * \brief Return all library specific option help texts
 *
 * Returns a string containing help messages for all loaded input lib options.
 * The string is pre-formated to be used in xmount's help output.
 *
 * The caller must free the returned string.
 *
 * \param p_h Input handle
 * \param pp_help_text Help text is returned in this parameter
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_GetOptionsHelpText(pts_XmountInputHandle p_h,
                                                    char **pp_help_text);

/*!
 * \brief Returns a string containing infos about loaded libs
 *
 * Returns a string containing infos about loaded input libraries. The string is
 * pre-formated to be used in xmount's info output.
 *
 * The caller must free the returned string.
 *
 * \param p_h Input handle
 * \param pp_info_text Info text is returned in this parameter
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_GetLibsInfoText(pts_XmountInputHandle p_h,
                                                 char **pp_info_text);

/*!
 * \brief Add an input image
 *
 * Adds the given input image to the list of available input images.
 *
 * \param p_h Input handle
 * \param p_format Input image format string as given by the user
 * \param files_count Amount of files specified by the user
 * \param pp_files Array containing all specified files
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_AddImage(pts_XmountInputHandle p_h,
                                          const char *p_format,
                                          uint64_t files_count,
                                          const char **pp_files);

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
                                               uint64_t *p_count);

/*!
 * \brief Set an input image offset
 *
 * Sets the amount of bytes that should be ignored at the beginning of every
 * input image.
 *
 * \param p_h Input handle
 * \param offset Amount of bytes to ignore
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_SetInputOffset(pts_XmountInputHandle p_h,
                                                uint64_t offset);

/*!
 * \brief Set an input image size limit
 *
 * Sets the amount of bytes that should be ignored at the end of every
 * input image.
 *
 * \param p_h Input handle
 * \param size_limit Amount of bytes to ignore
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_SetInputSizeLimit(pts_XmountInputHandle p_h,
                                                   uint64_t size_limit);

/*!
 * \brief Opens all added input images
 *
 * Opens all added input images.
 *
 * \param p_h Input handle
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_Open(pts_XmountInputHandle p_h);

/*!
 * \brief Closes all previously opened input images
 *
 * Closes all previously opened input images.
 *
 * \param p_h Input handle
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_Close(pts_XmountInputHandle p_h);

/*!
 * \brief Get the size of an input image
 *
 * Returns the size (in bytes) of the specified input image.
 *
 * \param p_h Input handle
 * \param image_nr Image number for which to return the size
 * \param p_size On success, size is returned in this variable
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_GetSize(pts_XmountInputHandle p_h,
                                         uint64_t image_nr,
                                         uint64_t *p_size);

/*!
 * \brief Read data from an input image
 *
 * Reads count bytes from input image image_nr starting at given offset and
 * copies the data into p_buf.
 *
 * The given buffer must be pre-allocated to hold as many bytes as should be
 * read!
 *
 * \param p_h Input handle
 * \param image_nr Image number for which to return the size
 * \param p_buf Buffer into which to copy read data
 * \param offset Offset at which to start reading
 * \param count Amount of bytes to read
 * \param p_read On success, amount of bytes read is returned in this variable
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_ReadData(pts_XmountInputHandle p_h,
                                          uint64_t image_nr,
                                          char *p_buf,
                                          uint64_t offset,
                                          uint64_t count,
                                          uint64_t *p_read);

te_XmountInput_Error XmountInput_WriteData(pts_XmountInputHandle p_h,
                                           uint64_t image_nr,
                                           const char *p_buf,
                                           uint64_t offset,
                                           uint64_t count);

/*!
 * \brief Get info text to be added to xmount's info file
 *
 * Generates a string containing informations about currently opened input
 * images.
 *
 * The caller must free the returned string.
 *
 * \param p_h Input handle
 * \param pp_content Buffer in which text is returned
 * \return e_XmountInput_Error_None on success
 */
te_XmountInput_Error XmountInput_GetInfoFileContent(pts_XmountInputHandle p_h,
                                                    char **pp_content);

#endif // XMOUNT_INPUT_H
