/****************************************************************************
 * libs/libc/net/lib_getifaddrs.c
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

#include <debug.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/net/netconfig.h>

#include "libc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#undef  broadaddr
#define broadaddr dstaddr

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct myifaddrs
{
  struct ifaddrs          addrs;
  char                    name[IF_NAMESIZE];
  struct sockaddr_storage addr;
  struct sockaddr_storage netmask;
  struct sockaddr_storage dstaddr;
  struct sockaddr         hwaddr;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: getmutil_ipv6addr
 *
 * Input Parameters:
 *   fd     - socket fd of ioctl.
 *   req    - lifreq struct for ioctl request.
 *   ifaddr - the ifaddrs struct to be filled.
 *
 * Returned Value:
 *   On success, getmutil_ipv6addr() returns newest pointer of the linked
 *   list; on error, NULL is returned.
 *
 ****************************************************************************/

#ifdef CONFIG_NETDEV_MAX_IPv6_ADDR
static FAR struct myifaddrs *getmutil_ipv6addr(int fd,
                                               FAR struct lifreq *req,
                                               FAR struct myifaddrs *ifaddr)
{
  FAR struct sockaddr_in6 *ipv6addr;
  int index;

  if (req == NULL || ifaddr == NULL)
    {
      return NULL;
    }

  /* eth0:0 is the second ipaddr */

  for (index = 0; index < CONFIG_NETDEV_MAX_IPv6_ADDR - 1; index++)
    {
      int len = snprintf(req->lifr_name, sizeof(req->lifr_name),
                         "%s:%d", ifaddr->name, index);

      if (len < 0 || len >= sizeof(req->lifr_name))
        {
          nwarn("ifname %s:%2d error or too long\n", ifaddr->name, index);
          return ifaddr;
        }

      if (ioctl(fd, SIOCGLIFADDR, (unsigned long)req) < 0)
        {
          continue;
        }

      ipv6addr = (FAR struct sockaddr_in6 *)&(req->lifr_addr);
      if (IN6_IS_ADDR_UNSPECIFIED(&ipv6addr->sin6_addr))
        {
          continue;
        }

      ifaddr->addrs.ifa_next = lib_zalloc(sizeof(*ifaddr));
      if (ifaddr->addrs.ifa_next == NULL)
        {
          return NULL;
        }

      memcpy(ifaddr->addrs.ifa_next, ifaddr, sizeof(struct myifaddrs));
      ifaddr = (FAR struct myifaddrs *)ifaddr->addrs.ifa_next;
      memcpy(&ifaddr->addr, &(req->lifr_addr), sizeof(req->lifr_addr));
      ifaddr->addrs.ifa_addr = (FAR struct sockaddr *)&ifaddr->addr;
      ifaddr->addrs.ifa_netmask = (FAR struct sockaddr *)&ifaddr->netmask;
      ifaddr->addrs.ifa_dstaddr = (FAR struct sockaddr *)&ifaddr->dstaddr;
      ifaddr->addrs.ifa_broadaddr =
            (FAR struct sockaddr *)&ifaddr->broadaddr;
      ifaddr->addrs.ifa_data = (FAR struct sockaddr *)&ifaddr->hwaddr;
    }

  ifaddr->addrs.ifa_next = NULL;
  return ifaddr;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: getifaddrs
 *
 * Description:
 *   The getifaddrs() function returns a linked list of ifaddrs structures,
 *   each containing information about one of the network interfaces on the
 *   local system. The ifaddrs structure contains at least the following
 *   entries:
 *        struct ifaddrs  *ifa_next;
 *        char            *ifa_name;
 *        unsigned int     ifa_flags;
 *        struct sockaddr *ifa_addr;
 *        struct sockaddr *ifa_netmask;
 *        struct sockaddr *ifa_dstaddr;
 *        void            *ifa_data;
 *  The ifa_next field contains a pointer to the next structure on
 *  the list, or NULL if this is the last item of the list.
 *
 *  The ifa_name points to the null-terminated interface name.
 *
 *  The ifa_flags field contains the interface flags, as returned by
 *  the SIOCGIFFLAGS ioctl(2) operation (see netdevice(7) for a list
 *  of these flags).
 *
 *  The ifa_addr field points to a structure containing the interface
 *  address.  (The sa_family subfield should be consulted to
 *  determine the format of the address structure.)  This field may
 *  contain a null pointer.
 *
 *  The ifa_netmask field points to a structure containing the
 *  netmask associated with ifa_addr, if applicable for the address
 *  family.  This field may contain a null pointer.
 *
 *  Depending on whether the bit IFF_BROADCAST or IFF_POINTOPOINT is
 *  set in ifa_flags (only one can be set at a time), either
 *  ifa_broadaddr will contain the broadcast address associated with
 *  ifa_addr (if applicable for the address family) or ifa_dstaddr
 *  will contain the destination address of the point-to-point
 *  interface.
 *
 *  The ifa_data field points to a buffer containing address-family-
 *  specific data; this field may be NULL if there is no such data
 *  for this interface.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   On success, getifaddrs() returns pointer to the linked list; on error,
 *   NULL is returned, and errno is set to indicate the error.
 *
 ****************************************************************************/

int getifaddrs(FAR struct ifaddrs **addrs)
{
  FAR struct myifaddrs *myaddrs = NULL;
  int sockfd;
  int i;

  if (addrs == NULL)
    {
      set_errno(EINVAL);
      return ERROR;
    }

  sockfd = socket(NET_SOCK_FAMILY, NET_SOCK_TYPE, NET_SOCK_PROTOCOL);
  if (sockfd < 0)
    {
      return sockfd;
    }

  for (i = 1, *addrs = NULL; i <= MAX_IFINDEX; i++)
    {
      unsigned int flags;
      struct lifreq req;

      memset(&req, 0, sizeof(req));
      req.lifr_ifindex = i;

      if (ioctl(sockfd, SIOCGIFNAME, (unsigned long)&req) < 0)
        {
          continue; /* Empty slot, try next one */
        }

      if (ioctl(sockfd, SIOCGIFFLAGS, (unsigned long)&req) < 0)
        {
          goto err;
        }

      flags = req.lifr_flags;

      if (myaddrs != NULL)
        {
          myaddrs->addrs.ifa_next = lib_zalloc(sizeof(*myaddrs));
          myaddrs = (FAR struct myifaddrs *)myaddrs->addrs.ifa_next;
        }
      else
        {
          *addrs = lib_zalloc(sizeof(*myaddrs));
          myaddrs = (FAR struct myifaddrs *)*addrs;
        }

      if (myaddrs == NULL)
        {
          goto err;
        }

      myaddrs->addrs.ifa_name = myaddrs->name;
      strlcpy(myaddrs->name, req.lifr_name, IF_NAMESIZE);

      myaddrs->addrs.ifa_flags = flags;

#ifdef CONFIG_NET_IPv4
      if (ioctl(sockfd, SIOCGIFADDR, (unsigned long)&req) >= 0)
        {
          myaddrs->addrs.ifa_addr = (FAR struct sockaddr *)&myaddrs->addr;
          memcpy(&myaddrs->addr, &req.lifr_addr, sizeof(req.lifr_addr));

          if (ioctl(sockfd, SIOCGIFNETMASK, (unsigned long)&req) >= 0)
            {
              myaddrs->addrs.ifa_netmask =
                     (FAR struct sockaddr *)&myaddrs->netmask;
              memcpy(&myaddrs->netmask,
                     &req.lifr_netmask, sizeof(req.lifr_netmask));
            }

          if (ioctl(sockfd, SIOCGIFDSTADDR, (unsigned long)&req) >= 0)
            {
              myaddrs->addrs.ifa_dstaddr =
                     (FAR struct sockaddr *)&myaddrs->dstaddr;
              memcpy(&myaddrs->dstaddr,
                     &req.lifr_dstaddr, sizeof(req.lifr_dstaddr));
            }
          else if (ioctl(sockfd, SIOCGIFBRDADDR, (unsigned long)&req) >= 0)
            {
              myaddrs->addrs.ifa_broadaddr =
                     (FAR struct sockaddr *)&myaddrs->broadaddr;
              memcpy(&myaddrs->broadaddr,
                     &req.lifr_broadaddr, sizeof(req.lifr_broadaddr));
            }

          if (ioctl(sockfd, SIOCGIFHWADDR, (unsigned long)&req) >= 0)
            {
              myaddrs->addrs.ifa_data = &myaddrs->hwaddr;
              memcpy(&myaddrs->hwaddr,
                     &req.lifr_hwaddr, sizeof(req.lifr_hwaddr));
            }
        }
#endif

#ifdef CONFIG_NET_IPv6
      if (ioctl(sockfd, SIOCGLIFADDR, (unsigned long)&req) >= 0)
        {
          /* Has IPv4 entry ? */

          if (myaddrs->addrs.ifa_addr)
            {
              /* Yes, allocate the new entry for IPv6 */

              myaddrs->addrs.ifa_next = lib_zalloc(sizeof(*myaddrs));
              myaddrs = (FAR struct myifaddrs *)myaddrs->addrs.ifa_next;

              if (myaddrs == NULL)
                {
                  goto err;
                }

              myaddrs->addrs.ifa_name = myaddrs->name;
              strlcpy(myaddrs->name, req.lifr_name, IF_NAMESIZE);

              myaddrs->addrs.ifa_flags = flags;
            }

          myaddrs->addrs.ifa_addr = (FAR struct sockaddr *)&myaddrs->addr;
          memcpy(&myaddrs->addr, &req.lifr_addr, sizeof(req.lifr_addr));
          ((struct sockaddr_in6 *)&myaddrs->addr)->sin6_scope_id = i;

          if (ioctl(sockfd, SIOCGLIFNETMASK, (unsigned long)&req) >= 0)
            {
              myaddrs->addrs.ifa_netmask =
                     (FAR struct sockaddr *)&myaddrs->netmask;
              memcpy(&myaddrs->netmask,
                     &req.lifr_netmask, sizeof(req.lifr_netmask));
            }

          if (ioctl(sockfd, SIOCGLIFDSTADDR, (unsigned long)&req) >= 0)
            {
              myaddrs->addrs.ifa_dstaddr =
                     (FAR struct sockaddr *)&myaddrs->dstaddr;
              memcpy(&myaddrs->dstaddr,
                     &req.lifr_dstaddr, sizeof(req.lifr_dstaddr));
            }
          else if (ioctl(sockfd, SIOCGLIFBRDADDR, (unsigned long)&req) >= 0)
            {
              myaddrs->addrs.ifa_broadaddr =
                     (FAR struct sockaddr *)&myaddrs->broadaddr;
              memcpy(&myaddrs->broadaddr,
                     &req.lifr_broadaddr, sizeof(req.lifr_broadaddr));
            }

          if (ioctl(sockfd, SIOCGIFHWADDR, (unsigned long)&req) >= 0)
            {
              myaddrs->addrs.ifa_data = &myaddrs->hwaddr;
              memcpy(&myaddrs->hwaddr,
                     &req.lifr_hwaddr, sizeof(req.lifr_hwaddr));
            }

#  ifdef CONFIG_NETDEV_MAX_IPv6_ADDR
          if (getmutil_ipv6addr(sockfd, &req, myaddrs) == NULL)
            {
              goto err;
            }
#  endif
        }
#endif
    }

  close(sockfd);
  return OK;

err:
  if (*addrs != NULL)
    {
      freeifaddrs(*addrs);
      *addrs = NULL;
    }

  close(sockfd);
  return ERROR;
}
