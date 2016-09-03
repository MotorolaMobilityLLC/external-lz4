/*
    bench.c - Demo program to benchmark open-source compression algorithms
    Copyright (C) Yann Collet 2012-2016

    GPL v2 License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    You can contact the author at :
    - LZ4 homepage : http://www.lz4.org
    - LZ4 source repository : https://github.com/Cyan4973/lz4
*/

/*-************************************
*  Compiler Options
***************************************/
#if defined(_MSC_VER) || defined(_WIN32)
#  define _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_DEPRECATE     /* VS2005 */
#endif

/* Unix Large Files support (>4GB) */
#define _FILE_OFFSET_BITS 64
#if (defined(__sun__) && (!defined(__LP64__)))   /* Sun Solaris 32-bits requires specific definitions */
#  define _LARGEFILE_SOURCE
#elif ! defined(__LP64__)                        /* No point defining Large file for 64 bit */
#  define _LARGEFILE64_SOURCE
#endif

#if defined(__MINGW32__) && !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE 1          /* disable %llu warnings with MinGW on Windows */
#endif


/*-************************************
*  Includes
***************************************/
#include <stdlib.h>     /* malloc */
#include <stdio.h>      /* fprintf, fopen */
#include <sys/types.h>  /* stat64 */
#include <sys/stat.h>   /* stat64 */
#include <time.h>       /* clock_t, clock, CLOCKS_PER_SEC */
#include <string.h>     /* strlen */


#include "lz4.h"
#define COMPRESSOR0 LZ4_compress_local
static int LZ4_compress_local(const char* src, char* dst, int srcSize, int dstSize, int clevel) { (void)clevel; return LZ4_compress_default(src, dst, srcSize, dstSize); }
#include "lz4hc.h"
#define COMPRESSOR1 LZ4_compress_HC
#define DEFAULTCOMPRESSOR COMPRESSOR0

#include "xxhash.h"


/*-************************************
*  Compiler specifics
***************************************/
#if !defined(S_ISREG)
#  define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif


/*-************************************
*  Basic Types
***************************************/
#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* C99 */
# include <stdint.h>
  typedef uint8_t  BYTE;
  typedef uint16_t U16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
#else
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
#endif


/*-************************************
*  Constants
***************************************/
#define NBLOOPS    3
#define TIMELOOP_S 1
#define TIMELOOP_CLOCK (TIMELOOP_S * CLOCKS_PER_SEC)

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define MAX_MEM             (2 GB - 64 MB)
#define DEFAULT_CHUNKSIZE   (4 MB)


/*-************************************
*  Local structures
***************************************/
struct chunkParameters
{
    U32   id;
    char* origBuffer;
    char* compressedBuffer;
    int   origSize;
    int   compressedSize;
};

struct compressionParameters
{
    int (*compressionFunction)(const char* src, char* dst, int srcSize, int dstSize, int cLevel);
    int (*decompressionFunction)(const char* src, char* dst, int dstSize);
};


/*-************************************
*  Macro
***************************************/
#define DISPLAY(...) fprintf(stderr, __VA_ARGS__)


/*-************************************
*  Benchmark Parameters
***************************************/
static int g_chunkSize = DEFAULT_CHUNKSIZE;
static int g_nbIterations = NBLOOPS;
static int BMK_pause = 0;

void BMK_setBlocksize(int bsize) { g_chunkSize = bsize; }

void BMK_setNbIterations(int nbLoops)
{
    g_nbIterations = nbLoops;
    DISPLAY("- %i iterations -\n", g_nbIterations);
}

void BMK_setPause(void) { BMK_pause = 1; }


/*-*******************************************************
*  Private functions
**********************************************************/

/** BMK_getClockSpan() :
    works even if overflow; Typical max span ~ 30 mn */
static clock_t BMK_getClockSpan (clock_t clockStart)
{
    return clock() - clockStart;
}

static size_t BMK_findMaxMem(U64 requiredMem)
{
    size_t const step = 64 MB;
    void* testmem = NULL;

    requiredMem = (((requiredMem >> 26) + 1) << 26);
    requiredMem += 2*step;
    if (requiredMem > MAX_MEM) requiredMem = MAX_MEM;

    while (!testmem) {
        if (requiredMem > step) requiredMem -= step;
        else requiredMem >>= 1;
        testmem = malloc ((size_t)requiredMem);
    }
    free (testmem);

    /* keep some space available */
    if (requiredMem > step) requiredMem -= step;
    else requiredMem >>= 1;

    return (size_t)requiredMem;
}

static U64 BMK_GetFileSize(const char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
#endif
    if (r || !S_ISREG(statbuf.st_mode)) return 0;   /* No good... */
    return (U64)statbuf.st_size;
}


/*-*******************************************************
*  Public function
**********************************************************/

