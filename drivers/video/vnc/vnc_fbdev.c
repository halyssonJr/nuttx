/****************************************************************************
 * drivers/video/vnc/vnc_fbdev.c
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#if defined(CONFIG_VNCSERVER_DEBUG) && !defined(CONFIG_DEBUG_GRAPHICS)
#  undef  CONFIG_DEBUG_ERROR
#  undef  CONFIG_DEBUG_WARN
#  undef  CONFIG_DEBUG_INFO
#  undef  CONFIG_DEBUG_GRAPHICS_ERROR
#  undef  CONFIG_DEBUG_GRAPHICS_WARN
#  undef  CONFIG_DEBUG_GRAPHICS_INFO
#  define CONFIG_DEBUG_ERROR          1
#  define CONFIG_DEBUG_WARN           1
#  define CONFIG_DEBUG_INFO           1
#  define CONFIG_DEBUG_GRAPHICS       1
#  define CONFIG_DEBUG_GRAPHICS_ERROR 1
#  define CONFIG_DEBUG_GRAPHICS_WARN  1
#  define CONFIG_DEBUG_GRAPHICS_INFO  1
#endif
#include <debug.h>

#include <nuttx/kthread.h>
#include <nuttx/video/fb.h>
#include <nuttx/video/vnc.h>
#include <nuttx/input/touchscreen.h>

#include "vnc_server.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* This structure provides the frame buffer interface and also incapulates
 * information about the frame buffer instances for each display.
 */

struct vnc_fbinfo_s
{
  /* The publicly visible frame buffer interface.  This must appear first
   * so that struct vnc_fbinfo_s is cast compatible with struct fb_vtable_s.
   */

  struct fb_vtable_s vtable;

  /* Our private per-display information */

