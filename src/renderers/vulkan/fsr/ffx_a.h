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

// Packing functions for half precision
#if defined(A_HALF)
  #if defined(A_GPU)
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
  #else
    typedef uint16_t AP1_AH1;
    typedef uint32_t AP1_AF1;
    typedef uint32_t AP1_AB1;
    typedef uint32_t AP1_AU1;
    typedef uint16_t AH1_AP1;
    typedef uint32_t AF1_AP1;
    typedef uint32_t AB1_AP1;
    typedef uint32_t AU1_AP1;
  #endif
#endif

// Basic types
#if defined(A_HALF)
  typedef uint16_t AH1;
  typedef uint32_t AF1;
  typedef uint32_t AB1;
  typedef uint32_t AU1;
#endif

// Math functions
#define A_ABS(a) ((a) < 0 ? -(a) : (a))
#define A_MAX(a,b) ((a) > (b) ? (a) : (b))
#define A_MIN(a,b) ((a) < (b) ? (a) : (b))
#define A_CLAMP(a,b,c) A_MIN(A_MAX(a,b),c)

// Common constants
#define A_2PI 6.28318530718f

#endif // FFX_A_H