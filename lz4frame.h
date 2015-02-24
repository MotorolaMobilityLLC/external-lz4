/*
   LZ4 auto-framing library
   Header File
   Copyright (C) 2011-2014, Yann Collet.
   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - LZ4 source repository : http://code.google.com/p/lz4/
   - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/
#pragma once

#if defined (__cplusplus)
extern "C" {
#endif


/**************************************
   Error management
**************************************/
typedef enum { 
	ERROR_GENERIC                  = -1U, 
	ERROR_maxDstSize_tooSmall      = -2U,
    ERROR_compressionLevel_invalid = -3U,
    ERROR_maxBlockSize_invalid     = -4U,
    ERROR_blockMode_invalid        = -5U,
    ERROR_contentChecksumFlag_invalid = -6U,
	ERROR_MIN = -7U 
	} LZ4F_errorCode_t;
int LZ4F_isError(size_t code);


/**************************************
   Framing compression functions
**************************************/

typedef void* LZ4F_compressionContext_t;

typedef enum { LZ4F_default=0, max64KB=4, max256KB=5, max1MB=6, max4MB=7} maxBlockSize_t;
typedef enum { LZ4F_default=0, blockLinked, blockIndependent} blockMode_t;
typedef enum { LZ4F_default=0, contentChecksumEnabled, contentNoChecksum} contentChecksum_t;

typedef struct {
  int               compressionLevel;   /* valid values are >= 0 */
  maxBlockSize_t    maxBlockSize;
  blockMode_t       blockMode;
  contentChecksum_t contentChecksumFlag;
} LZ4F_preferences_t;

typedef struct {
  int compressionLevel;   /* default is compressionLevel value passed within preferences */
  int autoFlush;          /* default is 0 = no autoflush */
} LZ4F_compressOptions_t;

size_t LZ4F_compressBound(size_t srcSize,    const LZ4F_preferences_t* preferences);
size_t LZ4F_getMaxSrcSize(size_t maxDstSize, const LZ4F_preferences_t* preferences);


/* Main compression functions */

size_t LZ4F_compressInit(LZ4F_compressionContext_t* compressionContextPtr, void* dstBuffer, size_t dstMaxSize, const LZ4F_preferences_t* preferences);
/* LZ4F_compressInit() :
 * The first thing to do is to create a compressionContext object.
 * This is achieved using LZ4F_compressInit(), which takes as argument a dstBuffer and a LZ4F_preferences_t structure.
 * The LZ4F_preferences_t structure is optional : you can provide NULL as argument, all preferences will then be set to default.
 * The dstBuffer is required : LZ4F_compressInit() will write the frame header into it.
 * dstBuffer must be large enough to accomodate a header (dstMaxSize). Maximum header size is 15 bytes.
 * The result of the function is the number of bytes written into dstBuffer for the header, or an error code (can be tested using LZ4F_isError())
 */

size_t LZ4F_compress(LZ4F_compressionContext_t compressionContext, void* dstBuffer, size_t dstMaxSize, const void* srcBuffer, size_t srcSize, const LZ4F_compressOptions_t* compressOptions);
/* LZ4F_compress()
 * You can then call LZ4F_compress() repetitively to compress as much data as necessary.
 * The most important rule to keep in mind is that dstBuffer must be large enough (dstMaxSize) to ensure compression completion.
 * You can know the minimum size of dstMaxSize by using LZ4F_compressBound()
 * Conversely, given a fixed dstMaxSize value, you can know the maximum srcSize authorized using LZ4F_getMaxSrcSize()
 * If this condition is not respected, LZ4F_compress() will fail (result is an errorCode)
 * The LZ4F_compressOptions_t structure is optional : you can provide NULL as argument.
 * The result of the function is the number of bytes written into dstBuffer (it can be zero, meaning input data is just stored within compressionContext for a future block to complete)
 * The function outputs an error code if it fails (can be tested using LZ4F_isError())
 */
       
size_t LZ4F_flush(LZ4F_compressionContext_t compressionContext, void* dstBuffer, size_t dstMaxSize, const LZ4F_compressOptions_t* compressOptions);
/* LZ4F_flush()
 * Should you need to create compressed data immediately, without waiting for a block to be filled,
 * you can call LZ4_flush(), which will immediately compress any remaining data stored within compressionContext.
 * The result of the function is the number of bytes written into dstBuffer (it can be zero there was no data left within compressionContext)
 * The function outputs an error code if it fails (can be tested using LZ4F_isError())
 * The LZ4F_compressOptions_t structure is optional : you can provide NULL as argument.
 */
       
size_t LZ4F_compressEnd(LZ4F_compressionContext_t compressionContext, void* dstBuffer, size_t dstMaxSize, const LZ4F_compressOptions_t* compressOptions);
/* LZ4F_compressEnd()
 * When you want to properly finish the compressed frame, just call LZ4F_compressEnd().
 * It will flush whatever data remained within compressionContext (like LZ4_flush())
 * but also properly finalize the frame, with an endMark and a checksum.
 * It will also free compressionContext memory, so you can't call LZ4F_compress() anymore afterwards.
 * The result of the function is the number of bytes written into dstBuffer (necessarily >= 4 (endMark size))
 * The function outputs an error code if it fails (can be tested using LZ4F_isError())
 * The LZ4F_compressOptions_t structure is optional : you can provide NULL as argument.
 */


#if defined (__cplusplus)
}
#endif
