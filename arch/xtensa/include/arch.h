/****************************************************************************
 * arch/xtensa/include/arch.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/* This file should never be included directly but, rather, only indirectly
 * through nuttx/arch.h
 */

#ifndef __ARCH_XTENSA_INCLUDE_ARCH_H
#define __ARCH_XTENSA_INCLUDE_ARCH_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#ifndef __ASSEMBLY__
#  include <stddef.h>
#  include <stdint.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

#ifdef CONFIG_XTENSA_IMEM_USE_SEPARATE_HEAP
struct mallinfo; /* Forward reference, see malloc.h */

/****************************************************************************
 * Name: xtensa_imm_initialize
 *
 * Description:
 *   Initialize the internal heap.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

void xtensa_imm_initialize(void);

/****************************************************************************
 * Name: xtensa_imm_malloc
 *
 * Description:
 *   Allocate memory from the internal heap.
 *
 * Input Parameters:
 *   size - Size (in bytes) of the memory region to be allocated.
 *
 * Return Value:
 *   Address of the allocated memory space. NULL, if allocation fails.
 *
 ****************************************************************************/

void *xtensa_imm_malloc(size_t size);

/****************************************************************************
 * Name: xtensa_imm_calloc
 *
 * Description:
 *   Calculates the size of the allocation and
 *   allocate memory the internal heap.
 *
 * Input Parameters:
 *   n         - Size (in types) of the memory region to be allocated.
 *   elem_size - Size (in bytes) of the type to be allocated.
 *
 * Return Value:
 *   Address of the allocated memory space. NULL, if allocation fails.
 *
 ****************************************************************************/

void *xtensa_imm_calloc(size_t n, size_t elem_size);

/****************************************************************************
 * Name: xtensa_imm_realloc
 *
 * Description:
 *   Reallocate memory from the internal heap.
 *
 * Input Parameters:
 *   ptr  - Address to be reallocate.
 *   size - Size (in bytes) to be reallocate.
 *
 * Return Value:
 *   Address of the possibly moved memory space. NULL, if allocation fails.
 *
 ****************************************************************************/

void *xtensa_imm_realloc(void *ptr, size_t size);

/****************************************************************************
 * Name: xtensa_imm_zalloc
 *
 * Description:
 *   Allocate and zero memory from the internal heap.
 *
 * Input Parameters:
 *   size - Size (in bytes) of the memory region to be allocated.
 *
 * Return Value:
 *   Address of the allocated memory space. NULL, if allocation fails.
 *
 ****************************************************************************/

void *xtensa_imm_zalloc(size_t size);

/****************************************************************************
 * Name: xtensa_imm_free
 *
 * Description:
 *   Free memory from the internal heap.
 *
 * Input Parameters:
 *   mem - Address to be freed.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

void xtensa_imm_free(void *mem);

/****************************************************************************
 * Name: xtensa_imm_memalign
 *
 * Description:
 *   memalign requests more than enough space from malloc, finds a region
 *   within that chunk that meets the alignment request and then frees any
 *   leading or trailing space.
 *
 *   The alignment argument must be a power of two (not checked).  8-byte
 *   alignment is guaranteed by normal malloc calls.
 *
 * Input Parameters:
 *   alignment - Requested alignment.
 *   size - Size (in bytes) of the memory region to be allocated.
 *
 * Return Value:
 *   Address of the allocated address. NULL, if allocation fails.
 *
 ****************************************************************************/

void *xtensa_imm_memalign(size_t alignment, size_t size);

/****************************************************************************
 * Name: xtensa_imm_heapmember
 *
 * Description:
 *   Check if an address lies in the internal heap.
 *
 * Input Parameters:
 *   mem - The address to check.
 *
 * Return Value:
 *   True if the address is a member of the internal heap. False if not.
 *
 ****************************************************************************/

bool xtensa_imm_heapmember(void *mem);

/****************************************************************************
 * Name: xtensa_imm_mallinfo
 *
 * Description:
 *   mallinfo returns a copy of updated current heap information for the
 *   user heap.
 *
 * Input Parameters:
 *   None.
 *
 * Return Value:
 *   info - Where memory information will be copied.
 *
 ****************************************************************************/

struct mallinfo xtensa_imm_mallinfo(void);
#endif

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __ARCH_XTENSA_INCLUDE_ARCH_H */
