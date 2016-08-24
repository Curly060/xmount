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

#ifndef XMOUNT_MORPHING_H
#define XMOUNT_MORPHING_H

/*******************************************************************************
 * Public definitions / macros
 ******************************************************************************/
//! Naming scheme of morphing libraries
#define XMOUNT_MORPHING_LIBRARY_NAMING_SCHEME "libxmount_morph_"

/*******************************************************************************
 * Public types / structures / enums
 ******************************************************************************/
//! Morphing handle
typedef struct s_XmountMorphHandle *pts_XmountMorphHandle;

/*!
 * \brief Function to get the amount of input images
 *
 * Function to get the amount of input images
 *
 * \param p_count Count of input images
 * \return 0 on success
 */
typedef int (*tfun_XmountMorphing_InputImageCount)(uint64_t *p_count);

/*!
 * \brief Function to get the size of the morphed data
 *
 * Function to get the size of the morphed data
 *
 * \param image Image number
 * \param p_size Pointer to store input image's size to
 * \return 0 on success
 */
typedef int (*tfun_XmountMorphing_InputImageSize)(uint64_t image,
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
typedef int (*tfun_XmountMorphing_InputImageRead)(uint64_t image,
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
typedef int (*tfun_XmountMorphing_InputImageWrite)(uint64_t image,
                                                   char *p_buf,
                                                   off_t offset,
                                                   size_t count,
                                                   size_t *p_written);

typedef enum e_XmountMorphError {
  //! No error
  e_XmountMorphError_None=0,
  //! Error to allocate memory
  e_XmountMorphError_Alloc,
  //! Invalid morphing handle
  e_XmountMorphError_InvalidHandle,
  //! Invalid pointer to a morphing handle
  e_XmountMorphError_InvalidHandlePointer,
  //! A given buffer is invalid
  e_XmountMorphError_InvalidBuffer,
  //! A given string is invalid
  e_XmountMorphError_InvalidString,
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
} te_XmountMorphError;

/*******************************************************************************
 * Public functions declarations
 ******************************************************************************/
/*!
 * \brief Create new morphing handle
 *
 * Creates a new morphing handle.
 *
 * \param pp_h Pointer to morphing handle
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError
  XmountMorphing_CreateHandle(pts_XmountMorphHandle *pp_h,
                              tfun_XmountMorphing_InputImageCount p_img_count,
                              tfun_XmountMorphing_InputImageSize p_img_size,
                              tfun_XmountMorphing_InputImageRead p_img_read,
                              tfun_XmountMorphing_InputImageWrite p_img_write);

/*!
 * \brief Destroy morphing handle
 *
 * Invalidates the given handle and frees all used resources.
 *
 * \param pp_h Pointer to morphing handle
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_DestroyHandle(pts_XmountMorphHandle *pp_h);

/*!
 * \brief Enable internal debugging
 *
 * Enables the generation of intrernal debugging messages.
 *
 * \param p_h Morphing handle
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_EnableDebugging(pts_XmountMorphHandle p_h);

/*!
 * \brief Load a morphing library
 *
 * Loads a given morphing library.
 *
 * \param p_h Morphing handle
 * \param p_lib Library name (without path)
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_AddLibrary(pts_XmountMorphHandle p_h,
                                              const char *p_lib_name);

/*!
 * \brief Get loaded morphing library count
 *
 * Returns the number of successfully loaded morphing libraries.
 *
 * \param p_h Morphing handle
 * \param p_count Library count is returned in this variable
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_GetLibraryCount(pts_XmountMorphHandle p_h,
                                                   uint32_t *p_count);

/*!
 * \brief Return all supported morphing types
 *
 * Returns a null-terminated vector of all supported morphing types.
 *
 * The returned vector must be freed by the caller.
 *
 * \param p_h Morphing handle
 * \param pp_types Supported types vector is returned in this var
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_GetSupportedTypes(pts_XmountMorphHandle p_h,
                                                     char **pp_types);

/*!
 * \brief Set library options
 *
 * Parses the given library option string (as given after --morphopts).
 *
 * \param p_h Morphing handle
 * \param p_options Library option string
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_SetOptions(pts_XmountMorphHandle p_h,
                                              char *p_options);

/*!
 * \brief Return all library specific option help texts
 *
 * Returns a string containing help messages for all loaded morphing lib
 * options. The string is pre-formated to be used in xmount's help output.
 *
 * The caller must free the returned string.
 *
 * \param p_h Morphing handle
 * \param pp_help_text Help text is returned in this parameter
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_GetOptionsHelpText(pts_XmountMorphHandle p_h,
                                                      char **pp_help_text);

/*!
 * \brief Returns a string containing infos about loaded libs
 *
 * Returns a string containing infos about loaded morphing libraries. The
 * string is pre-formated to be used in xmount's info output.
 *
 * The caller must free the returned string.
 *
 * \param p_h Morphing handle
 * \param pp_info_text Info text is returned in this parameter
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_GetLibsInfoText(pts_XmountMorphHandle p_h,
                                                    char **pp_info_text);

/*!
 * \brief Set morphing type
 *
 * Set the morphing type that should be applied to the input image.
 *
 * \param p_h Morphing handle
 * \param p_type Morphing type string as specified with xmount's --morph option.
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_SetType(pts_XmountMorphHandle p_h,
                                           char *p_type);

/*!
 * \brief Start the morphing process
 *
 * Begins the morphing process.
 *
 * \param p_h Morphing handle
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_StartMorphing(pts_XmountMorphHandle p_h);

/*!
 * \brief Stop the morphing process
 *
 * Ends the morphing process.
 *
 * \param p_h Morphing handle
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_StopMorphing(pts_XmountMorphHandle p_h);

/*!
 * \brief Get the size of the morphed image
 *
 * Returns the size (in bytes) of the morphed image.
 *
 * \param p_h Morphing handle
 * \param image_nr Image number for which to return the size
 * \param p_size On success, size is returned in this variable
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_GetSize(pts_XmountMorphHandle p_h,
                                           uint64_t *p_size);

/*!
 * \brief Read data from the morphed image
 *
 * Reads count bytes from the morphed image starting at given offset and
 * copies the data into p_buf.
 *
 * The given buffer must be pre-allocated to hold as many bytes as should be
 * read!
 *
 * \param p_h Morphing handle
 * \param p_buf Buffer into which to copy read data
 * \param offset Offset at which to start reading
 * \param count Amount of bytes to read
 * \param p_read On success, amount of bytes read is returned in this variable
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_ReadData(pts_XmountMorphHandle p_h,
                                            char *p_buf,
                                            uint64_t offset,
                                            uint64_t count,
                                            uint64_t *p_read);

/*!
 * \brief Writes data to the morphed image
 *
 * Writes count bytes from p_buf to the morphed image starting at the given
 * offset.
 *
 * \param p_h Morphing handle
 * \param p_buf Buffer with data to write
 * \param offset Offset at which to start writing
 * \param count Amount of bytes to write
 * \param p_written On success, amount of bytes written is returned here
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_WriteData(pts_XmountMorphHandle p_h,
                                             const char *p_buf,
                                             uint64_t offset,
                                             uint64_t count,
                                             uint64_t *p_written);

/*!
 * \brief Get info text to be added to xmount's info file
 *
 * Generates a string containing informations about the currently morphed image.
 *
 * The caller must free the returned string.
 *
 * \param p_h Morphing handle
 * \param pp_content Buffer in which text is returned
 * \return e_XmountMorphError_None on success
 */
te_XmountMorphError XmountMorphing_GetInfoFileContent(pts_XmountMorphHandle p_h,
                                                      char **pp_content);

#endif // XMOUNT_MORPHING_H
