/* date = March 25th 2021 0:38 pm */

#ifndef SYNTH_PLATFORM_H
#define SYNTH_PLATFORM_H

#include <stdint.h>
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usize;
typedef float f32;
typedef double f64;

#include <float.h>
#define F32_MAX FLT_MAX
#include <limits.h>
#define U16_MAX USHRT_MAX;
#define U32_MAX UINT_MAX;
#define U64_MAX ULONG_MAX;
#define I16_MAX SHRT_MAX;
#define I32_MAX INT_MAX;
#define I64_MAX LONG_MAX;

#define global static
#define local_static static
#define internal static
#define ArrayCount(arr) (sizeof((arr)) / (sizeof((arr)[0])))
#define Kilobytes(number) ((number)*1024ull)
#define Megabytes(number) (Kilobytes(number) * 1024ull)
#define Gigabytes(number) (Megabytes(number) * 1024ull)
#define Terabytes(number) (Gigabytes(number) * 1024ull)

#define not !
#define InsideOpen(v, a, b) ((v > a) && (v < b))
#define InsideClosed(v, a, b) ((v >= a) && (v <= b))
#define InsideClosedOpen(v, a, b) ((v >= a) && (v < b))
#define InsideOpenClosed(v, a, b) ((v > a) && (v <= b))
#define InsideUpto InsideClosedOpen
#define InsideDownto InsideOpenClosed
#define InsideExclusive InsideOpen
#define InsideInclusive InsideClosed
#define OutsideExlusive !InsideInclusive
#define OutsideInclusive !InsideExclusive

#ifdef SYNTH_SLOW
#define Assert(expression)                                                     \
if(!(expression)) {                                                          \
*(int *)0 = 0;                                                             \
}
#else
#define Assert(expression)
#endif

#define Unreachable Assert(!"Unreachable code")

inline f32
Log2f(f32 n)  
{
    return logf( n ) / logf( 2 );  
}

inline u32
RandomU32(u32 seed)
{
    local_static u32 z = 362436069;
    local_static u32 w = 521288629;
    local_static u32 jcong = 380116160;
    local_static u32 jsr = 123456789;
    u32 z_new = 36969 * ((z+seed) & 65535) + ((z+seed) >> 16);
    u32 w_new = 18000 * (w & 65535) + (w >> 16);
    u32 mwc = (z_new << 16) + w_new;
    u32 jcong_new = 69069 * jcong + 1234567;
    u32 jsr_new = jsr ^ (jsr << 17);
    jsr_new ^= (jsr >> 13);
    jsr_new ^= (jsr << 5);
    u32 result = (mwc ^ jcong_new) + jsr_new;
    z = z_new;
    w = w_new;
    jcong = jcong_new;
    jsr = jsr_new;
    return result;
}

inline f32
RandomF32(u32 seed)
{
    u32 val = RandomU32(seed);
    return (f32)val / (f32)U32_MAX;
}

#endif //SYNTH_PLATFORM_H
