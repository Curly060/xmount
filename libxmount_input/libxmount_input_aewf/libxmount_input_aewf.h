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

// Please don't touch source code formatting!

#ifndef AEWF_H
#define AEWF_H

typedef struct _t_Aewf       *t_pAewf;
typedef struct _t_Aewf const *t_pcAewf;

// ----------------------
//  Constant definitions
// ----------------------

#define GETMAX(a,b) ((a)>(b)?(a):(b))
#define GETMIN(a,b) ((a)<(b)?(a):(b))

#define FALSE 0
#define TRUE  1

// ---------------------
//  Types and strutures
// ---------------------

typedef struct
{
   unsigned char      Signature[8];
   unsigned char      StartOfFields; // 0x01;
   uint16_t           SegmentNumber;
   uint16_t           EndOfFields;   // 0x0000
} __attribute__ ((packed)) t_AewfFileHeader, *t_AewfpFileHeader;

static const unsigned char AEWF_SIGNATURE[8]  = {0x45, 0x56, 0x46, 0x09, 0x0D, 0x0A, 0xFF, 0x00};


typedef struct
{
   unsigned char      Type[16];
   uint64_t           OffsetNextSection;
   uint64_t           Size;    // Whole section size including this header (t_AewfSection) plus Data (which might be t_AewfSectionVolume, t_AewfSectionTable, ...)
   unsigned char      Padding[40];
   uint32_t           Checksum;
   char               Data[];  //lint !e1501 data member has zero size
} __attribute__ ((packed)) t_AewfSection, *t_pAewfSection;

typedef struct
{
   unsigned char      MediaType;
   unsigned char      Unknown1[3];  // contains 0x00
   uint32_t           ChunkCount;
   uint32_t           SectorsPerChunk;
   uint32_t           BytesPerSector;
   uint64_t           SectorCount;
   uint32_t           CHS_Cylinders;
   uint32_t           CHS_Heads;
   uint32_t           CHS_Sectors;
   unsigned char      MediaFlags;
   unsigned char      Unknown2[3];  // contains 0x00
   uint32_t           PalmVolumeStartSector;
   unsigned char      Padding1[4];  // contains 0x00
   uint32_t           SmartLogsStartSector;
   unsigned char      CompressionLevel;
   unsigned char      Unknown3[3];  // contains 0x00
   uint32_t           ErrorBlockSize;
   unsigned char      Unknown4[4];
   unsigned char      AcquirySystemGUID[16];
   unsigned char      Padding2[963];
   unsigned char      Reserved [5];
   uint32_t           Checksum;
} __attribute__ ((packed)) t_AewfSectionVolume, *t_pAewfSectionVolume;

typedef struct
{
   uint32_t           ChunkCount;
   unsigned char      Padding1 [4];
   uint64_t           TableBaseOffset;
   unsigned char      Padding2 [4];
   uint32_t           Checksum1;
   uint32_t           OffsetArray[0];
   uint32_t           Checksum2;      // Checksum of OffsetArray
} __attribute__ ((packed)) t_AewfSectionTable, *t_pAewfSectionTable;

const uint32_t      AEWF_COMPRESSED = 0x80000000;

typedef struct
{
   uint32_t     FirstSector;
   uint32_t     NumberOfSectors;
} __attribute__ ((packed)) t_AewfSectionErrorEntry, *t_pAewfSectionErrorEntry;

typedef struct
{
   uint32_t                NumberOfErrors;
   unsigned char           Padding[512];
   uint32_t                Checksum;
   t_AewfSectionErrorEntry ErrorArr[0];  //lint !e1501 data member 'ErrorArr' has zero size
   uint32_t                ChecksumArr;
} __attribute__ ((packed)) t_AewfSectionError, *t_pAewfSectionError;

typedef struct
{
   unsigned char MD5[16];
   unsigned char Unknown[16];
   uint32_t      Checksum;
} __attribute__ ((packed)) t_AewfSectionHash, *t_pAewfSectionHash;


typedef struct
{
   char     *pName;
   uint16_t   Number;       // Same type as t_AewfFileHeader.SegmentNumber
   FILE     *pFile;         // NULL if file is not opened (never read or kicked out from cache)
   uint64_t   FileSize;
   time_t     LastUsed;
} t_Segment, *t_pSegment;

