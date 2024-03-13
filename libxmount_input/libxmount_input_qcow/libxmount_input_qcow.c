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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <zlib.h>

#include "../libxmount_input.h"
#include "libxmount_input_qcow.h"

#define LOG_WARNING(...) {            \
  LIBXMOUNT_LOG_WARNING(__VA_ARGS__); \
}

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
    return "qcow\0qcow2\0qemu\0\0";
}

/*
 * LibXmount_Input_GetFunctions
 */
void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions *p_functions) {
    p_functions->CreateHandle = &QcowCreateHandle;
    p_functions->DestroyHandle = &QcowDestroyHandle;
    p_functions->Open = &QcowOpen;
    p_functions->Close = &QcowClose;
    p_functions->Size = &QcowSize;
    p_functions->Read = &QcowRead;
    p_functions->OptionsHelp = &QcowOptionsHelp;
    p_functions->OptionsParse = &QcowOptionsParse;
    p_functions->GetInfofileContent = &QcowGetInfofileContent;
    p_functions->GetErrorMessage = &QcowGetErrorMessage;
    p_functions->FreeBuffer = &QcowFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/

// ---------------------------
//  Internal static functions
// ---------------------------


static uint64_t QcowClusterOffsetFromAddress(t_pQcow pQcow, uint64_t Address) {
    return Address & (pQcow->ClusterSize - 1);
}

static uint64_t QcowL2OffsetFromAddress(t_pQcow pQcow, uint64_t Address) {
    return (Address >> pQcow->Header.ClusterBits) & (pQcow->L2Size - 1);
}

static uint64_t QcowL1OffsetFromAddress(t_pQcow pQcow, uint64_t Address) {
    return (Address >> (pQcow->Header.ClusterBits + pQcow->L2Bits));
}

static int QcowUtilFileSeek(t_pQcow pQcow, size_t offset) {
    if (fseek(pQcow->pFile, offset, SEEK_SET)) {
        return QCOW_CANNOT_SEEK;
    }
    return QCOW_OK;
}

static int QcowUtilFileRead(t_pQcow pQcow, void* Ptr, size_t Size) {
    if (fread(Ptr, Size, 1, pQcow->pFile) != 1) {
        return QCOW_CANNOT_READ_DATA;
    }
    return QCOW_OK;
}

static void QcowUtilLog(char* format, ...) {
    printf("[QCOWLOG] ");
    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
    printf("\n");
}

/*
 * QcowParseHeader
 */
static int QcowParseHeader(t_pQcow pQcow) {
    t_pQcowHeader pHeader =  &(pQcow->Header);
    CHK(QcowUtilFileRead(pQcow, pHeader, sizeof(t_QcowHeader)))
    if (memcmp(&(pHeader->Magic), "QFI\xfb", 4) != 0) {
        return QCOW_BAD_MAGIC_HEADER;
    }

    //Fix endianess of header fields
    pHeader->Version = be32toh(pHeader->Version);
    pHeader->BackingFileOffset = be64toh(pHeader->BackingFileOffset);
    pHeader->BackingFileSize = be64toh(pHeader->BackingFileSize);
    pHeader->ClusterBits = be32toh(pHeader->ClusterBits);
    pHeader->Size = be64toh(pHeader->Size);
    pHeader->CryptMethod = be32toh(pHeader->CryptMethod);
    pHeader->L1Size = be32toh(pHeader->L1Size);
    pHeader->L1TableOffset = be64toh(pHeader->L1TableOffset);
    pHeader->RefCountTableOffset = be64toh(pHeader->RefCountTableOffset);
    pHeader->RefCountTableClusters = be32toh(pHeader->RefCountTableClusters);
    pHeader->NbSnapshots = be32toh(pHeader->NbSnapshots);
    pHeader->SnapshotsOffset = be64toh(pHeader->SnapshotsOffset);

    // Check if unsupported features are used
    // TODO Check if v3 features are used that are not supported
    if (pHeader->Version != 2 && pHeader->Version != 3) { 
        return QCOW_BAD_VERSION;
    }
    if (pHeader->CryptMethod != 0) {
        return QCOW_UNSUPPORTED_ENCRYPTION;
    }

    return QCOW_OK;
}

/*
 * QcowRawRead0
 */
static int QcowRead0(t_pQcow pQcow, char *pBuffer, uint64_t Seek, uint32_t *pCount)
{
    uint64_t L1Offset;
    uint64_t L2Offset;
    uint64_t ClusterOffset;
    uint64_t ClusterBaseAddress;
    uint64_t L2TableAddress;
    uint64_t DataAddress;
    int ClusterIsCompressed = 0;
    uint64_t CompressedClusterSize = 0;

    L1Offset = QcowL1OffsetFromAddress(pQcow, Seek);
    L2Offset = QcowL2OffsetFromAddress(pQcow, Seek);
    ClusterOffset = QcowClusterOffsetFromAddress(pQcow, Seek);
    *pCount = GETMIN (*pCount, pQcow->ClusterSize - ClusterOffset);

    // Some boundary checks
    if (L1Offset >= pQcow->Header.L1Size) {
        return QCOW_BAD_L1_OFFSET;
    }

    //Bottom 9 bits are reserved, Top byte too
    L2TableAddress = be64toh(pQcow->pL1Table[L1Offset]) & UINT64_C(0x00fffffffffffe00);

    if (L2TableAddress == 0) {
        ClusterBaseAddress = 0;
    } else {
        CHK(QcowUtilFileSeek(pQcow, L2TableAddress + L2Offset * sizeof(uint64_t)))
        CHK(QcowUtilFileRead(pQcow, &ClusterBaseAddress, sizeof(uint64_t)))
        ClusterBaseAddress = be64toh(ClusterBaseAddress);
        ClusterIsCompressed = (ClusterBaseAddress >> 62) & 1;
        if (!ClusterIsCompressed)
            ClusterBaseAddress = ClusterBaseAddress & UINT64_C(0x00fffffffffffe00);
    }
    if (ClusterBaseAddress == 0 ) {
        memset(pBuffer, '\0', *pCount);
        return QCOW_OK;
        return QCOW_CANNOT_READ_DATA;
    }
    if (ClusterIsCompressed) {
        uint64_t AddressBits = 64 - 2 - (pQcow->Header.ClusterBits - 8);
        CompressedClusterSize = 512 * (1 + ((ClusterBaseAddress >> AddressBits) & (((size_t)1 << (pQcow->Header.ClusterBits - 8)) - 1)));
        ClusterBaseAddress = ClusterBaseAddress & (((size_t)1 << AddressBits) - 1);
        char* p_compressed_buffer = malloc(CompressedClusterSize);
        if(p_compressed_buffer == NULL) {
            return QCOW_MEMALLOC_FAILED;
        }
        char* p_uncompressed_buffer = malloc(pQcow->ClusterSize);
        if(p_uncompressed_buffer == NULL) {
            return QCOW_MEMALLOC_FAILED;
        }
        CHK(QcowUtilFileSeek(pQcow, ClusterBaseAddress))
        CHK(QcowUtilFileRead(pQcow, p_compressed_buffer, CompressedClusterSize))
        z_stream zlib_stream;
        memset(&zlib_stream, 0, sizeof( z_stream ) );
        zlib_stream.next_in   = p_compressed_buffer;
        zlib_stream.avail_in  = CompressedClusterSize;
        zlib_stream.next_out  = p_uncompressed_buffer;
        zlib_stream.avail_out = pQcow->ClusterSize;
        int r = inflateInit2(&zlib_stream, -12);
        if (r) {
            free(p_compressed_buffer);
            free(p_uncompressed_buffer);
            return QCOW_UNABLE_TO_DECOMPRESS_CLUSTER;
        }
        r = inflate(&zlib_stream, Z_FINISH);
        if (r < 0) {
            free(p_compressed_buffer);
            free(p_uncompressed_buffer);
            return QCOW_UNABLE_TO_DECOMPRESS_CLUSTER;
        }
        memcpy(pBuffer, p_uncompressed_buffer + ClusterOffset, *pCount);
        free(p_compressed_buffer);
        free(p_uncompressed_buffer);
        return QCOW_OK;
    } else {
        DataAddress = ClusterBaseAddress + ClusterOffset;
        CHK(QcowUtilFileSeek(pQcow, DataAddress))
        CHK(QcowUtilFileRead(pQcow, pBuffer, *pCount))
        return QCOW_OK;
    }
}

/*
 * QcowCreateHandle
 */
static int QcowCreateHandle(void **ppHandle,
                            const char *pFormat,
                            uint8_t Debug)
{
    (void)pFormat;
    t_pQcow pQcow = NULL;

    pQcow = (t_pQcow)malloc(sizeof(t_Qcow));
    if (pQcow == NULL) return QCOW_MEMALLOC_FAILED;

    memset(pQcow, 0, sizeof(t_Qcow));
    *ppHandle = pQcow;
    return QCOW_OK;
}

/*
 * QcowDestroyHandle
 */
static int QcowDestroyHandle(void **ppHandle) {
    free(*ppHandle);
    *ppHandle = NULL;
    return QCOW_OK;
}

/*
 * QcowOpen
 */
static int QcowOpen(void *pHandle,
                    const char **ppFilenameArr,
                    uint64_t FilenameArrLen)
{
    t_pQcow pQcow = (t_pQcow)pHandle;
    if (FilenameArrLen == 0) {
        return QCOW_FILE_OPEN_FAILED;
    }
    pQcow->pFilename = strdup(ppFilenameArr[0]);
    pQcow->pFile = fopen (pQcow->pFilename, "r");
    if (pQcow->pFile == NULL) {
        QcowClose(pHandle);
        return QCOW_FILE_OPEN_FAILED;
    }

    //Parse Qcow Header
    CHK(QcowParseHeader(pQcow))

    pQcow->L2Bits = pQcow->Header.ClusterBits - 3;
    pQcow->L2Size = (size_t)1 << pQcow->L2Bits;
    pQcow->L1Bits = 64 - pQcow->L2Bits - pQcow->Header.ClusterBits;
    pQcow->ClusterSize = (size_t)1 << pQcow->Header.ClusterBits;

    //Cache L1 Table
    pQcow->pL1Table = malloc(pQcow->Header.L1Size * sizeof(uint64_t));
    if (pQcow->pL1Table == NULL) {
        QcowClose(pHandle);
        return QCOW_MEMALLOC_FAILED;
    }

    CHK(QcowUtilFileSeek(pQcow, pQcow->Header.L1TableOffset))
    CHK(QcowUtilFileRead(pQcow, pQcow->pL1Table,  pQcow->Header.L1Size * sizeof(uint64_t)))

    return QCOW_OK;
}

/*
 * QcowClose
 */
static int QcowClose(void *pHandle) {
    t_pQcow    pQcow = (t_pQcow)pHandle;
    if (pQcow->pFilename) {
        free (pQcow->pFilename); 
        pQcow->pFilename = NULL;
    }
    if (pQcow->pL1Table) {
        free(pQcow->pL1Table);
        pQcow->pL1Table = NULL;
    }
    if (pQcow->pFile) {
        if (fclose (pQcow->pFile)) return QCOW_CANNOT_CLOSE_FILE;
        pQcow->pFile = NULL;
    }
    return QCOW_OK;
}

/*
 * QcowSize
 */
static int QcowSize(void *pHandle, uint64_t *pSize) {
    t_pQcow pQcow = (t_pQcow)pHandle;
    *pSize = pQcow->Header.Size;
    return QCOW_OK;
}

/*
 * QcowRead
 */
static int QcowRead(void *pHandle,
                    char *pBuf,
                    off_t Seek,
                    size_t Count,
                    size_t *pRead,
                    int *pErrno)
{

    t_pQcow pQcow = (t_pQcow)pHandle;
    uint32_t Remaining = Count;
    uint32_t ToRead;

    if ((Seek + Count) > pQcow->Header.Size) {
        return QCOW_READ_BEYOND_END_OF_IMAGE;
    }
    do {
        ToRead = Remaining;
        CHK(QcowRead0(pQcow, pBuf, Seek, &ToRead))
        Remaining -= ToRead;
        pBuf += ToRead;
        Seek += ToRead;
    } while (Remaining);

    *pRead = Count;

    return QCOW_OK;
}

/*
 * QcowOptionsHelp
 */
static int QcowOptionsHelp(const char **ppHelp) {
    *ppHelp = NULL;
    return QCOW_OK;
}

/*
 * QcowOptionsParse
 */
static int QcowOptionsParse(void *pHandle,
                            uint32_t OptionsCount,
                            const pts_LibXmountOptions *ppOptions,
                            const char **ppError)
{
    return QCOW_OK;
}

/*
 * QCowGetInfofileContent
 */
static int QcowGetInfofileContent(void *pHandle, const char **ppInfoBuf) {
    t_pQcow pQcow = (t_pQcow)pHandle;
    int ret;
    char *p_info_buf;

    ret = asprintf(&p_info_buf,
                   "QCOW image assembled of %" PRIu64 " bytes in total (%0.3f GiB)\n",
                   pQcow->FileSize,
                   pQcow->FileSize / (1024.0 * 1024.0 * 1024.0));
    if (ret < 0 || *ppInfoBuf == NULL) return QCOW_MEMALLOC_FAILED;

    *ppInfoBuf = p_info_buf;
    return QCOW_OK;
}

/*
 * QcowGetErrorMessage
 */
static const char* QcowGetErrorMessage(int ErrNum) {
    switch (ErrNum) {
    case QCOW_MEMALLOC_FAILED:
        return "Unable to allocate memory";
        break;
    case QCOW_FILE_OPEN_FAILED:
        return "Unable to open qcow file";
        break;
    case QCOW_CANNOT_READ_DATA:
        return "Unable to read qcow data";
        break;
    case QCOW_CANNOT_CLOSE_FILE:
        return "Unable to close qcow file";
        break;
    case QCOW_BAD_MAGIC_HEADER:
        return "Unable to verify magic header of qcow file";
        break;
    case QCOW_BAD_L1_OFFSET:
        return "Got an L1 Index that is bigger than the L1 table size";
        break;
    case QCOW_BAD_VERSION:
        return "Unsupported qcow file version. Only v2 is supported.";
        break;
    case QCOW_CANNOT_SEEK:
        return "Unable to seek into qcow data";
        break;
    case QCOW_UNABLE_TO_DECOMPRESS_CLUSTER:
        return "Unable to init decompresison or decompress a cluster.";
        break;
    case QCOW_READ_BEYOND_END_OF_IMAGE:
        return "Unable to read qcow data: Attempt to read past EOF";
        break;
    case QCOW_UNSUPPORTED_ENCRYPTION:
        return "Encrpyted qcow format is not supported";
        break;
    default:
        return "Unknown error";
    }
}

/*
 * QcowFreeBuffer
 */
static int QcowFreeBuffer(void *pBuf) {
    free(pBuf);
    return QCOW_OK;
}
