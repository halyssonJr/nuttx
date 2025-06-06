/****************************************************************************
 * libs/libc/pwd/lib_getpwbufr.c
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>

#include "pwd/lib_pwd.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: getpwbuf_r
 *
 * Description:
 *   libc/grp internal helper function for getpwuid_r and getpwnam_r to setup
 *   the caller supplied 'pwd' and 'buf' buffers once a matching entry has
 *   been found.
 *
 * Input Parameters:
 *   uid    - Value to set the passwd structure's pw_uid field to.
 *   gid    - Value to set the passwd structure's pw_gid field to.
 *   name   - Value to set the passwd structure's pw_name field to.
 *   gecos  - Value to set the passwd structure's pw_gecos field to.
 *   dir    - Value to set the passwd structure's pw_dir field to.
 *   shell  - Value to set the passwd structure's pw_shell field to.
 *   pwd    - Pointer to the space to store the retrieved passwd structure
 *            in.
 *   buf    - String fields pointed to by the passwd struct are stored here.
 *   buflen - The length of buf in bytes.
 *   result - Pointer to the resulting passwd struct, or NULL in case of
 *            fail.
 *
 * Returned Value:
 *   On success getpwgid_r returns 0 and sets *result to pwd.  In case of
 *   failure an error number is returned.
 *
 ****************************************************************************/

int getpwbuf_r(uid_t uid, gid_t gid, FAR const char *name,
               FAR const char *gecos, FAR const char *dir,
               FAR const char *shell, FAR const char *passwd,
               FAR struct passwd *pwd, FAR char *buf, size_t buflen,
               FAR struct passwd **result)
{
  size_t reqdlen;
  size_t nsize;
  size_t gsize;
  size_t dsize;
  size_t ssize;
  size_t psize;

  nsize = strlen(name) + 1;
  gsize = strlen(gecos) + 1;
  dsize = strlen(dir) + 1;
  ssize = strlen(shell) + 1;
  psize = strlen(passwd) + 1;
  reqdlen = nsize + gsize + dsize + ssize + psize;

  if (buflen < reqdlen)
    {
      /* Insufficient buffer space supplied. */

      *result = NULL;
      return ERANGE;
    }

  pwd->pw_name   = buf;
  pwd->pw_gecos  = &buf[nsize];
  pwd->pw_dir    = &buf[nsize + gsize];
  pwd->pw_shell  = &buf[nsize + gsize + dsize];
  pwd->pw_passwd = &buf[nsize + gsize + dsize + ssize];

  pwd->pw_uid = uid;
  pwd->pw_gid = gid;
  strlcpy(pwd->pw_name, name, nsize);
  strlcpy(pwd->pw_gecos, gecos, gsize);
  strlcpy(pwd->pw_dir, dir, dsize);
  strlcpy(pwd->pw_shell, shell, ssize);
  strlcpy(pwd->pw_passwd, passwd, psize);

  *result = pwd;
  return 0;
}
