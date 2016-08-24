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

#ifndef XMOUNT_OUTPUT_H
#define XMOUNT_OUTPUT_H

/*******************************************************************************
 * Public definitions / macros
 ******************************************************************************/
//! Naming scheme of morphing libraries
#define XMOUNT_OUTPUT_LIBRARY_NAMING_SCHEME "libxmount_output_"

/*******************************************************************************
 * Public types / structures / enums
 ******************************************************************************/
//! Output handle
typedef struct s_XmountOutputHandle *pts_XmountOutputHandle;

/*!
 * \brief Function to get the size of the morphed data
 *
 * Function to get the size of the morphed data
 *
 * \param image Image number
 * \param p_size Pointer to store input image's size to
 * \return 0 on success
 */
typedef int (*tfun_XmountOutput_InputImageSize)(uint64_t image,
                                                uint64_t *p_size);

//! Function to read data from input image
/*!
 * \param image Image number
 * \param p_buf Buffer to store read data to
 * \param offset Position at which to start reading
 * \param count Amount of bytes to read
 * \param p_read Number of read bytes on success
 * \return 0 on success or negated error code on error
 */
typedef int (*tfun_XmountOutput_InputImageRead)(uint64_t image,
                                                char *p_buf,
                                                off_t offset,
                                                size_t count,
                                                size_t *p_read);

//! Function to write data to input image
/*!
 * \param image Image number
 * \param p_buf Buffer to store read data to
 * \param offset Position at which to start reading
 * \param count Amount of bytes to read
 * \param p_read Number of read bytes on success
 * \return 0 on success or negated error code on error
 */
typedef int (*tfun_XmountOutput_InputImageWrite)(uint64_t image,
                                                 char *p_buf,
                                                 off_t offset,
                                                 size_t count,
                                                 size_t *p_written);

typedef enum e_XmountOutputError {
  //! No error
  e_XmountOutputError_None=0,
  //! Error to allocate memory
  e_XmountOutputError_Alloc,
  //! Invalid morphing handle
  e_XmountOutputError_InvalidHandle,
  //! Invalid pointer to a morphing handle
  e_XmountOutputError_InvalidHandlePointer,
  //! A given buffer is invalid
  e_XmountOutputError_InvalidBuffer,
  //! A given string is invalid
  e_XmountOutputError_InvalidString,
/*
  //! Library options have already been set
  e_XmountMorphError_LibOptionsAlreadySet,
  //! Library options couldn't be parsed
  e_XmountMorphError_FailedParsingOptions,
  //! Unable to get info file content from library
  e_XmountMorphError_FailedGettingInfoFileContent,
  //! Unable to load library file
  e_XmountMorphError_FailedLoadingLibrary,
  //! Unable to load a library symbol
  e_XmountMorphError_FailedLoadingSymbol,
  //! Library has wrong API version
  e_XmountMorphError_WrongLibraryApiVersion,
  //! Library is missing a function
  e_XmountMorphError_MissingLibraryFunction,
  //! Unsupported morphing type
  e_XmountMorphError_UnsupportedType,
  //! Unable to create morphing image handle
  e_XmountMorphError_FailedCreatingMorphHandle,
  //! Unable to parse morphing library options
  e_XmountMorphError_FailedParsingLibParams,
  //! Unable to get image size
  e_XmountMorphError_FailedGettingImageSize,
  //! A specified offset is larger than the image
  e_XmountMorphError_OffsetExceedsImageSize,
  //! Unable to read data from morphed image
  e_XmountMorphError_FailedReadingData
*/
} te_XmountOutputError;

/*******************************************************************************
 * Public functions declarations
 ******************************************************************************/
