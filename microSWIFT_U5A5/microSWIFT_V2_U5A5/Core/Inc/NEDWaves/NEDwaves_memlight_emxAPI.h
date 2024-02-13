/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only.  Not for
 * government, commercial, or other organizational use.
 * File: NEDwaves_memlight_emxAPI.h
 *
 * MATLAB Coder version            : 5.4
 * C/C++ source code generated on  : 16-Oct-2023 17:01:43
 */

#ifndef NEDWAVES_MEMLIGHT_EMXAPI_H
#define NEDWAVES_MEMLIGHT_EMXAPI_H

/* Include Files */
#include "NEDWaves/NEDwaves_memlight_types.h"
#include "NEDWaves/rtwtypes.h"
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Function Declarations */
extern emxArray_real32_T *emxCreateND_real32_T(int numDimensions,
                                               const int *size);

extern emxArray_real32_T *
emxCreateWrapperND_real32_T(float *data, int numDimensions, const int *size);

extern emxArray_real32_T *emxCreateWrapper_real32_T(float *data, int rows,
                                                    int cols);

extern emxArray_real32_T *emxCreate_real32_T(int rows, int cols);

extern void emxDestroyArray_real32_T(emxArray_real32_T *emxArray);

extern void emxInitArray_real32_T(emxArray_real32_T **pEmxArray,
                                  int numDimensions);

#ifdef __cplusplus
}
#endif

#endif
/*
 * File trailer for NEDwaves_memlight_emxAPI.h
 *
 * [EOF]
 */
