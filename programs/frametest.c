/*
    frameTest - test tool for lz4frame
    Copyright (C) Yann Collet 2014
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
    - LZ4 source repository : http://code.google.com/p/lz4/
    - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/**************************************
  Compiler specific
**************************************/
#define _CRT_SECURE_NO_WARNINGS   // fgets
#ifdef _MSC_VER    /* Visual Studio */
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4146)        /* disable: C4146: minus unsigned expression */
#endif


/**************************************
 Includes
**************************************/
#include <stdlib.h>
#include <stdio.h>      // fgets, sscanf
#include <sys/timeb.h>  // timeb
#include <string.h>     // strcmp
#include "lz4frame.h"


/**************************************
   Basic Types
**************************************/
#if defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)   /* C99 */
# include <stdint.h>
  typedef  uint8_t BYTE;
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


/**************************************
 Constants
**************************************/
#ifndef LZ4_VERSION
#  define LZ4_VERSION ""
#endif

#define NB_ATTEMPTS (1<<16)
#define COMPRESSIBLE_NOISE_LENGTH (1 << 21)
#define FUZ_MAX_BLOCK_SIZE (1 << 17)
#define FUZ_MAX_DICT_SIZE  (1 << 15)
#define FUZ_COMPRESSIBILITY_DEFAULT 50
#define PRIME1   2654435761U
#define PRIME2   2246822519U
#define PRIME3   3266489917U

#define KB *(1U<<10)
#define MB *(1U<<20)
#define GB *(1U<<30)


/**************************************
  Macros
**************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (displayLevel>=l) { DISPLAY(__VA_ARGS__); }


/*****************************************
  Local Parameters
*****************************************/
static int no_prompt = 0;
static char* programName;
static int displayLevel = 2;


/*********************************************************
  Fuzzer functions
*********************************************************/
static int FUZ_GetMilliStart(void)
{
   struct timeb tb;
   int nCount;
   ftime( &tb );
   nCount = (int) (tb.millitm + (tb.time & 0xfffff) * 1000);
   return nCount;
}


static int FUZ_GetMilliSpan( int nTimeStart )
{
   int nSpan = FUZ_GetMilliStart() - nTimeStart;
   if ( nSpan < 0 )
      nSpan += 0x100000 * 1000;
   return nSpan;
}


#  define FUZ_rotl32(x,r) ((x << r) | (x >> (32 - r)))
unsigned int FUZ_rand(unsigned int* src)
{
    U32 rand32 = *src;
    rand32 *= PRIME1;
    rand32 += PRIME2;
    rand32  = FUZ_rotl32(rand32, 13);
    *src = rand32;
    return rand32 >> 3;
}


#define FUZ_RAND15BITS  ((FUZ_rand(seed) >> 3) & 32767)
#define FUZ_RANDLENGTH  ( ((FUZ_rand(seed) >> 7) & 3) ? (FUZ_rand(seed) % 14) : (FUZ_rand(seed) & 511) + 15)
void FUZ_fillCompressibleNoiseBuffer(void* buffer, int bufferSize, double proba, U32* seed)
{
    BYTE* BBuffer = (BYTE*)buffer;
    int pos = 0;
    U32 P32 = (U32)(32768 * proba);

    // First Byte
    BBuffer[pos++] = (BYTE)(FUZ_rand(seed));

    while (pos < bufferSize)
    {
        // Select : Literal (noise) or copy (within 64K)
        if (FUZ_RAND15BITS < P32)
        {
            // Copy (within 64K)
            int ref, d;
            int length = FUZ_RANDLENGTH + 4;
            int offset = FUZ_RAND15BITS + 1;
            if (offset > pos) offset = pos;
            if (pos + length > bufferSize) length = bufferSize - pos;
            ref = pos - offset;
            d = pos + length;
            while (pos < d) BBuffer[pos++] = BBuffer[ref++];
        }
        else
        {
            // Literal (noise)
            int d;
            int length = FUZ_RANDLENGTH;
            if (pos + length > bufferSize) length = bufferSize - pos;
            d = pos + length;
            while (pos < d) BBuffer[pos++] = (BYTE)(FUZ_rand(seed) >> 5);
        }
    }
}



#define FUZ_MAX(a,b) (a>b?a:b)

