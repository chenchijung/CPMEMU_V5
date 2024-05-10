#if !defined(LIMITS_H)
#define LIMITS_H
#include <limits.h>
#endif

#if !defined(YTYPES_H)
#define YTYPES_H

#if UCHAR_MAX == 255
typedef unsigned char	BYTE;
#else
#error Need to find an 8-bit type for BYTE
#endif

#if USHRT_MAX == 65535
typedef unsigned short	WORD;
#else
#error Need to find an 16-bit type for WORD
#endif

#if UINT_MAX == 4294967295U
typedef unsigned int ui32;
typedef int i32;
#else
#if ULONG_MAX == 4294967295UL
typedef unsigned long ui32;
typedef long i32;
#else
#error Need to find a 32-bit integer type for ui32 and i32
#endif
#endif

#endif /* YTYPES_H */

#define	bool	_Bool
#define	true	1
#define	false	0
