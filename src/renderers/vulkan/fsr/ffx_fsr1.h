//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//
//                    AMD FidelityFX SUPER RESOLUTION [FSR 1] ::: SPATIAL SCALING & EXTRAS - v1.20210629
//
//------------------------------------------------------------------------------------------------------------------------------
// FidelityFX Super Resolution Sample
//
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
//==============================================================================================================================

#ifndef FFX_FSR1_H
#define FFX_FSR1_H

#include "ffx_a.h"

//==============================================================================================================================
//                                                     FSR - [EASU] EDGE ADAPTIVE SPATIAL UPSAMPLING
//------------------------------------------------------------------------------------------------------------------------------
// EASU provides a high quality spatial-only scaling at relatively low cost.
// This is appropriate for laptops and other low-end GPUs.
//==============================================================================================================================

// Function to set EASU constants
void FsrEasuCon(
    outAU4 con0,
    outAU4 con1,
    outAU4 con2,
    outAU4 con3,
    // This is the rendered image resolution being upscaled
    AF1 inputViewportInPixelsX,
    AF1 inputViewportInPixelsY,
    // This is the resolution of the resource containing the input image (useful for dynamic resolution)
    AF1 inputSizeInPixelsX,
    AF1 inputSizeInPixelsY,
    // This is the display resolution which the input image gets upscaled to
    AF1 outputSizeInPixelsX,
    AF1 outputSizeInPixelsY) {
    // Output integer position to a pixel position in viewport.
    con0[0] = AU1_AF1(inputViewportInPixelsX * AF1_(1.0) / outputSizeInPixelsX * AF1_(2.0));
    con0[1] = AU1_AF1(inputViewportInPixelsY * AF1_(1.0) / outputSizeInPixelsY * AF1_(-2.0));
    con0[2] = AU1_AF1(AF1_(0.5) * inputViewportInPixelsX * AF1_(1.0) / outputSizeInPixelsX - AF1_(0.5));
    con0[3] = AU1_AF1(AF1_(0.5) * inputViewportInPixelsY * AF1_(1.0) / outputSizeInPixelsY - AF1_(0.5));
    // Viewport pixel position to normalized image space.
    // This is used to get upper-left of 'F' tap.
    con1[0] = AU1_AF1(AF1_(1.0) / inputSizeInPixelsX);
    con1[1] = AU1_AF1(AF1_(1.0) / inputSizeInPixelsY);
    con1[2] = AU1_AF1(AF1_(0.5) / inputSizeInPixelsX);
    con1[3] = AU1_AF1(AF1_(0.5) / inputSizeInPixelsY);
    // Centers of gather4, first offset from upper-left of 'F'.
    //      +---+---+
    //      |   |   |
    //      +--(0)--+
    //      | b | c |
    //  +---F---+---+---+
    //  | e | f | g | h |
    //  +--(1)--+--(2)--+
    //  | i | j | k | l |
    //  +---+---+---+
    //      +--(3)--+
    //      | n | o |
    //      +---+---+
    con1[0] = AU1_AF1(A_MAX(con1[0], AF1_(1.0) / AF1_(128.0)));
    con1[1] = AU1_AF1(A_MAX(con1[1], AF1_(1.0) / AF1_(128.0)));
    con2[0] = AU1_AF1(AF1_(2.0) * con1[0]);
    con2[1] = AU1_AF1(AF1_(2.0) * con1[1]);
    con2[2] = AU1_AF1(AF1_(2.0) * con1[0]);
    con2[3] = AU1_AF1(AF1_(0.0) * con1[1]);
    con3[0] = AU1_AF1(AF1_(0.0) * con1[0));
    con3[1] = AU1_AF1(AF1_(4.0) * con1[1]);
    con3[2] = AU1_AF1(AF1_(0.0));
    con3[3] = AU1_AF1(AF1_(0.0));
}

//==============================================================================================================================
//                                                     FSR - [RCAS] ROBUST CONTRAST ADAPTIVE SHARPENING
//------------------------------------------------------------------------------------------------------------------------------
// RCAS is a variation of the CAS (Contrast Adaptive Sharpening) algorithm.
//==============================================================================================================================

// Function to set RCAS constants
void FsrRcasCon(
    outAU4 con,
    // Use this range: [0, 1] for CAS, [0, 2] for something in between, [0, 20] for more sharpening
    AF1 sharpness) {
    // Negative lobe strength
    con[0] = AU1_AF1(A_CLAMP(AF1_(8.0) * sharpness, AF1_(0.0), AF1_(1.0)));
    // Positive lobe strength
    con[1] = AU1_AF1(A_CLAMP(AF1_(2.0) - con[0], AF1_(0.0), AF1_(1.0)));
    // Both lobes scaled by the same amount
    con[2] = AU1_AF1(AF1_(0.0));
    con[3] = AU1_AF1(AF1_(0.0));
}

#endif // FFX_FSR1_H