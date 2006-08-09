/*
 * types.h
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 */

#ifndef VISLIB_TYPES_H_INCLUDED
#define VISLIB_TYPES_H_INCLUDED
#if (_MSC_VER > 1000)
#pragma once
#endif /* (_MSC_VER > 1000) */


#ifdef _WIN32
#include <windows.h>

#else /* _WIN32 */

// TODO: INT_PTR, UINT_PTR

#include <inttypes.h>

typedef int8_t CHAR;
typedef int8_t INT8;
typedef uint8_t UCHAR;
typedef uint8_t UINT8;
typedef uint8_t BYTE;

typedef int16_t SHORT;
typedef int16_t INT16;
typedef uint16_t USHORT;
typedef uint16_t UINT16;
typedef uint16_t WORD;

typedef int32_t INT;
typedef int32_t INT32;
typedef int32_t LONG;
typedef uint32_t UINT;
typedef uint32_t UINT32;
typedef uint32_t ULONG;
typedef uint32_t DWORD;

typedef int64_t LONGLONG;
typedef int64_t INT64;
typedef uint64_t ULONGLONG;
typedef uint64_t UINT64;


// TODO: Remove float/double or add bool etc.?

typedef float FLOAT;
typedef double DOUBLE;

#endif /* _WIN32 */

#endif /* VISLIB_TYPES_H_INCLUDED */
