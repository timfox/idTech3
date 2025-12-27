//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//
// AMD FidelityFX SUPER RESOLUTION [FSR 1] ::: COMMON - v1.20210629
//
//------------------------------------------------------------------------------------------------------------------------------
// This file contains common definitions and utility functions for FSR.
//==============================================================================================================================

#ifndef FFX_A_H
#define FFX_A_H

#define A_2PI 6.28318530718

#if !defined(A_GPU)
#include <stdint.h>

// Basic types for C
typedef uint32_t AU1;
typedef uint32_t AU4[4];
typedef uint32_t* outAU4;
typedef float AF1;
typedef float AF4[4];

#define AF1_(x) ((AF1)(x))
#define AU1_AF1(x) ((AU1)(x))

#if defined(A_HALF)
    typedef uint16_t AP1_AH1;
    typedef uint32_t AP1_AF1;
    typedef uint32_t AP1_AB1;
    typedef uint32_t AP1_AU1;
    typedef uint16_t AH1_AP1;
    typedef uint32_t AF1_AP1;
    typedef uint32_t AB1_AP1;
    typedef uint32_t AU1_AP1;
    
    typedef uint16_t AH1;
    typedef uint32_t AF1_TYPE; // Avoid conflict with AF1
    typedef uint32_t AB1;
    typedef uint32_t AU1_TYPE;
#endif

#else // A_GPU

#if defined(A_GLSL)
    #define AU1 uint
    #define AU4 uvec4
    #define outAU4 out uvec4
    #define AF1 float
    #define AF4 vec4
    #define AF1_(x) float(x)
    #define AU1_AF1(x) uint(x)
#endif

// Packing functions for half precision for GPU
#if defined(A_HALF)
    #ifdef A_HLSL
      #define AP1_AH1(x) asfloat(uint16_t(x))
      #define AP1_AF1(x) asfloat(uint32_t(x))
      #define AP1_AB1(x) asfloat(uint32_t(x))
      #define AP1_AU1(x) asuint(x)
      #define AH1_AP1(x) asuint16(x)
      #define AF1_AP1(x) asuint(x)
      #define AB1_AP1(x) asuint(x)
      #define AU1_AP1(x) asuint(x)
    #else
      #define AP1_AH1(x) __ushort_as_float(x)
      #define AP1_AF1(x) __uint_as_float(x)
      #define AP1_AB1(x) __uint_as_float(x)
      #define AP1_AU1(x) __float_as_uint(x)
      #define AH1_AP1(x) __float_as_ushort(x)
      #define AF1_AP1(x) __float_as_uint(x)
      #define AB1_AP1(x) __float_as_uint(x)
      #define AU1_AP1(x) __float_as_uint(x)
    #endif
#endif

#endif // A_GPU

// Math functions
#define A_ABS(a) ((a) < 0 ? -(a) : (a))
#define A_MAX(a,b) ((a) > (b) ? (a) : (b))
#define A_MIN(a,b) ((a) < (b) ? (a) : (b))
#define A_CLAMP(a,b,c) A_MIN(A_MAX(a,b),c)

#endif // FFX_A_H
