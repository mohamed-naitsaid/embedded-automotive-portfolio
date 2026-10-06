#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>

typedef uint8_t  uint8;
typedef int8_t   sint8;
typedef uint16_t uint16;
typedef int16_t  sint16;
typedef uint32_t uint32;
typedef int32_t  sint32;
typedef uint64_t uint64;
typedef int64_t  sint64;

typedef float  float32;
typedef double float64;

typedef uint8 boolean;

#ifndef TRUE
#define TRUE ((boolean)1U)
#endif

#ifndef FALSE
#define FALSE ((boolean)0U)
#endif

#endif