/*!
 * \brief Create new output handle
 *
 * Creates a new output handle.
 *
 * \param pp_h Pointer to output handle
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError
  XmountOutput_CreateHandle(pts_XmountOutputHandle *pp_h,
                            tfun_XmountOutput_InputImageSize p_img_size,
                            tfun_XmountOutput_InputImageRead p_img_read,
                            tfun_XmountOutput_InputImageWrite p_img_write);

/*!
 * \brief Destroy output handle
 *
 * Invalidates the given handle and frees all used resources.
 *
 * \param pp_h Pointer to output handle
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_DestroyHandle(pts_XmountOutputHandle *pp_h);

/*!
 * \brief Enable internal debugging
 *
 * Enables the generation of intrernal debugging messages.
 *
 * \param p_h Output handle
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_EnableDebugging(pts_XmountOutputHandle p_h);

/*!
 * \brief Load an output library
 *
 * Loads a given output library.
 *
 * \param p_h Output handle
 * \param p_lib Library name (without path)
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_AddLibrary(pts_XmountOutputHandle p_h,
                                              const char *p_lib_name);

/*!
 * \brief Get loaded output library count
 *
 * Returns the number of successfully loaded output libraries.
 *
 * \param p_h Output handle
 * \param p_count Library count is returned in this variable
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_GetLibraryCount(pts_XmountOutputHandle p_h,
                                                   uint32_t *p_count);

/*!
 * \brief Return all supported output formats
 *
 * Returns a null-terminated vector of all supported output formats.
 *
 * The returned vector must be freed by the caller.
 *
 * \param p_h Output handle
 * \param pp_types Supported formats vector is returned in this var
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError
  XmountOutput_GetSupportedFormats(pts_XmountOutputHandle p_h,
                                   char **pp_formats);

/*!
 * \brief Set library options
 *
 * Parses the given library option string (as given after --outopts).
 *
 * \param p_h Output handle
 * \param p_options Library option string
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_SetOptions(pts_XmountOutputHandle p_h,
                                              char *p_options);

/*!
 * \brief Return all library specific option help texts
 *
 * Returns a string containing help messages for all loaded output lib
 * options. The string is pre-formated to be used in xmount's help output.
 *
 * The caller must free the returned string.
 *
 * \param p_h Output handle
 * \param pp_help_text Help text is returned in this parameter
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_GetOptionsHelpText(pts_XmountOutputHandle p_h,
                                                     char **pp_help_text);

/*!
 * \brief Returns a string containing infos about loaded libs
 *
 * Returns a string containing infos about loaded output libraries. The
 * string is pre-formated to be used in xmount's info output.
 *
 * The caller must free the returned string.
 *
 * \param p_h Output handle
 * \param pp_info_text Info text is returned in this parameter
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_GetLibsInfoText(pts_XmountOutputHandle p_h,
                                                  char **pp_info_text);

/*!
 * \brief Set output format
 *
 * Set the output format.
 *
 * \param p_h Output handle
 * \param p_type Output format string as specified with xmount's --out option.
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_SetType(pts_XmountOutputHandle p_h,
                                          char *p_format);

/*!
 * \brief Generate output image
 *
 * Gets the output image ready for usage.
 *
 * \param p_h Output handle
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_Open(pts_XmountOutputHandle p_h);

/*!
 * \brief Destroy output image
 *
 * Frees what was necessary to use the output image.
 *
 * \param p_h Output handle
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_Close(pts_XmountOutputHandle p_h);

/*!
 * \brief Returns an array containing all output files names
 *
 * TODO: Describe
 *
 * \param p_h Output handle
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError
  XmountOutput_GetOutputFilenames(pts_XmountOutputHandle p_h,
                                  char ***ppp_output_files);

/*!
 * \brief Get the size of the output image
 *
 * Returns the size (in bytes) of the specified output image.
 *
 * \param p_h Output handle
 * \param p_output_filename Output file for which to return the size
 * \param p_size On success, size is returned in this variable
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_GetSize(pts_XmountOutputHandle p_h,
                                          const char *p_output_filename,
                                          uint64_t *p_size);

/*!
 * \brief Read data from an output image
 *
 * Reads count bytes from the specified output image starting at given offset
 * and copies the data into p_buf.
 *
 * The given buffer must be pre-allocated to hold as many bytes as should be
 * read!
 *
 * \param p_h Output handle
 * \param p_output_filename Output file for which to return the size
 * \param p_buf Buffer into which to copy read data
 * \param offset Offset at which to start reading
 * \param count Amount of bytes to read
 * \param p_read On success, amount of bytes read is returned in this variable
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_ReadData(pts_XmountOutputHandle p_h,
                                           const char *p_output_filename,
                                           char *p_buf,
                                           uint64_t offset,
                                           uint64_t count,
                                           uint64_t *p_read);

/*!
 * \brief Writes data to an output image
 *
 * Writes count bytes from p_buf to the specified output image starting at the
 * given offset.
 *
 * \param p_h Output handle
 * \param p_output_filename Output file for which to return the size
 * \param p_buf Buffer with data to write
 * \param offset Offset at which to start writing
 * \param count Amount of bytes to write
 * \param p_written On success, amount of bytes written is returned here
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_WriteData(pts_XmountOutputHandle p_h,
                                            const char *p_output_filename,
                                            const char *p_buf,
                                            uint64_t offset,
                                            uint64_t count,
                                            uint64_t *p_written);

/*!
 * \brief Get info text to be added to xmount's info file
 *
 * Generates a string containing informations about the output image.
 *
 * The caller must free the returned string.
 *
 * \param p_h Output handle
 * \param pp_content Buffer in which text is returned
 * \return e_XmountMorphError_None on success
 */
te_XmountOutputError XmountOutput_GetInfoFileContent(pts_XmountOutputHandle p_h,
                                                     char **pp_content);

#endif // XMOUNT_OUTPUT_H
