/*
 * mem_replacements.h
 *
 *  Created on: Dec 29, 2022
 *      Author: veteran
 */

#ifndef INC_WAVES_MEM_REPLACEMENTS_H_
#define INC_WAVES_MEM_REPLACEMENTS_H_

#define NUM_SAMPLES 8192

#include "stddef.h"
#include "math.h"
#include "tx_api.h"
#include "NEDwaves_memlight_types.h"

int memory_pool_init(VOID* pool_start,UINT size);
void* malloc_replacement(size_t size);
void* calloc_replacement(size_t num, size_t size);
void free_replacement(void* ptr);

// NEDWaves support
// TODO: delete the two testing functions below
emxArray_real32_T *argInit_1xUnbounded_real32_T_down(void);
emxArray_real32_T *argInit_1xUnbounded_real32_T_north_east(void);

emxArray_real32_T *argInit_1xUnbounded_real32_T(float* data);
double argInit_real_T(void);


#endif /* INC_WAVES_MEM_REPLACEMENTS_H_ */
