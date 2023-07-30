/****************************************************************************
 * include/nuttx/wiegand/wiegand.h
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

#ifndef __INCLUDE_NUTTX_WIEGAND_WIEGAND_H
#define __INCLUDE_NUTTX_WIEGAND_WIEGAND_H

#include <nuttx/irq.h>

struct wiegand_config_s
{
    CODE int (*irq_attach)(FAR struct wiegand_config_s *dev,xcpt_t isr,
                            FAR void *arg);
    CODE void (*irq_enable)(FAR struct wiegand_config_s *dev,bool enable);
    CODE bool (*get_data)(FAR const struct wiegand_config_s *dev,int index);
    
};

int wiegand_register(FAR const char  *devpath, FAR struct wiegand_config_s *config);
#endif /* __INCLUDE_NUTTX_WIEGAND_WIEGAND_H */