int BMK_benchLevel(const char** fileNamesTable, int nbFiles, int cLevel)
{
  int fileIdx=0;
  char* orig_buff;
  struct compressionParameters compP;
  int cfunctionId;

  U64 totals = 0;
  U64 totalz = 0;
  double totalc = 0.;
  double totald = 0.;

  /* Init */
  if (cLevel < LZ4HC_MIN_CLEVEL) cfunctionId = 0; else cfunctionId = 1;
  switch (cfunctionId)
  {
#ifdef COMPRESSOR0
  case 0 : compP.compressionFunction = COMPRESSOR0; break;
#endif
#ifdef COMPRESSOR1
  case 1 : compP.compressionFunction = COMPRESSOR1; break;
#endif
  default : compP.compressionFunction = DEFAULTCOMPRESSOR;
  }

  /* Loop for each file */
  while (fileIdx<nbFiles) {
      const char* inFileName = fileNamesTable[fileIdx++];
      FILE* const inFile = fopen( inFileName, "rb" );
      U64 const inFileSize = BMK_GetFileSize(inFileName);
      size_t benchedSize = BMK_findMaxMem(inFileSize * 2) / 2;
      unsigned nbChunks;
      int maxCompressedChunkSize;
      size_t readSize;
      char* compressedBuffer;
      struct chunkParameters* chunkP;
      U32 crcOrig;

      /* Check file and memory conditions */
      if (inFile==NULL) { DISPLAY( "Pb opening %s\n", inFileName); return 11; }
      if (inFileSize==0) { DISPLAY( "file is empty\n"); fclose(inFile); return 11; }
      if (benchedSize==0) { DISPLAY( "not enough memory\n"); fclose(inFile); return 11; }
      if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
      if (benchedSize < inFileSize) {
          DISPLAY("Not enough memory for '%s' full size; testing %u MB only...\n", inFileName, (unsigned)(benchedSize>>20));
      }

      /* Allocation */
      nbChunks = (unsigned)(benchedSize / g_chunkSize) + 1;
      chunkP = (struct chunkParameters*) malloc(nbChunks * sizeof(struct chunkParameters));
      orig_buff = (char*)malloc(benchedSize);
      maxCompressedChunkSize = LZ4_compressBound(g_chunkSize);
      { size_t const compressedBuffSize = (size_t)(nbChunks * maxCompressedChunkSize);
        compressedBuffer = (char*)malloc(compressedBuffSize); }

      if (!orig_buff || !compressedBuffer){
        DISPLAY("\nError: not enough memory!\n");
        free(orig_buff);
        free(compressedBuffer);
        free(chunkP);
        fclose(inFile);
        return 12;
      }

      /* Init chunks data */
      {   unsigned i;
          size_t remaining = benchedSize;
          char* in = orig_buff;
          char* out = compressedBuffer;
          for (i=0; i<nbChunks; i++) {
              chunkP[i].id = i;
              chunkP[i].origBuffer = in; in += g_chunkSize;
              if (remaining > (size_t)g_chunkSize) { chunkP[i].origSize = g_chunkSize; remaining -= g_chunkSize; } else { chunkP[i].origSize = (int)remaining; remaining = 0; }
              chunkP[i].compressedBuffer = out; out += maxCompressedChunkSize;
              chunkP[i].compressedSize = 0;
      }   }

      /* Fill input buffer */
      DISPLAY("Loading %s...       \r", inFileName);
      if (strlen(inFileName)>16) inFileName += strlen(inFileName)-16;   /* can only display 16 characters */
      readSize = fread(orig_buff, 1, benchedSize, inFile);
      fclose(inFile);

      if (readSize != benchedSize) {
        DISPLAY("\nError: problem reading file '%s' !! \n", inFileName);
        free(orig_buff);
        free(compressedBuffer);
        free(chunkP);
        return 13;
      }

      /* Calculating input Checksum */
      crcOrig = XXH32(orig_buff, benchedSize,0);

      /* Bench */
      { int loopNb;
        size_t cSize = 0;
        double fastestC = 100000000., fastestD = 100000000.;
        double ratio = 0.;
        U32 crcCheck = 0;

        DISPLAY("\r%79s\r", "");
        for (loopNb = 1; loopNb <= g_nbIterations; loopNb++) {
          int nbLoops = 0;
          clock_t clockStart, clockEnd;
          unsigned chunkNb;

          /* Compression */
          DISPLAY("%2i#%1i-%-14.14s : %9i ->\r", cLevel, loopNb, inFileName, (int)benchedSize);
          { size_t i; for (i=0; i<benchedSize; i++) compressedBuffer[i]=(char)i; }     /* warmimg up memory */

          clockStart = clock();
          while (clock() == clockStart);
          clockStart = clock();
          while (BMK_getClockSpan(clockStart) < TIMELOOP_CLOCK) {
             for (chunkNb=0; chunkNb<nbChunks; chunkNb++)
                chunkP[chunkNb].compressedSize = compP.compressionFunction(chunkP[chunkNb].origBuffer, chunkP[chunkNb].compressedBuffer, chunkP[chunkNb].origSize, maxCompressedChunkSize, cLevel);
             nbLoops++;
          }
          clockEnd = BMK_getClockSpan(clockStart);

          nbLoops += !nbLoops;   /* avoid division by zero */
          if ((double)clockEnd < fastestC*nbLoops) fastestC = (double)clockEnd/nbLoops;
          cSize=0; for (chunkNb=0; chunkNb<nbChunks; chunkNb++) cSize += chunkP[chunkNb].compressedSize;
          ratio = (double)cSize/(double)benchedSize*100.;

          DISPLAY("%2i#%1i-%-14.14s : %9i -> %9i (%5.2f%%),%7.1f MB/s\r",
                  cLevel, loopNb, inFileName, (int)benchedSize, (int)cSize, ratio, (double)benchedSize / (fastestC / CLOCKS_PER_SEC) / 1000000);

          /* Decompression */
          { size_t i; for (i=0; i<benchedSize; i++) orig_buff[i]=0; }     /* zeroing area, for CRC checking */

          nbLoops = 0;
          clockStart = clock();
          while (clock() == clockStart);
          clockStart = clock();
          while (BMK_getClockSpan(clockStart) < TIMELOOP_CLOCK) {
             for (chunkNb=0; chunkNb<nbChunks; chunkNb++)
                //chunkP[chunkNb].compressedSize = LZ4_decompress_fast(chunkP[chunkNb].compressedBuffer, chunkP[chunkNb].origBuffer, chunkP[chunkNb].origSize);
                LZ4_decompress_safe(chunkP[chunkNb].compressedBuffer, chunkP[chunkNb].origBuffer, chunkP[chunkNb].compressedSize, chunkP[chunkNb].origSize);
             nbLoops++;
          }
          clockEnd = BMK_getClockSpan(clockStart);

          nbLoops += !nbLoops;   /* avoid division by zero */
          if ((double)clockEnd < fastestD*nbLoops) fastestD = (double)clockEnd/nbLoops;
          DISPLAY("%2i#%1i-%-14.14s : %9i -> %9i (%5.2f%%),%7.1f MB/s ,%7.1f MB/s \r",
                  cLevel, loopNb, inFileName, (int)benchedSize, (int)cSize, ratio,
                  (double)benchedSize / (fastestC / CLOCKS_PER_SEC) / 1000000, (double)benchedSize / (fastestD / CLOCKS_PER_SEC) / 1000000 );

          /* CRC Checking */
          crcCheck = XXH32(orig_buff, benchedSize,0);
          if (crcOrig!=crcCheck) { DISPLAY("\n!!! WARNING !!! %14s : Invalid Checksum : %x != %x\n", inFileName, (unsigned)crcOrig, (unsigned)crcCheck); break; }
        }

        if (crcOrig==crcCheck) {
            if (ratio < 100.)
                DISPLAY("%2i#%-16.16s : %9i -> %9i (%5.2f%%),%7.1f MB/s ,%7.1f MB/s \n",
                        cLevel, inFileName, (int)benchedSize, (int)cSize, ratio,
                        (double)benchedSize / (fastestC / CLOCKS_PER_SEC) / 1000000, (double)benchedSize / (fastestD / CLOCKS_PER_SEC) / 1000000 );
            else
                DISPLAY("%2i#%-16.16s : %9i -> %9i (%5.1f%%),%7.1f MB/s ,%7.1f MB/s  \n",
                        cLevel, inFileName, (int)benchedSize, (int)cSize, ratio,
                        (double)benchedSize / (fastestC / CLOCKS_PER_SEC) / 1000000, (double)benchedSize / (fastestD / CLOCKS_PER_SEC) / 1000000 );
        }
        totals += benchedSize;
        totalz += cSize;
        totalc += fastestC;
        totald += fastestD;
      }

      free(orig_buff);
      free(compressedBuffer);
      free(chunkP);
  }

  if (nbFiles > 1)
        DISPLAY("%2i#%-16.16s :%10llu ->%10llu (%5.2f%%), %6.1f MB/s , %6.1f MB/s\n", cLevel, "  TOTAL",
                (long long unsigned)totals, (long long unsigned int)totalz, (double)totalz/(double)totals*100.,
                (double)totals/(totalc/CLOCKS_PER_SEC)/1000000, (double)totals/(totald/CLOCKS_PER_SEC)/1000000);

  if (BMK_pause) { DISPLAY("\npress enter...\n"); (void)getchar(); }

  return 0;
}


int BMK_benchFiles(const char** fileNamesTable, int nbFiles, int cLevel, int cLevelLast)
{
    int i, res = 0;

    if (cLevel > LZ4HC_MAX_CLEVEL) cLevel = LZ4HC_MAX_CLEVEL;
    if (cLevelLast > LZ4HC_MAX_CLEVEL) cLevelLast = LZ4HC_MAX_CLEVEL;
    if (cLevelLast < cLevel) cLevelLast = cLevel;

    if (cLevelLast > cLevel) DISPLAY("Benchmarking levels from %d to %d\n", cLevel, cLevelLast);
    for (i=cLevel; i<=cLevelLast; i++) {
        res = BMK_benchLevel(fileNamesTable, nbFiles, i);
        if (res != 0) break;
    }

    return res;
}
