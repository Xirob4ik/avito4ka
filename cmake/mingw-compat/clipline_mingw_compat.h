// Force-included in the llvm-mingw cross build only (cmake/toolchains/llvm-mingw-x86_64.cmake).
// Supplies a few public Windows SDK constants that JUCE 8 uses and the mingw-w64 headers lack.
// Values are those documented by Microsoft (UIAutomationCore.h, d2d1effects.h, nserror.h).
#pragma once

#if defined (__MINGW32__) && defined (__cplusplus)
 // UIAutomationCore.h: enum CaretPosition
 #define CaretPosition_Unknown           0
 #define CaretPosition_EndOfLine         1
 #define CaretPosition_BeginningOfLine   2

 // d2d1effects.h: enum D2D1_SATURATION_PROP
 #define D2D1_SATURATION_PROP_SATURATION 0

 // nserror.h (identical definition, so including nserror.h later is harmless)
 #define NS_E_NO_MORE_SAMPLES _HRESULT_TYPEDEF_(0xC00D0BCF)
#endif
