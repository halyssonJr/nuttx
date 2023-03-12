/****************************************************************************
 * boards/xtensa/esp32/esp32-devkitc/src/esp32_wiegand.c
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
#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <nuttx/irq.h>
#include <arch/irq.h>

#include "esp32_gpio.h"

#include "esp32-devkitc.h"

#include <nuttx/wiegand/wiegand.h>



#define GPIO_DATA_0 22
#define GPIO_DATA_1 23


struct esp32_wiegand_config_s
{
  struct wiegand_config_s config;
  void *arg;
  xcpt_t isr;
};

int gpio_data [2] =
{
  GPIO_DATA_0,
  GPIO_DATA_1
};

static int wiegand_irq_attach(struct wiegand_config_s *dev,xcpt_t isr,
                              void *arg);
static void wiegand_irq_enable(struct wiegand_config_s *dev, bool enable);
static bool wiegand_read_data(const struct wiegand_config_s *dev, int index);

// static struct esp32_wiegand_config_s g_wiegand_config=
// {
//     .config = 
//     {
//         .gpint_attach = wiegand_irq_read,
//         .gpint_enable = wiegand_irq_attach,
//         .gpint_read   = wiegand_irq_enable,  
//     },
// };
static bool wiegand_read_data(const struct wiegand_config_s *dev, int index)
{
  return esp32_gpioread(gpio_data[index]);
}

static void wiegand_irq_enable(struct wiegand_config_s *dev, bool enable)
{
  struct esp32_wiegand_config_s *priv = 
                (struct esp32_wiegand_config_s *)dev;
  int irq[2];

  irq[0] = ESP32_PIN2IRQ(gpio_data[0]);
  irq[1] = ESP32_PIN2IRQ(gpio_data[1]);
  
  if( priv->isr != NULL)
  {  
    if(enable)
    {
      syslog(LOG_DEBUG, "DEBUG: enable interrupt falling edge");
      esp32_gpioirqenable(irq[1], FALLING);
      esp32_gpioirqenable(irq[1], FALLING);
    }
    else
    {
      syslog(LOG_DEBUG, "DEBUG: disable ");
      esp32_gpioirqdisable(irq[0]);
      esp32_gpioirqdisable(irq[1]);
    }
  }
}

static int wiegand_irq_attach(struct wiegand_config_s *dev, xcpt_t isr,
                              void *arg)
{
  struct esp32_wiegand_config_s *priv =
                (struct esp32_wiegand_config_s *)dev;
  
  int ret;
  int irq[2];
  
  irq[0] = ESP32_PIN2IRQ(gpio_data[0]);
  irq[1] = ESP32_PIN2IRQ(gpio_data[1]);
  
  if(isr != NULL)
  {
    esp32_gpioirqdisable(irq[0]);
    esp32_gpioirqdisable(irq[1]);
    
    ret = irq_attach(irq[0],isr,arg);
    if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: irq_attach() gpio DATA 1 failed: %d\n", ret);
      return ret;
    }
    
    ret = irq_attach(irq[1],isr,arg);
    if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: irq_attach() gpio DATA 1 failed: %d\n", ret);
      return ret;
    }
  }
  else 
  {
    gpioinfo("Disable the interrupt\n");
    esp32_gpioirqdisable(irq[0]);
    esp32_gpioirqdisable(irq[1]);

  }
  priv->isr = isr;
  priv->arg = arg;

  return OK;
}