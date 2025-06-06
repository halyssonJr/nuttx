/****************************************************************************
 * drivers/power/pm/pm_register.c
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
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>

#include <nuttx/init.h>
#include <nuttx/queue.h>
#include <nuttx/power/pm.h>

#include "pm.h"

#ifdef CONFIG_PM

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pm_domain_register
 *
 * Description:
 *   This function is called by a device driver in order to register to
 *   receive power management event callbacks.
 *
 * Input Parameters:
 *   domain - Target register domain.
 *   cb     - An instance of struct pm_callback_s providing the driver
 *               callback functions.
 *
 * Returned Value:
 *    Zero (OK) on success; otherwise a negated errno value is returned.
 *
 ****************************************************************************/

int pm_domain_register(int domain, FAR struct pm_callback_s *cb)
{
  FAR struct pm_domain_s *pdom;
  irqstate_t flags;

  DEBUGASSERT(domain >= 0 && domain < CONFIG_PM_NDOMAINS);

  pdom  = &g_pmdomains[domain];
  flags = spin_lock_irqsave(&pdom->lock);

  /* Add the new entry to the end of the list of registered callbacks */

  dq_addlast(&cb->entry, &pdom->registry);
#if defined (CONFIG_PM_PROCFS)
  cb->preparefail.state = PM_RESTORE;
#endif
  spin_unlock_irqrestore(&pdom->lock, flags);
  return OK;
}

#endif /* CONFIG_PM */
