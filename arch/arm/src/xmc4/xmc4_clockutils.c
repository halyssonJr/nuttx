/****************************************************************************
 * arch/arm/src/xmc4/xmc4_clockutils.c
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

/****************************************************************************
 * May include some logic from sample code provided by Infineon:
 *
 *   Copyright (C) 2011-2015 Infineon Technologies AG. All rights reserved.
 *
 * Infineon Technologies AG (Infineon) is supplying this software for use
 * with Infineon's microcontrollers.  This file can be freely distributed
 * within development tools that are supporting such microcontrollers.
 *
 * THIS SOFTWARE IS PROVIDED AS IS. NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 * OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS
 * SOFTWARE. INFINEON SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 ****************************************************************************/

/* Reference: XMC4500 Reference Manual V1.5 2014-07 Microcontrollers. */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include "arm_internal.h"
#include "hardware/xmc4_scu.h"
#include "xmc4_clockconfig.h"

#include <arch/board/board.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: xmc4_get_coreclock
 *
 * Description:
 *   Return the current core clock frequency (fCPU).
 *
 ****************************************************************************/

uint32_t xmc4_get_coreclock(void)
{
  uint32_t pdiv;
  uint32_t ndiv;
  uint32_t kdiv;
  uint32_t sysdiv;
  uint32_t regval;
  uint32_t temp;

  if ((getreg32(XMC4_SCU_SYSCLKCR) & SCU_SYSCLKCR_SYSSEL) != 0)
    {
      /* fPLL is clock source for fSYS */

      if ((getreg32(XMC4_SCU_PLLCON2) & SCU_PLLCON2_PINSEL) != 0)
        {
          /* PLL input clock is the backup clock (fOFI) */

          temp = OFI_FREQUENCY;
        }
      else
        {
          /* PLL input clock is the high performance oscillator (fOSCHP);
           * Only board specific logic knows this value.
           */

          temp = BOARD_XTAL_FREQUENCY;
        }

      /* Check if PLL is locked */

      regval = getreg32(XMC4_SCU_PLLSTAT);
      if ((regval & SCU_PLLSTAT_VCOLOCK) != 0)
        {
          /* PLL normal mode */

          regval = getreg32(XMC4_SCU_PLLCON1);
          pdiv   = ((regval & SCU_PLLCON1_PDIV_MASK) >>
                     SCU_PLLCON1_PDIV_SHIFT) + 1;
          ndiv   = ((regval & SCU_PLLCON1_NDIV_MASK) >>
                     SCU_PLLCON1_NDIV_SHIFT) + 1;
          kdiv   = ((regval & SCU_PLLCON1_K2DIV_MASK) >>
                     SCU_PLLCON1_K2DIV_SHIFT) + 1;

          temp   = (temp / (pdiv * kdiv)) * ndiv;
        }
      else
        {
          /* PLL prescalar mode */

          regval = getreg32(XMC4_SCU_PLLCON1);
          kdiv   = ((regval & SCU_PLLCON1_K1DIV_MASK) >>
                     SCU_PLLCON1_K1DIV_SHIFT) + 1;

          temp   = (temp / kdiv);
        }
    }
  else
    {
      /* fOFI is clock source for fSYS */

      temp = OFI_FREQUENCY;
    }

  /* Divide by SYSDIV to get fSYS */

  regval = getreg32(XMC4_SCU_SYSCLKCR);
  sysdiv = ((regval & SCU_SYSCLKCR_SYSDIV_MASK) >>
             SCU_SYSCLKCR_SYSDIV_SHIFT) + 1;
  temp   = temp / sysdiv;

  /* Check if the fSYS clock is divided by two to produce fCPU clock. */

  regval = getreg32(XMC4_SCU_CPUCLKCR);
  if ((regval & SCU_CPUCLKCR_CPUDIV) != 0)
    {
      temp = temp >> 1;
    }

  return temp;
}

/****************************************************************************
 * Name: xmc4_get_periphclock
 *
 * Description:
 *   The peripheral clock is either fCPU or fCPU/2, depending on the state
 *   of the peripheral divider.
 *
 ****************************************************************************/

uint32_t xmc4_get_periphclock(void)
{
  uint32_t periphclock;
  uint32_t regval;

  /* Get the CPU clock frequency.  Unless it is divided down, this also the
   * peripheral clock frequency.
   */

  periphclock = xmc4_get_coreclock();

  /* Get the peripheral clock divider */

  regval = getreg32(XMC4_SCU_PBCLKCR);
  if ((regval & SCU_PBCLKCR_PBDIV) != 0)
    {
      /* The peripheral clock is fCPU/2 */

      periphclock >>= 1;
    }

  return periphclock;
}

/****************************************************************************
 * Name: xmc4_get_ccuclock
 *
 * Description:
 *   The ccu clock is either fCPU or fCPU/2, depending on the state
 *   of the peripheral divider.
 *
 ****************************************************************************/

uint32_t xmc4_get_ccuclock(void)
{
  uint32_t f_cpu = xmc4_get_coreclock();
  uint32_t f_ccu =
    f_cpu >> ((uint32_t)(getreg32(XMC4_SCU_CCUCLKCR) & SCU_CCUCLKCR_CCUDIV));
  return  f_ccu;
}
