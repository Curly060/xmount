/*******************************************************************************
* xmount Copyright (c) 2024 by SITS Sarl                                       *
*                                                                              *
* Author(s):                                                                   *
*   Alain K.                                                                   *
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
#include <time.h>
#include <unistd.h>

#include "../libxmount_input.h"
#include "libxmount_input_vdi.h"

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
    return "vdi\0\0";
}

/*
 * LibXmount_Input_GetFunctions
 */
void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions *pFunctions) {
    pFunctions->CreateHandle = &VdiCreateHandle;
    pFunctions->DestroyHandle = &VdiDestroyHandle;
    pFunctions->Open = &VdiOpen;
    pFunctions->Close = &VdiClose;
    pFunctions->Size = &VdiSize;
    pFunctions->Read = &VdiRead;
    pFunctions->OptionsHelp = &VdiOptionsHelp;
    pFunctions->OptionsParse = &VdiOptionsParse;
    pFunctions->GetInfofileContent = &VdiGetInfofileContent;
    pFunctions->GetErrorMessage = &VdiGetErrorMessage;
    pFunctions->FreeBuffer = &VdiFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/

// ---------------------------
//  Internal static functions
// ---------------------------


/*
 * VdiUtilFileSeek
 */
static int VdiUtilFileSeek(t_pVdi pVdi, size_t Offset) {
    if (fseek(pVdi->pFile, Offset, SEEK_SET)) {
        return VDI_CANNOT_SEEK;
    }
    return VDI_OK;
}

/*
 * VdiUtilFileRead
 */
static int VdiUtilFileRead(t_pVdi pVdi, void* pBuf, size_t Size) {
    if (fread(pBuf, Size, 1, pVdi->pFile) != 1) {
        return VDI_CANNOT_READ_DATA;
    }
    return VDI_OK;
}

/*
 * VdiUtilMalloc
 */
static int VdiUtilMalloc(void** pBuf, size_t Size) {
    *pBuf = malloc(Size);
    if (*pBuf == NULL)
        return VDI_MEMALLOC_FAILED;
    return VDI_OK;
}

#define LOG_HEADER_LEN 80

/*
 * LogvEntry
 */
int LogvEntry(const char *pLogPath, uint8_t LogStdout, const char *pFileName,
              const char *pFunctionName, int LineNr, const char *pFormat,
              va_list pArguments)
{
    time_t       NowT;
    struct tm  *pNowTM;
    FILE       *pFile;
    int          wr;
    char       *pFullLogFileName = NULL;
    const char *pBase;
    char         LogLineHeader[1024];
    pid_t        OwnPID;
    va_list     pArguments0;

    if (!LogStdout && (pLogPath == NULL))
        return VDI_OK;

    time(&NowT);
    pNowTM = localtime(&NowT);
    OwnPID = getpid();  // pthread_self()
    wr  = (int) strftime(&LogLineHeader[0], sizeof(LogLineHeader),
                         "%a %d.%b.%Y %H:%M:%S ", pNowTM);
    wr += snprintf(&LogLineHeader[wr], sizeof(LogLineHeader) - wr,
                   "%5d ", OwnPID);

    if (pFileName && pFunctionName)
    {
        pBase = strrchr(pFileName, '/');
        if (pBase)
            pFileName = pBase + 1;
        wr += snprintf(&LogLineHeader[wr], sizeof(LogLineHeader) - wr,
                       "%s %s %d ", pFileName, pFunctionName, LineNr);
    }

    if (pLogPath)
    {
        wr = asprintf(&pFullLogFileName, "%s/log_%d", pLogPath, OwnPID);
        if ((wr <= 0) || (pFullLogFileName == NULL))
        {
            if (LogStdout)
                printf("\nLog file error: Can't build filename");
            return VDI_MEMALLOC_FAILED;
        }
        else
        {
            pFile = fopen64(pFullLogFileName, "a");
            if (pFile == NULL)
            {
                if (LogStdout)
                    printf("\nLog file error: Can't be opened");
                return VDI_CANNOT_OPEN_LOGFILE;
            }
            else
            {
                fprintf  (pFile, "%-*s", LOG_HEADER_LEN, &LogLineHeader[0]);
                va_copy(pArguments0, pArguments);
                vfprintf(pFile, pFormat, pArguments0);
                fprintf  (pFile, "\n");
                fclose   (pFile);
            }
            free(pFullLogFileName);
        }
    }
    if (LogStdout)
    {
        printf  ("%s", &LogLineHeader[0]);
        va_copy(pArguments0, pArguments);
        vprintf(pFormat, pArguments0);
        printf  ("\n");
    }
    return VDI_OK;
}

/*
 * LogEntry
 */
int LogEntry(const char *pLogPath, uint8_t LogStdout, const char *pFileName,
             const char *pFunctionName, int LineNr, const char *pFormat, ...)
{
    va_list VaList;
    int     rc;

    if (!LogStdout && (pLogPath == NULL))
        return VDI_OK;

    va_start(VaList, pFormat); //lint !e530 Symbol 'VaList' not initialized
    rc = LogvEntry(pLogPath, LogStdout, pFileName, pFunctionName, LineNr,
                   pFormat, VaList);
    va_end(VaList);
    return rc;
}

#define LOG(...) \
   LogEntry(pVdi->pLogPath, pVdi->LogStdout, __FILE__, __FUNCTION__, \
    __LINE__, __VA_ARGS__);

/*
 * VdiRawRead0
 */
static int VdiRead0(t_pVdi pVdi, char *pBuffer, uint64_t Seek, uint32_t *pCount)
{
    uint64_t SeekBlock  = Seek / pVdi->Header.BlockSize;
    uint64_t SeekOffset = Seek % pVdi->Header.BlockSize;
    if(SeekBlock >= pVdi->Header.BlocksInImage) {
        return VDI_BAD_BLOCK_MAP_OFFSET;
    }
    uint64_t FileBlock = pVdi->Bmap[SeekBlock];
    *pCount = GETMIN(*pCount, pVdi->Header.BlockSize - SeekOffset);

    if (FileBlock == VDI_BLOCK_DISCARDED || FileBlock == VDI_BLOCK_UNALLOCATED) {
        memset(pBuffer, 0, *pCount);
        LOG("NULL BLOCK");
        return VDI_OK;
    }

    uint64_t FilePosition = pVdi->Header.OffsetData +
                            (FileBlock * pVdi->Header.BlockSize) + SeekOffset;

    CHK(VdiUtilFileSeek(pVdi, FilePosition))
    CHK(VdiUtilFileRead(pVdi, pBuffer, *pCount))

    return VDI_OK;
}

/*
 * VdiParseHeader
 */
static int VdiParseHeader(t_pVdi pVdi) {
    t_pVdiHeader pHeader =  &(pVdi->Header);
    CHK(VdiUtilFileRead(pVdi, pHeader, sizeof(t_VdiHeader)))
    if (pHeader->Signature != VDI_HEADER_SIGNATURE) {
        return VDI_BAD_MAGIC_HEADER;
    }
    if (pHeader->Version != VDI_HEADER_VERSION_1_1) {
        return VDI_BAD_VERSION;
    }
    if(pHeader->BlockSize == 0) {
        return VDI_INVALID_BLOCK_SIZE;
    }

    return VDI_OK;
}

// ---------------
//  API functions
// ---------------

/*
 * VdiInit
 */
static int VdiInit(void **pp_init_handle)
{
    *pp_init_handle = NULL;

    return VDI_OK;
}

/*
 * VdiDeInit
 */
static int VdiDeInit(void **pp_init_handle)
{
    return VDI_OK;
}

/*
 * VdiCreateHandle
 */
static int VdiCreateHandle(void **ppHandle,
                           void *p_init_handle,
                           const char *pFormat,
                           uint8_t Debug)
{
    (void)pFormat;
    t_pVdi pVdi = NULL;

    CHK(VdiUtilMalloc((void**)&pVdi, sizeof(t_Vdi)))
    memset(pVdi, 0, sizeof(t_Vdi));
    pVdi->LogStdout = Debug;
    *ppHandle = pVdi;
    return VDI_OK;
}

/*
 * VdiDestroyHandle
 */
static int VdiDestroyHandle(void **ppHandle) {
    t_pVdi* ppVdi = (t_pVdi*)ppHandle;
    if (*ppVdi != NULL) {
        if((*ppVdi)->Bmap != NULL) {
            free((*ppVdi)->Bmap);
            (*ppVdi)->Bmap = NULL;
        }
        free(*ppVdi);
        *ppVdi = NULL;
    }
    return VDI_OK;
}

/*
 * VdiOpen
 */
static int VdiOpen(void *pHandle,
                   const char **ppFilenameArr,
                   uint64_t FilenameArrLen)
{
    t_pVdi pVdi = (t_pVdi)pHandle;
    if (FilenameArrLen == 0) {
        return VDI_FILE_OPEN_FAILED;
    }
    pVdi->pFilename = strdup(ppFilenameArr[0]);
    pVdi->pFile = fopen(pVdi->pFilename, "r");
    if (pVdi->pFile == NULL) {
        VdiClose(pHandle);
        return VDI_FILE_OPEN_FAILED;
    }

    //Parse Vdi Header
    CHK(VdiParseHeader(pVdi))

    //Read Block Map
    uint64_t BmapSize = pVdi->Header.BlocksInImage * sizeof(uint32_t);
    CHK(VdiUtilMalloc((void**) & (pVdi->Bmap), BmapSize))
    CHK(VdiUtilFileSeek(pVdi, pVdi->Header.OffsetBmap))
    CHK(VdiUtilFileRead(pVdi, pVdi->Bmap, BmapSize))

    return VDI_OK;
}

/*
 * VdiClose
 */
static int VdiClose(void *pHandle) {
    t_pVdi pVdi = (t_pVdi)pHandle;
    if (pVdi != NULL && pVdi->Bmap != NULL) {
        free(pVdi->Bmap);
        pVdi->Bmap = NULL;
    }

    return VDI_OK;
}

/*
 * VdiSize
 */
static int VdiSize(void *pHandle, uint64_t *pSize) {
    t_pVdi pVdi = (t_pVdi)pHandle;
    *pSize = pVdi->Header.DiskSize;
    return VDI_OK;
}

/*
 * VdiRead
 */
static int VdiRead(void *pHandle,
                   char *pBuffer,
                   off_t Seek,
                   size_t Count,
                   size_t *pRead,
                   int *pErrno)
{
    t_pVdi pVdi = (t_pVdi)pHandle;
    uint32_t Remaining = Count;
    uint32_t ToRead;

    LOG("Reading %"PRIu64" from offset %"PRIu64, Count, Seek)
    if ((Seek + Count) > pVdi->Header.DiskSize) {
        return VDI_READ_BEYOND_END_OF_IMAGE;
    }
    do {
        ToRead = Remaining;
        CHK(VdiRead0(pVdi, pBuffer, Seek, &ToRead))
        Remaining -= ToRead;
        pBuffer += ToRead;
        Seek += ToRead;
    } while (Remaining);

    *pRead = Count;

    return VDI_OK;
}

/*
 * VdiOptionsHelp
 */
static int VdiOptionsHelp(const char **ppHelp) {
    char *pHelp = NULL;
    int    wr;

    wr = asprintf(&pHelp, "    %-12s : Path for writing log file(must exist).\n"
                  "                   The files created in this directory will be named log_<pid>.\n",
                  VDI_OPTION_LOG);
    if ((pHelp == NULL) || (wr <= 0))
        return VDI_MEMALLOC_FAILED;

    *ppHelp = pHelp;
    return VDI_OK;
}

/*
 * VdiOptionsParse
 */
static int VdiOptionsParse(void *pHandle,
                           uint32_t OptionCount,
                           const pts_LibXmountOptions *ppOptions,
                           const char **ppError)
{
    pts_LibXmountOptions pOption;
    t_pVdi              pVdi  = (t_pVdi) pHandle;
    const char         *pError = NULL;
    int                   rc    = VDI_OK;

    LOG("Called - OptionCount=%" PRIu32, OptionCount);
    *ppError = NULL;

    for (uint32_t i = 0; i < OptionCount; i++)
    {
        pOption = ppOptions[i];
        if (strcmp(pOption->p_key, VDI_OPTION_LOG) == 0)
        {
            pVdi->pLogPath = realpath(pOption->p_value, NULL);
            if (pVdi->pLogPath == NULL)
            {
                pError = "The given log path does not exist";
                LOG("Log path %s not found", pOption->p_value);
                break;
            }
            rc = LOG("Logging for libxmount_input_vdi started")
                 if (rc != VDI_OK)
            {
                pError = "Write test to log file failed";
                break;
            }
            pOption->valid = 1;
            LOG("Option %s set to %s(full path %s)", VDI_OPTION_LOG,
                pOption->p_value, pVdi->pLogPath);
        }
    }
    if (pError)
    {
        *ppError = strdup(pError);
        rc = VDI_OPTIONS_ERROR;
    }
    LOG("Ret - rc=%d, error=%s", rc, *ppError);
    return rc;
}

/*
 * vdiGetInfofileContent
 */
static int VdiGetInfofileContent(void *pHandle, const char **ppInfoBuffer) {
    t_pVdi pVdi = (t_pVdi)pHandle;
    int ret;
    char *pInfoBuf;

    ret = asprintf(&pInfoBuf,
                   "VDI image assembled of %" PRIu64 " bytes in total(%0.3f GiB)\n",
                   pVdi->FileSize,
                   pVdi->FileSize / (1024.0 * 1024.0 * 1024.0));
    if (ret < 0 || *ppInfoBuffer == NULL) return VDI_MEMALLOC_FAILED;

    *ppInfoBuffer = pInfoBuf;
    return VDI_OK;
}

/*
 * VdiGetErrorMessage
 */
static const char* VdiGetErrorMessage(int ErrNum) {
    switch (ErrNum) {
    case VDI_MEMALLOC_FAILED:
        return "Unable to allocate memory";
        break;
    case VDI_FILE_OPEN_FAILED:
        return "Unable to open vdi file";
        break;
    case VDI_CANNOT_READ_DATA:
        return "Unable to read vdi data";
        break;
    case VDI_CANNOT_CLOSE_FILE:
        return "Unable to close vdi file";
        break;
    case VDI_BAD_MAGIC_HEADER:
        return "Unable to verify magic header of vdi file";
        break;
    case VDI_BAD_VERSION:
        return "Unsupported vdi file version. Only v 1.1 is supported.";
        break;
    case VDI_CANNOT_SEEK:
        return "Unable to seek into vdi data";
        break;
    case VDI_READ_BEYOND_END_OF_IMAGE:
        return "Unable to read vdi data: Attempt to read past EOF";
        break;
    case VDI_UNSUPPORTED_ENCRYPTION:
        return "Encrpyted vdi format is not supported";
        break;
    case VDI_INVALID_BLOCK_SIZE:
        return "Header contained invalid block size";
        break;
    case VDI_BAD_BLOCK_MAP_OFFSET:
        return "Got an invalid Block Map index";
        break;
    default:
        return "Unknown error";
    }
}

/*
 * VdiFreeBuffer
 */
static int VdiFreeBuffer(void *pBuffer) {
    free(pBuffer);
    return VDI_OK;
}