  bool initialized;           /* True: This instance has been initialized */
  uint8_t display;            /* Display number */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Get information about the video controller configuration and the
 * configuration of each color plane.
 */

static int up_getvideoinfo(FAR struct fb_vtable_s *vtable,
                           FAR struct fb_videoinfo_s *vinfo);
static int up_getplaneinfo(FAR struct fb_vtable_s *vtable, int planeno,
                           FAR struct fb_planeinfo_s *pinfo);

/* The following are provided only if the video hardware supports RGB color
 * mapping.
 */

#ifdef CONFIG_FB_CMAP
static int up_getcmap(FAR struct fb_vtable_s *vtable,
                      FAR struct fb_cmap_s *cmap);
static int up_putcmap(FAR struct fb_vtable_s *vtable,
                      FAR const struct fb_cmap_s *cmap);
#endif

/* The following are provided only if the video hardware supports a hardware
 * cursor.
 */

#ifdef CONFIG_FB_HWCURSOR
static int up_getcursor(FAR struct fb_vtable_s *vtable,
                        FAR struct fb_cursorattrib_s *attrib);
static int up_setcursor(FAR struct fb_vtable_s *vtable,
                        FAR struct fb_setcursor_s *settings);
#endif

/* Update the host window when there is a change to the framebuffer */

static int up_updateearea(FAR struct fb_vtable_s *vtable,
                          FAR const struct fb_area_s *area);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Current cursor position */

#ifdef CONFIG_FB_HWCURSOR
static struct fb_cursorpos_s g_cpos;

/* Current cursor size */

#ifdef CONFIG_FB_HWCURSORSIZE
static struct fb_cursorsize_s g_csize;
#endif
#endif

/* The framebuffer objects, one for each configured display. */

static struct vnc_fbinfo_s g_fbinfo[RFB_MAX_DISPLAYS];

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* Used to synchronize the server thread with the framebuffer driver.
 * NOTE:  This depends on the fact that all zero is correct initial state
 * for the semaphores.
 */

struct fb_startup_s g_fbstartup[RFB_MAX_DISPLAYS];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_getvideoinfo
 ****************************************************************************/

static int up_getvideoinfo(FAR struct fb_vtable_s *vtable,
                           FAR struct fb_videoinfo_s *vinfo)
{
  FAR struct vnc_fbinfo_s *fbinfo = (FAR struct vnc_fbinfo_s *)vtable;
  FAR struct vnc_session_s *session;

  ginfo("vtable=%p vinfo=%p\n", vtable, vinfo);

  DEBUGASSERT(fbinfo != NULL && vinfo != NULL);
  if (fbinfo != NULL && vinfo != NULL)
    {
      DEBUGASSERT(fbinfo->display >= 0 &&
                  fbinfo->display < RFB_MAX_DISPLAYS);
      session = g_vnc_sessions[fbinfo->display];

      if (session == NULL)
        {
          gerr("ERROR: session is not connected\n");
          return -ENOTCONN;
        }

      /* Return the requested video info.  We are committed to using the
       * configured color format in the framebuffer, but performing color
       * conversions on the fly for the remote framebuffer as necessary.
       */

      vinfo->fmt     = RFB_COLORFMT;
      vinfo->xres    = CONFIG_VNCSERVER_SCREENWIDTH;
      vinfo->yres    = CONFIG_VNCSERVER_SCREENHEIGHT;
      vinfo->nplanes = 1;

      return OK;
    }

  gerr("ERROR: Invalid arguments\n");
  return -EINVAL;
}

/****************************************************************************
 * Name: up_getplaneinfo
 ****************************************************************************/

static int up_getplaneinfo(FAR struct fb_vtable_s *vtable, int planeno,
                           FAR struct fb_planeinfo_s *pinfo)
{
  FAR struct vnc_fbinfo_s *fbinfo = (FAR struct vnc_fbinfo_s *)vtable;
  FAR struct vnc_session_s *session;

  ginfo("vtable=%p planeno=%d pinfo=%p\n", vtable, planeno, pinfo);

  DEBUGASSERT(fbinfo != NULL && pinfo != NULL && planeno == 0);
  if (fbinfo != NULL && pinfo != NULL && planeno == 0)
    {
      DEBUGASSERT(fbinfo->display >= 0 &&
                  fbinfo->display < RFB_MAX_DISPLAYS);
      session = g_vnc_sessions[fbinfo->display];

      if (session == NULL)
        {
          gerr("ERROR: session is not connected\n");
          return -ENOTCONN;
        }

      DEBUGASSERT(session->fb != NULL);

      /* Return the requested plane info.  We are committed to using the
       * configured bits-per-pixels in the framebuffer, but performing color
       * conversions on the fly for the remote framebuffer as necessary.
       */

      pinfo->fbmem    = (FAR void *)session->fb;
      pinfo->fblen    = RFB_SIZE;
      pinfo->stride   = RFB_STRIDE;
      pinfo->display  = fbinfo->display;
      pinfo->bpp      = RFB_BITSPERPIXEL;

      return OK;
    }

  gerr("ERROR: Returning EINVAL\n");
  return -EINVAL;
}

/****************************************************************************
 * Name: up_getcmap
 ****************************************************************************/

#ifdef CONFIG_FB_CMAP
static int up_getcmap(FAR struct fb_vtable_s *vtable,
                      FAR struct fb_cmap_s *cmap)
{
  FAR struct vnc_fbinfo_s *fbinfo = (FAR struct vnc_fbinfo_s *)vtable;
  FAR struct vnc_session_s *session;
  int i;

  ginfo("vtable=%p cmap=%p\n", vtable, cmap);

  DEBUGASSERT(fbinfo != NULL && cmap != NULL);

  if (fbinfo != NULL && cmap != NULL)
    {
      DEBUGASSERT(fbinfo->display >= 0 &&
                  fbinfo->display < RFB_MAX_DISPLAYS);
      session = g_vnc_sessions[fbinfo->display];

      if (session == NULL || session->state != VNCSERVER_RUNNING)
        {
          gerr("ERROR: session is not connected\n");
          return -ENOTCONN;
        }

      ginfo("first=%d len=%d\n", vcmap->first, cmap->len);
#warning Missing logic

      return OK;
    }

  gerr("ERROR: Returning EINVAL\n");
  return -EINVAL;
}
#endif

/****************************************************************************
 * Name: up_putcmap
 ****************************************************************************/

#ifdef CONFIG_FB_CMAP
static int up_putcmap(FAR struct fb_vtable_s *vtable,
                      FAR const struct fb_cmap_s *cmap)
{
  FAR struct vnc_fbinfo_s *fbinfo = (FAR struct vnc_fbinfo_s *)vtable;
  FAR struct vnc_session_s *session;
  int i;

  ginfo("vtable=%p cmap=%p\n", vtable, cmap);

  DEBUGASSERT(fbinfo != NULL && cmap != NULL);

  if (fbinfo != NULL && cmap != NULL)
    {
      DEBUGASSERT(fbinfo->display >= 0 &&
                  fbinfo->display < RFB_MAX_DISPLAYS);
      session = g_vnc_sessions[fbinfo->display];

      if (session == NULL || session->state != VNCSERVER_RUNNING)
        {
          gerr("ERROR: session is not connected\n");
          return -ENOTCONN;
        }

      ginfo("first=%d len=%d\n", vcmap->first, cmap->len);
#warning Missing logic

      return OK;
    }

  gerr("ERROR: Returning EINVAL\n");
  return -EINVAL;
}
#endif

/****************************************************************************
 * Name: up_getcursor
 ****************************************************************************/

#ifdef CONFIG_FB_HWCURSOR
static int up_getcursor(FAR struct fb_vtable_s *vtable,
                        FAR struct fb_cursorattrib_s *attrib)
{
  FAR struct vnc_fbinfo_s *fbinfo = (FAR struct vnc_fbinfo_s *)vtable;
  FAR struct vnc_session_s *session;
  int i;

  ginfo("vtable=%p attrib=%p\n", vtable, attrib);

  DEBUGASSERT(fbinfo != NULL && attrib != NULL);

  if (fbinfo != NULL && attrib != NULL)
    {
      DEBUGASSERT(fbinfo->display >= 0 &&
                  fbinfo->display < RFB_MAX_DISPLAYS);
      session = g_vnc_sessions[fbinfo->display];

      if (session == NULL || session->state != VNCSERVER_RUNNING)
        {
          gerr("ERROR: session is not connected\n");
          return -ENOTCONN;
        }

#warning Missing logic

      return OK;
    }

  gerr("ERROR: Returning EINVAL\n");
  return -EINVAL;
}
#endif

/****************************************************************************
 * Name: up_setcursor
 ****************************************************************************/

#ifdef CONFIG_FB_HWCURSOR
static int up_setcursor(FAR struct fb_vtable_s *vtable,
                       FAR struct fb_setcursor_s *settings)
{
  FAR struct vnc_fbinfo_s *fbinfo = (FAR struct vnc_fbinfo_s *)vtable;
  FAR struct vnc_session_s *session;
  int i;

  ginfo("vtable=%p settings=%p\n", vtable, settings);

  DEBUGASSERT(fbinfo != NULL && settings != NULL);

  if (fbinfo != NULL && settings != NULL)
    {
      DEBUGASSERT(fbinfo->display >= 0 &&
                  fbinfo->display < RFB_MAX_DISPLAYS);
      session = g_vnc_sessions[fbinfo->display];

      if (session == NULL || session->state != VNCSERVER_RUNNING)
        {
          gerr("ERROR: session is not connected\n");
          return -ENOTCONN;
        }

      ginfo("flags:   %02x\n", settings->flags);
      if ((settings->flags & FB_CUR_SETPOSITION) != 0)
        {
#warning Missing logic
        }

#ifdef CONFIG_FB_HWCURSORSIZE
      if ((settings->flags & FB_CUR_SETSIZE) != 0)
        {
#warning Missing logic
        }
#endif

#ifdef CONFIG_FB_HWCURSORIMAGE
      if ((settings->flags & FB_CUR_SETIMAGE) != 0)
        {
#warning Missing logic
        }
#endif

      return OK;
    }

  gerr("ERROR: Returning EINVAL\n");
  return -EINVAL;
}
#endif

/****************************************************************************
 * Name: up_updateearea
 ****************************************************************************/

static int up_updateearea(FAR struct fb_vtable_s *vtable,
                          FAR const struct fb_area_s *area)
{
  FAR struct vnc_fbinfo_s *fbinfo = (FAR struct vnc_fbinfo_s *)vtable;
  FAR struct vnc_session_s *session;
  int ret = OK;

  DEBUGASSERT(fbinfo != NULL && area != NULL);

  /* Recover the session information from the display number in the planeinfo
   * structure.
   */

  DEBUGASSERT(fbinfo->display >= 0 && fbinfo->display < RFB_MAX_DISPLAYS);
  session = g_vnc_sessions[fbinfo->display];

  /* Verify that the session is still valid */

  if (session != NULL && session->state == VNCSERVER_RUNNING)
    {
      /* Queue the rectangular update */

      ret = vnc_update_rectangle(session, area, true);
      if (ret < 0)
        {
          gerr("ERROR: vnc_update_rectangle failed: %d\n", ret);
        }
    }

  return ret;
}

#ifdef CONFIG_FB_SYNC

/****************************************************************************
 * Name: up_waitforsync
 ****************************************************************************/

static int up_waitforsync(FAR struct fb_vtable_s *vtable)
{
  FAR struct vnc_fbinfo_s *fbinfo = (FAR struct vnc_fbinfo_s *)vtable;
  FAR struct vnc_session_s *session;

  DEBUGASSERT(fbinfo);
  DEBUGASSERT(fbinfo->display >= 0 && fbinfo->display < RFB_MAX_DISPLAYS);

  session = g_vnc_sessions[fbinfo->display];

  if (session && session->state == VNCSERVER_RUNNING)
    {
      return nxsem_wait(&session->vsyncsem);
    }

  return ERROR;
}
#endif

/****************************************************************************
 * Name: vnc_start_server
 *
 * Description:
 *   Start the VNC server.
 *
 * Input Parameters:
 *   display - In the case of hardware with multiple displays, this
 *     specifies the display.  Normally this is zero.
 *
 * Returned Value:
 *   Zero is returned on success; a negated errno value is returned on any
 *   failure.
 *
 ****************************************************************************/

static int vnc_start_server(int display)
{
  FAR char *argv[2];
  char str[8];
  int ret;

  DEBUGASSERT(display >= 0 && display < RFB_MAX_DISPLAYS);

  /* Check if the server is already running */

  if (g_vnc_sessions[display] != NULL)
    {
      DEBUGASSERT(g_vnc_sessions[display]->state >= VNCSERVER_INITIALIZED);
      return OK;
    }

  /* Start the VNC server kernel thread. */

  ginfo("Starting the VNC server for display %d\n", display);

  g_fbstartup[display].result = -EBUSY;
  nxsem_reset(&g_fbstartup[display].fbinit, 0);

  /* Format the kernel thread arguments (ASCII.. yech) */

  itoa(display, str, 10);
  argv[0] = str;
  argv[1] = NULL;

  ret = kthread_create("vnc_server", CONFIG_VNCSERVER_PRIO,
                       CONFIG_VNCSERVER_STACKSIZE,
                       vnc_server, argv);
  if (ret < 0)
    {
      gerr("ERROR: Failed to start the VNC server: %d\n", ret);
      return ret;
    }

  return OK;
}

/****************************************************************************
 * Name: vnc_wait_start
 *
 * Description:
 *   Wait for the server to be started.
 *
 * Input Parameters:
 *   display - In the case of hardware with multiple displays, this
 *     specifies the display.  Normally this is zero.
 *
 * Returned Value:
 *   Zero is returned on success; a negated errno value is returned on any
 *   failure.
 *
 ****************************************************************************/

static inline int vnc_wait_start(int display)
{
  int ret;

  /* Check if there has been a session allocated yet.  This is one of the
   * first things that the VNC server will do with the kernel thread is
   * started.  But we might be here before the thread has gotten that far.
   *
   * If it has been allocated, then wait until it is in the INITIALIZED
   * state.  The INITIALIZED states indicates that the session structure
   * has been allocated and fully initialized.
   */

  while (g_vnc_sessions[display] == NULL ||
         g_vnc_sessions[display]->state == VNCSERVER_UNINITIALIZED)
    {
      /* The server is not yet running.  Wait for the server to post the FB
       * semaphore.  In certain error situations, the server may post the
       * semaphore, then reset it to zero.  There are are certainly race
       * conditions here, but I think none that are fatal.
       */

      ret = nxsem_wait_uninterruptible(&g_fbstartup[display].fbinit);
      if (ret < 0)
        {
          return ret;
        }
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: vnc_fbinitialize
 *
 * Description:
 *   Initialize the VNC frame buffer driver.  The VNC frame buffer driver
 *   supports two initialization interfaces:  The standard up_fbinitialize()
 *   that will be called from the graphics layer and this special
 *   initialization function that can be used only by VNC aware OS logic.
 *
 *   The two initialization functions may be called separated or together in
 *   either order.  The difference is that standard up_fbinitialize(), if
 *   used by itself, will not have any remote mouse or keyboard inputs that
 *   are reported to the VNC framebuffer driver from the remote VNC client.
 *
 *   In the standard graphics architecture, the keyboard/mouse inputs are
 *   received by some application/board specific logic at the highest level
 *   in the architecture via input drivers.  The received keyboard/mouse
 *   input data must then be "injected" into NX where it can they can be
 *   assigned to the window that has focus.  They will eventually be
 *   received by the Window instances via NX callback methods.
 *
 *   NX is a middleware layer in the architecture, below the
 *   application/board specific logic but above the driver level logic.  The
 *   frame buffer driver, on the other hand lies at the very lowest level in
 *   the graphics architecture.  It cannot call upward into the application
 *   nor can it call upward into NX.  So, some other logic.
 *
 *   vnc_fbinitialize() provides an optional, alternative initialization
 *   function.  It is optional because it need not be called.  If it is not
 *   called, however, keyboard/mouse inputs from the remote VNC client will
 *   be lost.  By calling vnc_fbinitialize(), you can provide callout
 *   functions that can be received by logic higher in the architecture.
 *
 * Input Parameters:
 *   display - In the case of hardware with multiple displays, this
 *     specifies the display.  Normally this is zero.
 *   kbdout - If non-NULL, then the pointed-to function will be called to
 *     handle all keyboard input as it is received.  This may be either raw,
 *     ASCII keypress input or encoded keyboard input as determined by
 *     CONFIG_VNCSERVER_KBDENCODE.  See include/nuttx/input/kbd_codec.h.
 *   mouseout - If non-NULL, then the pointed-to function will be called to
 *     handle all mouse input as it is received.
 *   arg - An opaque user provided argument that will be provided when the
 *    callouts are performed.
 *
 * Returned Value:
 *   Zero (OK) is returned on success.  Otherwise, a negated errno value is
 *   returned to indicate the nature of the failure.
 *
 ****************************************************************************/

int vnc_fbinitialize(int display, vnc_kbdout_t kbdout,
                     vnc_mouseout_t mouseout, FAR void *arg)
{
  FAR struct vnc_session_s *session;
  int ret;

  DEBUGASSERT(display >= 0 && display < RFB_MAX_DISPLAYS);

  /* Start the VNC server kernel thread. */

  ret = vnc_start_server(display);
  if (ret < 0)
    {
      gerr("ERROR: vnc_start_server() failed: %d\n", ret);
      return ret;
    }

  /* Wait for the VNC server to start and complete initialization. */

  ret = vnc_wait_start(display);
  if (ret < 0)
    {
      gerr("ERROR: vnc_wait_start() failed: %d\n", ret);
      return ret;
    }

  /* Save the input callout function information in the session structure. */

  session           = g_vnc_sessions[display];
  DEBUGASSERT(session != NULL);

  session->kbdout   = kbdout;
  session->mouseout = mouseout;
  session->arg      = arg;

  return OK;
}

/****************************************************************************
 * Name: vnc_fb_register
 *
 * Description:
 *   Register the framebuffer support for the specified display.
 *
 * Input Parameters:
 *   display - The display number for the case of boards supporting multiple
 *             displays or for hardware that supports multiple
 *             layers (each layer is consider a display).  Typically zero.
 *
 * Returned Value:
 *   Zero (OK) is returned success; a negated errno value is returned on any
 *   failure.
 *
 ****************************************************************************/

int vnc_fb_register(int display)
{
  FAR struct fb_vtable_s *vtable;
  FAR struct vnc_session_s *session;
  FAR struct vnc_fbinfo_s *fbinfo;
#if defined(CONFIG_VNCSERVER_TOUCH) || defined(CONFIG_VNCSERVER_KBD)
  char devname[NAME_MAX];
#endif
  int ret;

  DEBUGASSERT(display >= 0 && display < RFB_MAX_DISPLAYS);

  /* Start the VNC server kernel thread. */

  ret = vnc_start_server(display);

  if (ret < 0)
    {
      gerr("ERROR: vnc_start_server() failed: %d\n", ret);
      return ret;
    }

  /* Wait for the VNC server to be ready */

  ret = vnc_wait_start(display);

  if (ret < 0)
    {
      gerr("ERROR: wait for vnc server start failed: %d\n", ret);
      return ret;
    }

  /* Save the input callout function information in the session structure. */

  session           = g_vnc_sessions[display];
  session->arg      = session;

#ifdef CONFIG_VNCSERVER_TOUCH
  ret = snprintf(devname, sizeof(devname),
                 CONFIG_VNCSERVER_TOUCH_DEVNAME "%d", display);

  if (ret < 0)
    {
      gerr("ERROR: Format vnc touch driver path failed.\n");
      return ret;
    }

  ret = vnc_touch_register(devname, session);

  if (ret < 0)
    {
      gerr("ERROR: Initial vnc touch driver failed.\n");
      return ret;
    }

  session->mouseout = vnc_touch_event;
#endif

#ifdef CONFIG_VNCSERVER_KBD
  ret = snprintf(devname, sizeof(devname),
                 CONFIG_VNCSERVER_KBD_DEVNAME "%d", display);
  if (ret < 0)
    {
      gerr("ERROR: Format vnc keyboard driver path failed.\n");
      return ret;
    }

  ret = vnc_kbd_register(devname, session);
  if (ret < 0)
    {
      gerr("ERROR: Initial vnc keyboard driver failed.\n");
      goto err_kbd_register_failed;
    }

  session->kbdout = vnc_kbd_event;
#endif

  /* Has the framebuffer information been initialized for this display? */

  fbinfo = &g_fbinfo[display];
  if (!fbinfo->initialized)
    {
      fbinfo->vtable.getvideoinfo = up_getvideoinfo,
      fbinfo->vtable.getplaneinfo = up_getplaneinfo,
#ifdef CONFIG_FB_CMAP
      fbinfo->vtable.getcmap      = up_getcmap,
      fbinfo->vtable.putcmap      = up_putcmap,
#endif
#ifdef CONFIG_FB_HWCURSOR
      fbinfo->vtable.getcursor    = up_getcursor,
      fbinfo->vtable.setcursor    = up_setcursor,
#endif
#ifdef CONFIG_FB_SYNC
      fbinfo->vtable.waitforvsync = up_waitforsync;
#endif
      fbinfo->vtable.updatearea   = up_updateearea,
      fbinfo->display             = display;
      fbinfo->initialized         = true;
    }

  vtable = &fbinfo->vtable;

  ret = fb_register_device(display, 0, vtable);
  if (ret < 0)
    {
      gerr("ERROR: Initial vnc keyboard driver failed.\n");
      goto err_fb_register_failed;
    }

  return OK;

err_fb_register_failed:
#ifdef CONFIG_VNCSERVER_KBD
  vnc_kbd_unregister(session, devname);
err_kbd_register_failed:
#endif
#ifdef CONFIG_VNCSERVER_TOUCH
  vnc_touch_unregister(session, devname);
#endif
  return ret;
}

/****************************************************************************
 * Name: vnc_fb_unregister
 *
 * Description:
 *   Unregister the framebuffer support for the specified display.
 *
 * Input Parameters:
 *   display - In the case of hardware with multiple displays, this
 *     specifies the display.  Normally this is zero.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void vnc_fb_unregister(int display)
{
  FAR struct vnc_session_s *session;
#if defined(CONFIG_VNCSERVER_TOUCH) || defined(CONFIG_VNCSERVER_KBD)
  int ret;
  char devname[NAME_MAX];
#endif

  DEBUGASSERT(display >= 0 && display < RFB_MAX_DISPLAYS);
  session = g_vnc_sessions[display];

  /* Verify that the session is still valid */

  if (session != NULL)
    {
#ifdef CONFIG_VNCSERVER_TOUCH
      ret = snprintf(devname, sizeof(devname),
                     CONFIG_VNCSERVER_TOUCH_DEVNAME "%d", display);

      if (ret < 0)
        {
          gerr("ERROR: Format vnc touch driver path failed.\n");
          return;
        }

      vnc_touch_unregister(session, devname);
#endif

#ifdef CONFIG_VNCSERVER_KBD
      ret = snprintf(devname, sizeof(devname),
                     CONFIG_VNCSERVER_KBD_DEVNAME "%d", display);
      if (ret < 0)
        {
          gerr("ERROR: Format vnc keyboard driver path failed.\n");
          return;
        }

      vnc_kbd_unregister(session, devname);
#endif
    }
}
