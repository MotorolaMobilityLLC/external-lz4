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

/* LZ4F is a stand-alone API to create LZ4-compressed frames
 * fully conformant to specification v1.4.1.
 * All related operations, including memory management, are handled by the library.
 * You don't need lz4.h when using lz4frame.h.
 * */
 
#pragma once

#if defined (__cplusplus)
extern "C" {
#endif


/**************************************
   Error management
**************************************/
typedef enum { 
	OK_NOERROR                     =  0,
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

typedef enum { LZ4F_default=0, max64KB=4, max256KB=5, max1MB=6, max4MB=7} maxBlockSize_t;
typedef enum { LZ4F_default=0, blockLinked, blockIndependent} blockMode_t;
typedef enum { LZ4F_default=0, contentChecksumEnabled, noContentChecksum} contentChecksum_t;

typedef struct {
  unsigned          compressionLevel;
  maxBlockSize_t    maxBlockSize;
  blockMode_t       blockMode;
  contentChecksum_t contentChecksumFlag;
  unsigned          autoFlush;
} LZ4F_preferences_t;



/**********************************
 * Simple compression function
 * *********************************/
size_t LZ4F_compressFrameBound(size_t srcSize, const LZ4F_preferences_t* preferences);

size_t LZ4F_compressFrame(void* dstBuffer, size_t dstMaxSize, const void* srcBuffer, size_t srcSize, const LZ4F_preferences_t* preferences);
/* LZ4F_compressFrame()
 * Compress an entire srcBuffer into a valid LZ4 frame, as defined by specification v1.4.1, in a single step.
 * The most important rule is that dstBuffer MUST be large enough (dstMaxSize) to ensure compression completion even in worst case.
 * You can get the minimum value of dstMaxSize by using LZ4F_compressFrameBound()
 * If this condition is not respected, LZ4F_compressFrame() will fail (result is an errorCode)
 * The LZ4F_preferences_t structure is optional : you can provide NULL as argument. All preferences will be set to default.
 * The result of the function is the number of bytes written into dstBuffer.
 * The function outputs an error code if it fails (can be tested using LZ4F_isError())
 */



/**********************************
 * Advanced compression functions 
 * *********************************/

typedef void* LZ4F_compressionContext_t;

typedef struct {
  int stableSrc;          /* unused for the time being, must be 0 */
} LZ4F_compressOptions_t;

size_t LZ4F_compressBound(size_t srcSize,    const LZ4F_preferences_t* preferences);
size_t LZ4F_getMaxSrcSize(size_t dstMaxSize, const LZ4F_preferences_t* preferences);
/* LZ4F_compressBound() : gives the size of Dst buffer given a srcSize to handle worst case situations.
 * LZ4F_getMaxSrcSize() : gives max allowed srcSize given dstMaxSize to handle worst case situations.
 *                        You can use dstMaxSize==0 to know the "natural" srcSize instead (block size).
 * The LZ4F_preferences_t structure is optional : you can provide NULL as argument, all preferences will then be set to default.
 */

/* Resource Management */

#define LZ4F_VERSION 100
LZ4F_errorCode_t LZ4F_createCompressionContext(LZ4F_compressionContext_t* LZ4F_compressionContext, int version, const LZ4F_preferences_t* preferences);
LZ4F_errorCode_t LZ4F_freeCompressionContext(LZ4F_compressionContext_t LZ4F_compressionContext);
/* LZ4F_createCompressionContext() :
 * The first thing to do is to create a compressionContext object, which will be used in all compression operations.
 * This is achieved using LZ4F_createCompressionContext(), which takes as argument a version and an LZ4F_preferences_t structure.
 * The version provided MUST be LZ4F_VERSION. It is intended to track potential version differences between different binaries.
 * The LZ4F_preferences_t structure is optional : you can provide NULL as argument, all preferences will then be set to default.
 * The function will provide a pointer to a fully allocated LZ4F_compressionContext_t object.
 * If the result LZ4F_errorCode_t is not zero, there was an error during context creation.
 * Object can release its memory using LZ4F_freeCompressionContext();
 */


/* Compression */

size_t LZ4F_compressBegin(LZ4F_compressionContext_t* compressionContextPtr, void* dstBuffer, size_t dstMaxSize);
/* LZ4F_compressBegin() :
 * will write the frame header into dstBuffer.
 * dstBuffer must be large enough to accomodate a header (dstMaxSize). Maximum header size is 15 bytes.
 * The result of the function is the number of bytes written into dstBuffer for the header
 * or an error code (can be tested using LZ4F_isError())
 */

size_t LZ4F_compress(LZ4F_compressionContext_t compressionContext, void* dstBuffer, size_t dstMaxSize, const void* srcBuffer, size_t srcSize, const LZ4F_compressOptions_t* compressOptions);
/* LZ4F_compress()
 * You can then call LZ4F_compress() repetitively to compress as much data as necessary.
 * The most important rule is that dstBuffer MUST be large enough (dstMaxSize) to ensure compression completion even in worst case.
 * You can get the minimum value of dstMaxSize by using LZ4F_compressBound()
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
 * The result of the function is the number of bytes written into dstBuffer 
 * (it can be zero, this means there was no data left within compressionContext)
 * The function outputs an error code if it fails (can be tested using LZ4F_isError())
 * The LZ4F_compressOptions_t structure is optional : you can provide NULL as argument.
 */
       
size_t LZ4F_compressEnd(LZ4F_compressionContext_t compressionContext, void* dstBuffer, size_t dstMaxSize, const LZ4F_compressOptions_t* compressOptions);
/* LZ4F_compressEnd()
 * When you want to properly finish the compressed frame, just call LZ4F_compressEnd().
 * It will flush whatever data remained within compressionContext (like LZ4_flush())
 * but also properly finalize the frame, with an endMark and a checksum.
 * The result of the function is the number of bytes written into dstBuffer (necessarily >= 4 (endMark size))
 * The function outputs an error code if it fails (can be tested using LZ4F_isError())
 * The LZ4F_compressOptions_t structure is optional : you can provide NULL as argument.
 * compressionContext can then be used again, starting with LZ4F_compressBegin(). The preferences will remain the same.
 */


/**********************************
 * Decompression functions 
 * *********************************/

typedef void* LZ4F_decompressionContext_t;

typedef struct {
  int stableDst;          /* unused for the time being, must be 0 */
} LZ4F_decompressOptions_t;

/* Resource management */

LZ4F_errorCode_t LZ4F_createDecompressionContext(LZ4F_compressionContext_t* LZ4F_decompressionContext);
LZ4F_errorCode_t LZ4F_freeDecompressionContext(LZ4F_compressionContext_t LZ4F_decompressionContext);
/* LZ4F_createDecompressionContext() :
 * The first thing to do is to create a decompressionContext object, which will be used in all decompression operations.
 * This is achieved using LZ4F_createDecompressionContext().
 * The function will provide a pointer to a fully allocated and initialised LZ4F_decompressionContext object.
 * If the result LZ4F_errorCode_t is not zero, there was an error during context creation.
 * Object can release its memory using LZ4F_freeDecompressionContext();
 */
	
/* Header information */

LZ4F_errorCode_t LZ4F_analyzeHeader(LZ4F_decompressionContext_t* decompressionContextPtr, void* srcBuffer, size_t* srcSize);
/* LZ4F_analyzeHeader()
 * 
 * This function decodes header information, such as blockSize.
 * LZ4F_analyzeHeader() is not compulsory. You could start instead by calling directly LZ4F_decompress.
 * The objective is to only extract useful header information, without starting decompression.
 * 
 * The number of bytes read from srcBuffer will be provided within *srcSize (necessarily <= original value).
 * 
 * The function result is an error code which can be tested using LZ4F_isError().
 */

size_t           LZ4F_getBlockSize(LZ4F_decompressionContext_t decompressionContext);
/* LZ4F_getBlockSize()
 * Provides the maximum size of a (decompressed) block within previously analyzed frame.
 * Primarily useful for memory allocation purposes.
 * Header must be previously analyzed, using LZ4F_analyzeHeader().
 * 
 * The result is the maximum size of a single block,
 * or an error code which can be tested using LZ4F_isError() in case of failure.
 */

/* Decompression */

LZ4F_errorCode_t LZ4F_decompress(LZ4F_decompressionContext_t decompressionContext, void* dstBuffer, size_t* dstSize, const void* srcBuffer, size_t* srcSize, const LZ4F_decompressOptions_t* decompressOptions);
/* LZ4F_decompress()
 * LZ4F_decompress() will be called repetitively to regenerate as much data as necessary.
 * The function will attempt to decode *srcSize from srcBuffer, into dstBuffer of maximum size *dstSize.
 *
 * The number of bytes generated into dstBuffer will be provided within *dstSize (necessarily <= original value).
 * 
 * The number of bytes effectively read from srcBuffer will be provided within *srcSize (necessarily <= original value).
 * If the number of bytes read is < number of bytes provided, then the function could not complete decompression operation.
 * You will have to call it again, using the same src arguments (but eventually different dst arguments).
 * 
 * The function result is an error code which can be tested using LZ4F_isError().
 */



#if defined (__cplusplus)
}
#endif