int frameTest(U32 seed, int nbCycles, int startCycle, double compressibility)
{
	int testResult = 0;
	void* CNBuffer;
	void* compressedBuffer;
	void* decodedBuffer;
	U32 randState = seed;
	size_t cSize;

	(void)nbCycles; (void)startCycle;
	// Create compressible test buffer
	CNBuffer = malloc(COMPRESSIBLE_NOISE_LENGTH);
	FUZ_fillCompressibleNoiseBuffer(CNBuffer, COMPRESSIBLE_NOISE_LENGTH, compressibility, &randState);
	compressedBuffer = malloc(LZ4F_compressFrameBound(COMPRESSIBLE_NOISE_LENGTH, NULL));
	decodedBuffer = malloc(COMPRESSIBLE_NOISE_LENGTH);

	// Trivial test : one-step frame, all default
	cSize = LZ4F_compressFrame(compressedBuffer, LZ4F_compressFrameBound(64 KB, NULL), CNBuffer, 64 KB, NULL);
	if (LZ4F_isError(cSize)) goto _output_error;
	DISPLAY("Compressed %i bytes into a %i bytes frame \n", 64 KB, (int)cSize);

_end:
	free(CNBuffer);
	free(compressedBuffer);
	free(decodedBuffer);
	return testResult;

_output_error:
	testResult = 1;
	if(!no_prompt) getchar();
	goto _end;
}


int FUZ_usage(void)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [args]\n", programName);
    DISPLAY( "\n");
    DISPLAY( "Arguments :\n");
    DISPLAY( " -i#    : Nb of tests (default:%i) \n", NB_ATTEMPTS);
    DISPLAY( " -s#    : Select seed (default:prompt user)\n");
    DISPLAY( " -t#    : Select starting test number (default:0)\n");
    DISPLAY( " -p#    : Select compressibility in %% (default:%i%%)\n", FUZ_COMPRESSIBILITY_DEFAULT);
    DISPLAY( " -v     : verbose\n");
    DISPLAY( " -h     : display help and exit\n");
    return 0;
}


int main(int argc, char** argv) {
    U32 timestamp = FUZ_GetMilliStart();
    U32 seed=0;
    int seedset=0;
    int argNb;
    int nbTests = NB_ATTEMPTS;
    int testNb = 0;
    int proba = FUZ_COMPRESSIBILITY_DEFAULT;

    // Check command line
    programName = argv[0];
    for(argNb=1; argNb<argc; argNb++)
    {
        char* argument = argv[argNb];

        if(!argument) continue;   // Protection if argument empty

        // Decode command (note : aggregated commands are allowed)
        if (argument[0]=='-')
        {
            if (!strcmp(argument, "--no-prompt")) { no_prompt=1; seedset=1; displayLevel=1; continue; }

            while (argument[1]!=0)
            {
                argument++;
                switch(*argument)
                {
                case 'h':
                    return FUZ_usage();
                case 'v':
                    argument++;
                    displayLevel=4;
                    break;
                case 'i':
                    argument++;
                    nbTests=0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        nbTests *= 10;
                        nbTests += *argument - '0';
                        argument++;
                    }
                    break;
                case 's':
                    argument++;
                    seed=0; seedset=1;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        seed *= 10;
                        seed += *argument - '0';
                        argument++;
                    }
                    break;
                case 't':
                    argument++;
                    testNb=0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        testNb *= 10;
                        testNb += *argument - '0';
                        argument++;
                    }
                    break;
                case 'p':
                    argument++;
                    proba=0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        proba *= 10;
                        proba += *argument - '0';
                        argument++;
                    }
                    if (proba<0) proba=0;
                    if (proba>100) proba=100;
                    break;
                default: ;
                }
            }
        }
    }

    // Get Seed
    printf("Starting lz4frame tester (%i-bits, %s)\n", (int)(sizeof(size_t)*8), LZ4_VERSION);

    if (!seedset)
    {
        char userInput[50] = {0};
        printf("Select an Initialisation number (default : random) : ");
        fflush(stdout);
        if ( no_prompt || fgets(userInput, sizeof userInput, stdin) )
        {
            if ( sscanf(userInput, "%u", &seed) == 1 ) {}
            else seed = FUZ_GetMilliSpan(timestamp);
        }
    }
    printf("Seed = %u\n", seed);
    if (proba!=FUZ_COMPRESSIBILITY_DEFAULT) printf("Compressibility : %i%%\n", proba);

    if (nbTests<=0) nbTests=1;

    return frameTest(seed, nbTests, testNb, ((double)proba) / 100);
}