typedef struct
{
   uint64_t             Nr;                 // The table's position in the pAewf->pTableArr, for debug output only
   uint64_t             ChunkFrom;          // Number of the chunk referred to by the first entry of this table (very first chunk has number 0)
   uint64_t             ChunkTo;            // Number of the chunk referred to by the last entry of this table
   t_pSegment          pSegment;            // The file segment where the table is located
   uint64_t             Offset;             // The offset of the table inside the segment file (start of t_AewfSectionTable, not of the preceding t_AewfSection)
   uint64_t             Size;               // The length of the table (allocated length for pEwfTable)
   uint32_t             ChunkCount;         // The number of chunks; this is the same as pEwfTable->Chunkcount, however, pEwfTable might not be available (NULL)
   uint64_t             SectionSectorsPos;  // Seek position of corresponding section SECTORS in segment file and its length. Silly EWF format has no clean way
   uint32_t             SectionSectorsSize; // of determining size of the last (possibly compressed) chunk of a table, that's why we need to memorise these values.
   time_t               LastUsed;           // Last usage of this table, for cache management
   t_pAewfSectionTable pEwfTable;           // Contains the original EWF table section or NULL, if never read or kicked out from cache
} t_Table, *t_pTable;

#define AEWF_NONE UINT64_MAX

enum
{
   READSIZE_32K = 0,
   READSIZE_64K,
   READSIZE_128K,
   READSIZE_256K,
   READSIZE_512K,
   READSIZE_1M,
   READSIZE_ABOVE_1M,
   READSIZE_ARRLEN
};

typedef enum
{
   AEWF_IDLE = 0,
   AEWF_LAUNCHED
} t_AewfThreadState;

typedef struct _t_AewfThread
{
   t_AewfThreadState  State;
   t_pcAewf          pAewf; // Give the threads access to some Aewf constants - make sure the threads only have read access
   pthread_t          ID;
   char             *pChunkBuffCompressed;
   uint64_t           ChunkBuffCompressedDataLen;
   char             *pChunkBuffUncompressed;         // This buffer serves as cache as well. ChunkInBuff contains the absolute chunk number whose data is stored here
   uint64_t           ChunkBuffUncompressedDataLen;  // This normally always is equal to the chunk size (32K), except maybe for the last chunk, if the image's total size is not a multiple of the chunk size
   uint64_t           ChunkInBuff;

   char              *pBuf;        // Job arguments to the thread: Copy the uncompressed
   uint64_t            Ofs;        // chunk data starting at chunk offset Ofs to pBuf, Len
   uint64_t            Len;        // bytes in total.

   int                ReturnCode;
} t_AewfThread, *t_pAewfThread;

#define AEWF_MAGIC 0x4d595f5f41455746   // MY__AEWF

typedef struct _t_Aewf
{
   uint64_t       Magic;
   int            Open;            // Memorize if AewfOpen has been called
   t_pSegment    pSegmentArr;      // Array of all segment files (in correct order)
   t_pTable      pTableArr;        // Array of all chunk offset tables found in the segment files (in correct order)
   uint64_t       Segments;
   uint64_t       Tables;
   uint64_t       Chunks;          // Total number of chunks in all tables
   uint64_t       TotalTableSize;  // Total size of all tables
   uint64_t       TableCache;      // Current amount RAM used by tables, in bytes
   uint64_t       OpenSegments;    // Current number of open segment files
   uint64_t       SectorSize;
   uint64_t       Sectors;
   uint64_t       ChunkSize;
   uint64_t       ImageSize;       // Equals to Sectors * SectorSize
   char         *pChunkBuffCompressed;
   char         *pChunkBuffUncompressed;
   uint64_t       ChunkBuffUncompressedDataLen;  // This normally always is equal to the chunk size (32K), except maybe for the last chunk, if the image's total size is not a multiple of the chunk size
   uint32_t       ChunkBuffSize;
   uint64_t       ChunkInBuff;     // Chunk currently residing in pChunkBuffUncompressed (AEWF_NONE if none)
   char         *pErrorText;       // Used for assembling error text during option parsing
   time_t         LastStatsUpdate;
   char         *pInfo;
   t_pAewfThread pThreadArr;

   // Statistics
   uint64_t   SegmentCacheHits;
   uint64_t   SegmentCacheMisses;
   uint64_t   TableCacheHits;
   uint64_t   TableCacheMisses;
   uint64_t   ChunkCacheHits;
   uint64_t   ChunkCacheMisses;
   uint64_t   ReadOperations;        // How many times did xmount call the function AewfRead
   uint64_t   DataReadFromImage;     // The data (in bytes) read from the image
   uint64_t   DataReadFromImageRaw;  // The same data (in bytes), after uncompression (if any)
   uint64_t   DataRequestedByCaller; // How much data was given back to the caller
   uint64_t   TablesReadFromImage;   // The overhead of the table read operations (in bytes)
   uint64_t   ChunksRead;
   uint64_t   BytesRead;
   uint64_t   ReadSizesArr[READSIZE_ARRLEN];  // Distribution of the requested block sizes to be read
   uint64_t   Errors;
   int        LastError;

   // Options
   uint64_t   MaxTableCache;    // Max. amount of bytes in pTableArr[x].pTableData, in bytes
   uint64_t   MaxOpenSegments;  // Max. number of open files in pSegmentArr
   char     *pStatsPath;        // Statistics path
   uint64_t   StatsRefresh;     // The time in seconds between update of the stats file
   char     *pLogPath;          // Path for log file
   uint8_t    LogStdout;
   uint32_t   Threads;          // Max. number of threads to be used in parallel actions. Currently only used for uncompression
} t_Aewf;


