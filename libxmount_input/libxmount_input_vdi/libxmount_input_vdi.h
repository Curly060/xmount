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

#ifndef LIBXMOUNT_INPUT_VDI_H
#define LIBXMOUNT_INPUT_VDI_H

#include <stdint.h>
#include <stdio.h>
#include "../libxmount_input.h"

/*******************************************************************************
 * References:
 * https://formats.kaitai.io/vdi/index.html
 * https://github.com/qemu/qemu/blob/master/block/vdi.c
 * https://forums.virtualbox.org/viewtopic.php?t=8046
 *******************************************************************************/

/*******************************************************************************
 * Error codes etc...
 ******************************************************************************/
enum {
   VDI_OK = 0,
   VDI_MEMALLOC_FAILED,
   VDI_FILE_OPEN_FAILED,
   VDI_CANNOT_READ_DATA,
   VDI_CANNOT_CLOSE_FILE,
   VDI_FILE_TOO_SMALL,
   VDI_BAD_MAGIC_HEADER,
   VDI_BAD_VERSION,
   VDI_UNSUPPORTED_ENCRYPTION,
   VDI_CANNOT_SEEK,
   VDI_READ_BEYOND_END_OF_IMAGE,
   VDI_CANNOT_OPEN_LOGFILE,
   VDI_OPTIONS_ERROR,
   VDI_INVALID_BLOCK_SIZE,
   VDI_BAD_BLOCK_MAP_OFFSET
};

// ----------------------
//  Constant definitions
// ----------------------

#define VDI_OPTION_LOG "vdilog"

#define GETMAX(a,b) ((a)>(b)?(a):(b))
#define GETMIN(a,b) ((a)<(b)?(a):(b))

#define VDI_HEADER_SIGNATURE   0xbeda107fU
#define VDI_HEADER_VERSION_1_1 0x00010001U

#define VDI_BLOCK_DISCARDED    0xfffffffeU
#define VDI_BLOCK_UNALLOCATED  0xffffffffU

// ---------------------
//  Types and strutures
// ---------------------
typedef struct {
   char Text[64];
   uint32_t Signature;
   uint32_t Version;
   uint32_t HeaderSize;
   uint32_t ImageType;
   uint32_t ImageFlags;
   char Description[256];
   uint32_t OffsetBmap;
   uint32_t OffsetData;
   //Start Geometry
   uint32_t Cylinders;
   uint32_t Heads;
   uint32_t Sectors;
   uint32_t SectorSize;
   //End Geometry
   uint32_t Unused1;
   uint64_t DiskSize;
   uint32_t BlockSize;
   uint32_t BlockExtra;
   uint32_t BlocksInImage;
   uint32_t BlocksAllocated;
   char UUIDImage[16];
   char UUIDLastSnap[16];
   char UUIDLink[16];
   char UUIDParent[16];
} t_VdiHeader, *t_pVdiHeader;

typedef struct {
   char *pFilename;
   FILE *pFile;
   uint64_t FileSize;
   t_VdiHeader Header;
   uint32_t *Bmap; //Contains Block Map
   char* pLogPath;
   uint8_t LogStdout;
   uint64_t CachedBlockIndex;
   uint64_t CachedSeekOffset;
   uint64_t CachedSize;
   uint8_t* Cache;
} t_Vdi, *t_pVdi;

// ----------------
//  Error handling
// ----------------

#ifdef VDI_DEBUG
#define CHK(ChkVal)    \
   {                                                                  \
      int ChkValRc;                                                   \
      if ((ChkValRc=(ChkVal)) != VDI_OK)                               \
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
      if ((ChkValRc=(ChkVal)) != VDI_OK)     \
         return ChkValRc;                   \
   }
#define DEBUG_PRINTF(...)
#endif


/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int VdiCreateHandle(void **ppHandle,
                           const char *pFormat,
                           uint8_t Debug);
static int VdiDestroyHandle(void **ppHandle);
static int VdiOpen(void *pHandle,
                   const char **ppFilenameArr,
                   uint64_t FilenameArrLen);
static int VdiClose(void *pHandle);
static int VdiSize(void *pHandle,
                   uint64_t *pSize);
static int VdiRead(void *pHandle,
                   char *pBuffer,
                   off_t Seek,
                   size_t Count,
                   size_t *pRead,
                   int *pErrno);
static int VdiOptionsHelp(const char **ppHelp);
static int VdiOptionsParse(void *pHandle,
                           uint32_t OptionsCount,
                           const pts_LibXmountOptions *ppOptions,
                           const char **ppError);
static int VdiGetInfofileContent(void *pHandle,
                                 const char **ppInfoBuffer);
static const char* VdiGetErrorMessage(int ErrNum);
static int VdiFreeBuffer(void *pBuffer);

#endif // LIBXMOUNT_INPUT_VDI_H

