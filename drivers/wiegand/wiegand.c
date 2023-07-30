/****************************************************************************
 * drivers/wiegand/wiegand.c
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
#include <sys/types.h>
#include <assert.h>
#include <debug.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>
#include <nuttx/mutex.h>
#include <nuttx/clock.h>
#include <nuttx/signal.h>

#include <nuttx/wiegand/wiegand.h>

#define WIEGAND_TIMEOUT 50
struct wiegand_dev_s
{
    FAR struct wiegand_config_s *config;
    mutex_t devlock;
    int bit_index;
    uint8_t buffer[4];
};

static int wiegand_open(FAR struct file *filep);
static int wiegand_close(FAR struct file *filep);
static ssize_t wiegand_read(FAR struct file *filep, FAR char *buffer,
                            size_t buflen);
static const struct file_operations g_wiegandops =
{
    wiegand_open,    /* open */
    wiegand_close,   /* close */
    wiegand_read,    /* read */
    NULL,           /* write */   
};

static int wiegand_open(FAR struct file *filep)
{
    FAR struct inode *inode = filep->f_inode;
    FAR struct wiegand_dev_s *priv = inode->i_private;
    int ret;

    ret = nxmutex_lock(&priv->devlock);
    if(ret <0)
    {
        return ret;
    }
    nxmutex_unlock(&priv->devlock);
    return OK;
}

static int wiegand_close(FAR struct file *filep)
{
    FAR struct inode *inode = filep->f_inode;
    FAR struct wiegand_dev_s *priv = inode->i_private;
    int ret;

    ret = nxmutex_lock(&priv->devlock);
    if(ret <0)
    {
        return ret;
    }
    nxmutex_unlock(&priv->devlock);
    return OK;
}


static void wiegand_start_read_signal(FAR struct wiegand_dev_s *priv)
{
    clock_t timeout;
    clock_t start;
    clock_t elapsed;
    
    int ret;
    bool data0 ;
    bool data1 ;
    int index;
    uint8_t raw_data;

    timeout = MSEC2TICK(WIEGAND_TIMEOUT);
    
    start = clock_systime_ticks();
    
    do
    {
        data0 = priv->config->get_data(priv->config,0);
        data1 = priv->config->get_data(priv->config,1);
        
        if(data0 != data1)
        {
            raw_data = (data0? 1 : 0) << (priv->bit_index%8);
            priv->buffer[priv->bit_index/8] |= raw_data;
            priv->bit_index++;

        }

        elapsed = clock_systime_ticks() - start;
      
    } while (elapsed < timeout);
    
    return 0;
}

static ssize_t wiegand_read(FAR struct file *filep, FAR char *buffer,
                            size_t buflen)
{
    FAR struct inode  *inode = filep->f_inode;
    FAR struct wiegand_dev_s *priv = inode->i_private;
    int ret;

    ret = nxmutex_lock(&priv->devlock);
    if(ret <0)
    {
        return (ssize_t)ret;
    }
    wiegand_start_read_signal(priv);

    
    nxmutex_unlock(&priv->devlock);
    
    return ret;
}

int wiegand_register(FAR const char  *devpath,
                     FAR struct wiegand_config_s *config)
{
    int ret = 0;
    FAR struct wiegand_dev_s *priv;

    priv = (FAR struct wiegand_dev_s *)kmm_malloc(sizeof(struct wiegand_dev_s));
    if(priv == NULL)
    {
        snerr("ERROR: Failed to allocate instance\n");
        return - ENOMEM;
    }
    priv->config = config;

    nxmutex_init(&priv->devlock);

    ret = register_driver(devpath,&g_wiegandops,0666,priv);
    if(ret<0)
    {
       nxmutex_destroy(&priv->devlock);
       kmm_free(priv);
       snerr("ERROR: Failed to register driver: %d\n", ret); 
    }

    return ret;
}