/*
fseDist.c
Universal length FSE coder
Copyright (C) Yann Collet 2012-2014
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
*/

//**************************************
// Compiler Options
//**************************************
#if defined(_MSC_VER)
#  define _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_DEPRECATE     // VS2005
#  pragma warning(disable : 4127)      // disable: C4127: conditional expression is constant
#  include <intrin.h>
#endif


//**************************************
// Includes
//**************************************
#include <stdlib.h>   // malloc
#include <stdio.h>    // fprintf, fopen, ftello64
#include <string.h>   // memcpy
#include "fse.h"


//**************************************
// Compiler specifics
//**************************************
#ifdef __GNUC__
#  define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#endif


//**************************************
// Basic Types
//**************************************
#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   // C99
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


//**************************************
// Constants
//**************************************
#define KB *(1U<<10)
#define MB *(1U<<20)
#define GB *(1U<<30)


//**************************************
// Macros
//**************************************
#define DISPLAY(...) fprintf(stderr, __VA_ARGS__)


//*********************************************************
//  Common internal functions
//*********************************************************
inline int FSED_highbit (register U32 val)
{
#   if defined(_MSC_VER)   // Visual
    unsigned long r;
    _BitScanReverse ( &r, val );
    return (int) r;
#   elif defined(__GNUC__) && (GCC_VERSION >= 304)   // GCC Intrinsic
    return 31 - __builtin_clz (val);
#   else   // Software version
    static const int DeBruijnClz[32] = { 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
    U32 v = val;
    int r;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    r = DeBruijnClz[ (U32) (v * 0x07C4ACDDU) >> 27];
    return r;
#   endif
}


//*********************************************************
//  U16 Compression functions
//*********************************************************
int FSED_countU16 (unsigned int* count, const U16* source, int sourceSize)
{
    const U16* ip = source;
    const U16* const iend = source+sourceSize;
    int   i;

    U32   Counting1[16] = {0};
    U32   Counting2[16] = {0};
    U32   Counting3[16] = {0};
    U32   Counting4[16] = {0};

    // Init checks
    if (!sourceSize) return -1;                              // Error : no input

    while (ip < iend-3)
    {
        Counting1[FSED_highbit(*ip++)]++;
        Counting2[FSED_highbit(*ip++)]++;
        Counting3[FSED_highbit(*ip++)]++;
        Counting4[FSED_highbit(*ip++)]++;
    }
    while (ip<iend) Counting1[FSED_highbit(*ip++)]++;

    for (i=0; i<16; i++) count[i] = Counting1[i] + Counting2[i] + Counting3[i] + Counting4[i];

    {
        int max = 16;
        while (!count[max-1]) max--;
        return max;
    }
}


int FSED_noCompressU16(void* dest, const U16* source, int sourceSize)
{
    BYTE* header = (BYTE*)dest;
    *header=0;
    memcpy(header+1, source, sourceSize*2);
    return sourceSize*2 + 1;
}


int FSED_writeSingleU16(void* dest, U16 distance)
{
    BYTE* header = (BYTE*) dest;
    U16* value = (U16*)(header+1);
    *header=1;
    *value = distance;
    return 3;
}


static inline void FSED_encodeU16(ptrdiff_t* state, bitContainer_forward_t* bitC, U16 value, const void* symbolTT, const void* stateTable)
{
    BYTE nbBits = (BYTE) FSED_highbit(value);
    FSE_addBits(bitC, nbBits, (size_t)value);
    FSE_encodeByte(state, bitC, nbBits, symbolTT, stateTable);
}


int FSED_compressU16_usingCTable (void* dest, const U16* source, int sourceSize, const void* CTable)
{
    const U16* const istart = source;
    const U16* ip;
    const U16* const iend = istart + sourceSize;

    BYTE* op = (BYTE*) dest;
    U32* streamSize;
    ptrdiff_t state;
    bitContainer_forward_t bitC = {0,0};
    const void* stateTable;
    const void* symbolTT;


    streamSize = (U32*)FSE_initCompressionStream((void**)&op, &state, &symbolTT, &stateTable, CTable);

    ip=iend-1;
    while (ip>istart)
    {
        FSED_encodeU16(&state, &bitC, *ip--, symbolTT, stateTable);
        if (sizeof(size_t)>4) FSED_encodeU16(&state, &bitC, *ip--, symbolTT, stateTable);   // static test
        FSE_flushBits((void**)&op, &bitC);
    }
    if (ip==istart) { FSED_encodeU16(&state, &bitC, *ip--, symbolTT, stateTable); FSE_flushBits((void**)&op, &bitC); }

    return FSE_closeCompressionStream(op, &bitC, 1, state,0,0,0, streamSize, CTable);
}


#define FSED_U16_MAXMEMLOG 10
int FSED_compressU16 (void* dest, const U16* source, int sourceSize, int memLog)
{
    const U16* const istart = (const U16*) source;
    const U16* ip = istart;

    BYTE* const ostart = (BYTE*) dest;
    BYTE* op = ostart;

    int nbSymbols = 16;
    U32 counting[16];
    U32 CTable[2 + 16 + (1<<FSED_U16_MAXMEMLOG)];


    if (memLog > FSED_U16_MAXMEMLOG) return -1;
    // early out
    if (sourceSize <= 1) return FSED_noCompressU16 (ostart, istart, sourceSize);

    // Scan for stats
    nbSymbols = FSED_countU16 (counting, ip, sourceSize);

    // Normalize
    memLog = FSE_normalizeCount (counting, memLog, counting, sourceSize, nbSymbols);
    if (memLog==0) return FSED_writeSingleU16 (ostart, *source);   // only one distance in the set

    op += FSE_writeHeader (op, counting, nbSymbols, memLog);

    // Compress
    FSE_buildCTable (&CTable, counting, nbSymbols, memLog);
    op += FSED_compressU16_usingCTable (op, ip, sourceSize, &CTable);

    // check compressibility
    if ( (op-ostart) >= (sourceSize*2-1) )
        return FSED_noCompressU16 (ostart, istart, sourceSize);

    return (int) (op-ostart);
}


//*********************************************************
//  U16 Decompression functions
//*********************************************************
int FSED_decompressRawU16 (U16* out, int osize, const BYTE* in)
{
    memcpy (out, in+1, osize*2);
    return osize*2+1;
}

int FSED_decompressSingleU16 (U16* out, int osize, U16 value)
{
    int i;
    for (i=0; i<osize; i++) *out++ = value;
    return 3;
}


int FSED_decompressU16_usingDTable (unsigned short* dest, const int originalSize, const void* compressed, const void* DTable, const int tableLog)
{
    const void* ip = (const BYTE*) compressed;
    const void* iend;
    unsigned short* op = dest;
    unsigned short* const oend = op + originalSize;
    bitContainer_backward_t bitC;
    int nbStates;
    U32 state, state2, state3, state4;

    // Init
    iend = FSE_initDecompressionStream(&bitC, &nbStates, &state, &state2, &state3, &state4, &ip, tableLog);

    // Hot loop
    while (op<oend)
    {
        int nbBits = FSE_decodeSymbol(&state, &bitC, DTable);
        unsigned short value = (U16)FSE_readBits(&bitC, nbBits);
        value += 1<<nbBits;
        *op++ = value;
        FSE_updateBitStream(&bitC, (const void**)&ip);
    }

    return FSE_closeDecompressionStream(iend, ip);
}


int FSED_decompressU16 (U16* dest, int originalSize,
                       const void* compressed)
{
    const BYTE* const istart = (const BYTE*) compressed;
    const BYTE* ip = istart;
    U32  counting[16];
    U32  DTable[1<<FSED_U16_MAXMEMLOG];
    BYTE headerId;
    int  nbSymbols;
    int  tableLog;

    // headerId early outs
    headerId = ip[0] & 3;
    if (headerId==0) return FSED_decompressRawU16 (dest, originalSize, istart);
    if (headerId==1) return FSED_decompressSingleU16 (dest, originalSize, (U16)(*(U16*)(istart+1)));

    // normal FSE decoding mode
    ip += FSE_readHeader (counting, &nbSymbols, &tableLog, istart);
    FSE_buildDTable (DTable, counting, nbSymbols, tableLog);
    ip += FSED_decompressU16_usingDTable (dest, originalSize, ip, DTable, tableLog);

    return (int) (ip-istart);
}


//*********************************************************
//  U16Log2 Compression functions
//*********************************************************
// note : values MUST be >= (1<<LN)
#define LN 3
int FSED_Log2(U16 value)
{
    int hb = FSED_highbit(value>>LN);
    return (hb * (1<<LN)) + (value>>hb) - (1<<LN);
}


int FSED_countU16Log2 (unsigned int* count, const U16* source, int sourceSize)
{
    const U16* ip = source;
    const U16* const iend = source+sourceSize;
    int   i;

    U32   Counting1[64] = {0};
    U32   Counting2[64] = {0};
    U32   Counting3[64] = {0};
    U32   Counting4[64] = {0};

    // Init checks
    if (!sourceSize) return -1;                              // Error : no input

    while (ip < iend-3)
    {
        Counting1[FSED_Log2(*ip++)]++;
        Counting2[FSED_Log2(*ip++)]++;
        Counting3[FSED_Log2(*ip++)]++;
        Counting4[FSED_Log2(*ip++)]++;
    }
    while (ip<iend) Counting1[FSED_Log2(*ip++)]++;

    for (i=0; i<64; i++) count[i] = Counting1[i] + Counting2[i] + Counting3[i] + Counting4[i];

    {
        int max = 64;
        while (!count[max-1]) max--;
        return max;
    }
}


static inline void FSED_encodeU16Log2(ptrdiff_t* state, bitContainer_forward_t* bitC, U16 value, const void* symbolTT, const void* stateTable)
{
    int nbBits = FSED_highbit(value>>LN);
    BYTE symbol = (BYTE)FSED_Log2(value);
    FSE_addBits(bitC, nbBits, (size_t)value);
    FSE_encodeByte(state, bitC, symbol, symbolTT, stateTable);
}


int FSED_compressU16Log2_usingCTable (void* dest, const U16* source, int sourceSize, const void* CTable)
{
    const U16* const istart = source;
    const U16* ip;
    const U16* const iend = istart + sourceSize;

    BYTE* const ostart = (BYTE*) dest;
    BYTE* op = (BYTE*) dest;

    const int memLog = ( (U16*) CTable) [0];
    const int tableSize = 1 << memLog;
    const U16* const stateTable = ( (const U16*) CTable) + 2;
    const void* const symbolTT = (const void*) (stateTable + tableSize);

    ptrdiff_t state=tableSize;
    bitContainer_forward_t bitC = {0,0};
    U32* streamSize = (U32*) op;
    op += 4;

    ip=iend-1;
    // cheap last-symbol storage
    //if (*ip < tableSize) state += *ip--;

    while (ip>istart)
    {
        FSED_encodeU16Log2(&state, &bitC, *ip--, symbolTT, stateTable);
        if (sizeof(size_t)>4) FSED_encodeU16Log2(&state, &bitC, *ip--, symbolTT, stateTable);   // static test
        FSE_flushBits((void**)&op, &bitC);
    }
    if (ip==istart) { FSED_encodeU16Log2(&state, &bitC, *ip--, symbolTT, stateTable); FSE_flushBits((void**)&op, &bitC); }

    // Finalize block
    FSE_addBits(&bitC, state, memLog);
    FSE_flushBits((void**)&op, &bitC);
    *streamSize = (U32) ( ( (op- (BYTE*) streamSize) *8) + bitC.bitPos);
    op += bitC.bitPos > 0;

    return (int) (op-ostart);
}


#define FSED_U16LOG2_MAXMEMLOG 11
int FSED_compressU16Log2 (void* dest, const U16* source, int sourceSize, int memLog)
{
    const U16* const istart = (const U16*) source;
    const U16* ip = istart;

    BYTE* const ostart = (BYTE*) dest;
    BYTE* op = ostart;

    int nbSymbols = 16;
    U32 counting[64];
    U32 CTable[2 + 16 + (1<<FSED_U16LOG2_MAXMEMLOG)];


    if (memLog > FSED_U16LOG2_MAXMEMLOG) return -1;
    // early out
    if (sourceSize <= 1) return FSED_noCompressU16 (ostart, istart, sourceSize);

    // Scan for stats
    nbSymbols = FSED_countU16Log2 (counting, ip, sourceSize);

    // Normalize
    memLog = FSE_normalizeCount (counting, memLog, counting, sourceSize, nbSymbols);
    if (memLog==0) return FSED_writeSingleU16 (ostart, *source);   // only one distance in the set

    op += FSE_writeHeader (op, counting, nbSymbols, memLog);

    // Compress
    FSE_buildCTable (&CTable, counting, nbSymbols, memLog);
    op += FSED_compressU16Log2_usingCTable (op, ip, sourceSize, &CTable);

    // check compressibility
    if ( (op-ostart) >= (sourceSize*2-1) )
        return FSED_noCompressU16 (ostart, istart, sourceSize);

    return (int) (op-ostart);
}


//*********************************************************
//  U32 Compression functions
//*********************************************************
#define FSED_MAXBITS_U32 26
int FSED_countU32 (unsigned int* count, const U32* source, int sourceSize)
{
    const U32* ip = source;
    const U32* const iend = source+sourceSize;
    int   i;

    U32   Counting1[FSED_MAXBITS_U32] = {0};
    U32   Counting2[FSED_MAXBITS_U32] = {0};
    U32   Counting3[FSED_MAXBITS_U32] = {0};
    U32   Counting4[FSED_MAXBITS_U32] = {0};

    // Init checks
    if (!sourceSize) return -1;                              // Error : no input

    while (ip < iend-3)
    {
        Counting1[FSED_highbit(*ip++)]++;
        Counting2[FSED_highbit(*ip++)]++;
        Counting3[FSED_highbit(*ip++)]++;
        Counting4[FSED_highbit(*ip++)]++;
    }
    while (ip<iend) Counting1[FSED_highbit(*ip++)]++;

    for (i=0; i<FSED_MAXBITS_U32; i++) count[i] = Counting1[i] + Counting2[i] + Counting3[i] + Counting4[i];

    {
        int max = FSED_MAXBITS_U32;
        while (!count[max-1]) max--;
        return max;
    }
}


int FSED_noCompressU32(void* dest, const U32* source, int sourceSize)
{
    BYTE* header = (BYTE*)dest;
    *header=0;
    memcpy(header+1, source, sourceSize*4);
    return (sourceSize*4) + 1;
}


int FSED_writeSingleU32(void* dest, U32 val)
{
    BYTE* header = (BYTE*) dest;
    U32* value = (U32*)(header+1);
    *header=1;
    *value = val;
    return 5;
}


void FSED_encodeU32(ptrdiff_t* state, bitContainer_forward_t* bitC, void** op, U32 value, const void* symbolTT, const void* stateTable)
{
    BYTE nbBits = (BYTE) FSED_highbit(value);
    FSE_addBits(bitC, nbBits, (size_t)value);
    if (sizeof(size_t)==4) FSE_flushBits(op, bitC);   // static test
    FSE_encodeByte(state, bitC, nbBits, symbolTT, stateTable);
}


int FSED_compressU32_usingCTable (void* dest, const U32* source, int sourceSize, const void* CTable)
{
    const U32* const istart = source;
    const U32* ip;
    const U32* const iend = istart + sourceSize;

    BYTE* op = (BYTE*) dest;
    bitContainer_forward_t bitC = {0,0};

    #if 1   // This version is bit faster for the time being
    const int memLog = ( (U16*) CTable) [0];
    ptrdiff_t state = (ptrdiff_t)1 << memLog;
    const U16* const stateTable = ( (const U16*) CTable) + 2;
    const void* const symbolTT = (const void*) (stateTable + ((ptrdiff_t)1 << memLog));
    U32* streamSize = (U32*) op;
    op += 4;
    #else
    ptrdiff_t state;
    const void* stateTable;
    const void* symbolTT;
    U32* streamSize = (U32*)FSE_initCompressionStream((void**)&op, &state, &symbolTT, &stateTable, CTable);
    #endif


    ip=iend-1;
    while (ip>=istart)
    {
        FSED_encodeU32(&state, &bitC, (void**)&op, *ip--, symbolTT, stateTable);
        FSE_flushBits((void**)&op, &bitC);
    }

    return FSE_closeCompressionStream(op, &bitC, 1, state,0,0,0, streamSize, CTable);
}


#define FSED_U32_MAXMEMLOG 11
int FSED_compressU32 (void* dest, const U32* source, int sourceSize, int memLog)
{
    const U32* const istart = (const U32*) source;
    const U32* ip = istart;

    BYTE* const ostart = (BYTE*) dest;
    BYTE* op = ostart;

    int nbSymbols = FSED_MAXBITS_U32;
    U32 counting[FSED_MAXBITS_U32];
    U32 CTable[2 + FSED_MAXBITS_U32 + (1<<FSED_U32_MAXMEMLOG)];


    if (memLog > FSED_U32_MAXMEMLOG) return -1;
    // early out
    if (sourceSize <= 1) return FSED_noCompressU32 (ostart, istart, sourceSize);

    // Scan for stats
    nbSymbols = FSED_countU32 (counting, ip, sourceSize);

    // Normalize
    memLog = FSE_normalizeCount (counting, memLog, counting, sourceSize, nbSymbols);
    if (memLog==0) return FSED_writeSingleU32 (ostart, *source);

    op += FSE_writeHeader (op, counting, nbSymbols, memLog);

    // Compress
    FSE_buildCTable (&CTable, counting, nbSymbols, memLog);
    op += FSED_compressU32_usingCTable (op, ip, sourceSize, &CTable);

    // check compressibility
    if ( (op-ostart) >= (sourceSize*4-1) )
        return FSED_noCompressU32 (ostart, istart, sourceSize);

    return (int) (op-ostart);
}


/*********************************************************
    U32 Decompression functions
*********************************************************/
int FSED_decompressRawU32 (U32* out, int osize, const BYTE* in)
{
    memcpy (out, in+1, osize*4);
    return osize*4+1;
}

int FSED_decompressSingleU32 (U32* out, int osize, U32 value)
{
    int i;
    for (i=0; i<osize; i++) *out++ = value;
    return 5;
}


int FSED_decompressU32_usingDTable (unsigned int* dest, const int originalSize, const void* compressed, const void* DTable, const int tableLog)
{
    const void* ip = compressed;
    const void* iend;
    unsigned int* op = dest;
    unsigned int* const oend = op + originalSize;
    bitContainer_backward_t bitC;
    int nbStates;
    U32 state, state2, state3, state4;

    // Init
    iend = FSE_initDecompressionStream(&bitC, &nbStates, &state, &state2, &state3, &state4, &ip, tableLog);

    // Hot loop
    while (op<oend)
    {
        int nbBits = FSE_decodeSymbol(&state, &bitC, DTable);
        U32 value;
        FSE_updateBitStream(&bitC, &ip);
        value = FSE_readBits(&bitC, nbBits);
        value += 1<<nbBits;
        *op++ = value;
        FSE_updateBitStream(&bitC, &ip);
    }

    //return FSE_closeDecompressionStream(iend, ip);  // slower
    return (int) ((const BYTE*)iend- (const BYTE*)compressed);
}


int FSED_decompressU32 (U32* dest, int originalSize,
                       const void* compressed)
{
    const BYTE* const istart = (const BYTE*) compressed;
    const BYTE* ip = istart;
    U32  counting[FSED_MAXBITS_U32];
    U32  DTable[1<<FSED_U32_MAXMEMLOG];
    BYTE headerId;
    int  nbSymbols;
    int  tableLog;

    // headerId early outs
    headerId = ip[0] & 3;
    if (headerId==0) return FSED_decompressRawU32 (dest, originalSize, istart);
    if (headerId==1) return FSED_decompressSingleU32 (dest, originalSize, (U32)(*(U32*)(istart+1)));

    // normal FSE decoding mode
    ip += FSE_readHeader (counting, &nbSymbols, &tableLog, istart);
    FSE_buildDTable (DTable, counting, nbSymbols, tableLog);
    ip += FSED_decompressU32_usingDTable (dest, originalSize, ip, DTable, tableLog);

    return (int) (ip-istart);
}