// The values below have been chosen as reasonable limits and are used
// for image consistency checks only There is no such definition of 
// limits in the EWF specification.

#define AEWF_MAX_SECTOR_SIZE        10485760
#define AEWF_MAX_CHUNK_SIZE        104857600
#define AEWF_MAX_SECTORS_PER_CHUNK     65536
#define AEWF_MAX_TABLES             10000000
#define AEWF_MAX_SECTION_COUNT      10485760  // No segment file should have that many sections
#define AEWF_MAX_HEADER_LEN          1048576

// ----------------
//    Error codes
// ----------------

// AEWF Error codes are automatically mapped to errno codes by means of the groups
// below. AEWF uses these errno codes:
//   ENOMEM    memory allocation errors
//   EINVAL    wrong parameter(s) passed to an AEWF function
//   EIO       all others: AEWF function errors, EWF image errors

// In case of errors: AewfClose should be called (if AewfOpen already was 
// called before) and also AwfDestroyHandle. In that case, expect more errors
// to occur when calling these functions.

enum
{
   AEWF_OK = 0,

   AEWF_ERROR_ENOMEM_START=1000,  // Memory allocation errors
   AEWF_MEMALLOC_FAILED,
   AEWF_ERROR_ENOMEM_END,

   AEWF_ERROR_EINVAL_START=2000,  // Wrong parameters passed
   AEWF_READ_BEYOND_END_OF_IMAGE,
   AEWF_OPTIONS_ERROR,
   AEWF_CANNOT_OPEN_LOGFILE,
   AEWF_ERROR_EINVAL_END,

   AEWF_ERROR_EIO_START=3000,     // Image errors, function errors, ...
   AEWF_MAGIC_BROKEN,
   AEWF_HANDLE_IS_NULL,
   AEWF_ALREADY_OPEN,
   AEWF_NOT_OPEN,
   AEWF_FILE_OPEN_FAILED,
   AEWF_FILE_CLOSE_FAILED,
   AEWF_FILE_SEEK_FAILED,
   AEWF_FILE_READ_FAILED,
   AEWF_READFILE_BAD_MEM,
   AEWF_BAD_FILE_SIGNATURE,
   AEWF_MISSING_SEGMENT_NUMBER,
   AEWF_DUPLICATE_SEGMENT_NUMBER,
   AEWF_WRONG_SEGMENT_FILE_COUNT,
   AEWF_VOLUME_MUST_PRECEDE_TABLES,
   AEWF_SECTORS_MUST_PRECEDE_TABLES,
   AEWF_WRONG_CHUNK_COUNT,
   AEWF_CHUNK_NOT_FOUND,
   AEWF_VOLUME_MISSING,
   AEWF_ERROR_EWF_TABLE_NOT_READY,
   AEWF_ERROR_EWF_SEGMENT_NOT_READY,
   AEWF_CHUNK_TOO_BIG,
   AEWF_UNCOMPRESS_FAILED,
   AEWF_BAD_UNCOMPRESSED_LENGTH,
   AEWF_CHUNK_CRC_ERROR,
   AEWF_ERROR_IN_CHUNK_NUMBER,
   AEWF_UNCOMPRESS_HEADER_FAILED,
   AEWF_ASPRINTF_FAILED,
   AEWF_CHUNK_LENGTH_ZERO,
   AEWF_NEGATIVE_SEEK,
   AEWF_ERROR_EIO_END,
   AEWF_ERROR_PTHREAD,
   AEWF_WRONG_CHUNK_CALCULATION,
   AEWF_ZERO_SECTORS,
   AEWF_INVALID_SECTOR_SIZE,
   AEWF_INVALID_CHUNK_SIZE,
   AEWF_ZERO_IMAGE_SIZE,
   AEWF_NUMBER_OF_TABLES,
   AEWF_SECTION_STARTPOS_ERROR,
   AEWF_TOO_MANY_SECTIONS,
   AEWF_THREADS_STILL_RUNNING,
   AEWF_FILESIZE_CHANGED,
   AEWF_SECTION_SECTORS_WRONG_SIZE,
   AEWF_SECTION_TABLE_WRONG_SIZE,
   AEWF_SECTION_TABLE_BEYOND_EOF,
   AEWF_SECTION_HEADER_WRONG_SIZE,
   AEWF_SECTION_VOLUME_WRONG_SIZE,
   AEWF_SECTION_HASH_WRONG_SIZE,
   AEWF_SECTION_BEYOND_EOF
};

#endif
