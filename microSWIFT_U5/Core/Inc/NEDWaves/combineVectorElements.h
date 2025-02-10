/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only.  Not for
 * government, commercial, or other organizational use.
 * File: combineVectorElements.h
 *
 * MATLAB Coder version            : 5.4
 * C/C++ source code generated on  : 16-Oct-2023 17:01:43
 */

#ifndef COMBINEVECTORELEMENTS_H
#define COMBINEVECTORELEMENTS_H

/* Include Files */
#include "NEDWaves/NEDwaves_memlight_types.h"
#include "NEDWaves/rtwtypes.h"
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Function Declarations */
float b_combineVectorElements(const emxArray_real32_T *x);

float combineVectorElements(const emxArray_real32_T *x, int vlen);

#ifdef __cplusplus
}
#endif

#endif
/*
 * File trailer for combineVectorElements.h
 *
 * [EOF]
 */
