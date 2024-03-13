/*******************************************************************************
* xmount Copyright (c) 2024 by SITS Sarl                                       *
*                                                                              *
* Author(s):                                                                   *
*   Guy Voncken <deve‍lop@f‍aert.n‍et>                                            *
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

#ifndef LIBXMOUNT_INPUT_QCOW_H
#define LIBXMOUNT_INPUT_QCOW_H

#include <stdint.h>
#include <stdio.h>
#include "../libxmount_input.h"

/*******************************************************************************
 * Error codes etc...
 ******************************************************************************/
enum {
   QCOW_OK = 0,
   QCOW_MEMALLOC_FAILED,
   QCOW_FILE_OPEN_FAILED,
   QCOW_CANNOT_READ_DATA,
   QCOW_CANNOT_CLOSE_FILE,
   QCOW_FILE_TOO_SMALL,
   QCOW_BAD_MAGIC_HEADER,
   QCOW_BAD_VERSION,
   QCOW_UNSUPPORTED_ENCRYPTION,
   QCOW_CANNOT_SEEK,
   QCOW_UNABLE_TO_DECOMPRESS_CLUSTER,
   QCOW_READ_BEYOND_END_OF_IMAGE,
   QCOW_BAD_L1_OFFSET
};

// ----------------------
//  Constant definitions
// ----------------------

#define GETMAX(a,b) ((a)>(b)?(a):(b))
#define GETMIN(a,b) ((a)<(b)?(a):(b))

// ---------------------
//  Types and strutures
// ---------------------
typedef struct {
   uint32_t Magic;
   uint32_t Version;
   uint64_t BackingFileOffset;
   uint32_t BackingFileSize;
   uint32_t ClusterBits;
   uint64_t Size; /* in bytes */
   uint32_t CryptMethod;
   uint32_t L1Size;
   uint64_t L1TableOffset;
   uint64_t RefCountTableOffset;
   uint32_t RefCountTableClusters;
   uint32_t NbSnapshots;
   uint64_t SnapshotsOffset;
} t_QcowHeader, *t_pQcowHeader;

typedef struct {
   char     *pFilename;
   FILE     *pFile;
   uint64_t   FileSize;
   t_QcowHeader Header;
   uint64_t* pL1Table;
   uint32_t L2Bits;
   uint64_t L2Size;
   uint32_t L1Bits;
   uint64_t ClusterSize;
} t_Qcow, *t_pQcow;

// ----------------
//  Error handling
// ----------------

#ifdef QCOW_DEBUG
#define CHK(ChkVal)    \
   {                                                                  \
      int ChkValRc;                                                   \
      if ((ChkValRc=(ChkVal)) != QCOW_OK)                               \
      {                                                               \
         printf ("Err %d in %s, %d\n", ChkValRc, __FILE__, __LINE__); \
         return ChkValRc;                                             \
      }                                                               \
   }
#define DEBUG_PRINTF(pFormat, ...) \
      printf (pFormat, ##__VA_ARGS__);
#else
#define CHK(ChkVal)                      \
   {                                        \
      int ChkValRc;                         \
      if ((ChkValRc=(ChkVal)) != QCOW_OK)     \
         return ChkValRc;                   \
   }
#define DEBUG_PRINTF(...)
#endif

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int QcowCreateHandle(void **ppHandle,
                            const char *pFormat,
                            uint8_t Debug);
static int QcowDestroyHandle(void **ppHandle);
static int QcowOpen(void *pHandle,
                    const char **ppFilenameArr,
                    uint64_t FilenameArrLen);
static int QcowClose(void *pHandle);
static int QcowSize(void *pHandle,
                    uint64_t *pSize);
static int QcowRead(void *pHandle,
                    char *pBuf,
                    off_t Seek,
                    size_t count,
                    size_t *pRead,
                    int *pErrno);
static int QcowOptionsHelp(const char **ppHelp);
static int QcowOptionsParse(void *pHandle,
                            uint32_t OptionsCount,
                            const pts_LibXmountOptions *ppOptions,
                            const char **ppError);
static int QcowGetInfofileContent(void *pHandle,
                                  const char **ppInfoBuf);
static const char* QcowGetErrorMessage(int ErrNum);
static int QcowFreeBuffer(void *pBuf);

#endif // LIBXMOUNT_INPUT_QCOW_H

