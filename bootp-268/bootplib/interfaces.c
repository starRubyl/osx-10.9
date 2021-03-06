/*
 * Copyright (c) 1999-2013 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * interfaces.c
 * - get the list of interfaces in the system
 */

/*
 * Modification History
 * 02/23/98	Dieter Siegmund (dieter@apple.com)
 * - initial version
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <strings.h>
#include <netdb.h>
#include "interfaces.h"
#include <arpa/inet.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if_media.h>
#include <net/route.h>
#include <ifaddrs.h>

#include "util.h"
#include "IPConfigurationLog.h"

static boolean_t
S_get_ifmediareq(const char * name, struct ifmediareq * ifmr_p);

static boolean_t
S_get_ifmediareq_s(int s, const char * name, struct ifmediareq * ifmr_p);

static boolean_t
S_is_awdl(int s, const char * name);

static boolean_t
S_ifmediareq_get_is_wireless(struct ifmediareq * ifmr_p);

static link_status_t
S_ifmediareq_get_link_status(struct ifmediareq * ifmr_p);

void *
inet_addrinfo_copy(void * p)
{
    inet_addrinfo_t * src = (inet_addrinfo_t *)p;
    inet_addrinfo_t * dest;

    dest = malloc(sizeof(*dest));
    if (dest) {
	bcopy(src, dest, sizeof(*dest));
    }
    return (dest);
}

void
inet_addrinfo_free(void * p)
{
    free(p);
    return;
}

static interface_t *
S_next_entry(interface_list_t * interfaces, const char * name)
{
    interface_t * entry;

    if (interfaces->count >= interfaces->size)
	return (NULL);

    entry = interfaces->list + interfaces->count++;
    bzero(entry, sizeof(*entry));
    strlcpy(entry->name, name, sizeof(entry->name));
    dynarray_init(&entry->inet, inet_addrinfo_free,
		  inet_addrinfo_copy);
    return (entry);
}

static __inline__ int
count_ifaddrs(const struct ifaddrs * ifap)
{
    int		count;

    for (count = 0; ifap != NULL && ifap->ifa_addr != NULL; 
	 ifap = ifap->ifa_next) {
	count++;
    }
    return (count);
}

static boolean_t
S_build_interface_list(interface_list_t * interfaces)
{
    struct ifaddrs *	addrs = NULL;
    struct ifaddrs *	ifap = NULL;
    struct ifmediareq	ifmr;
    int			s = -1;
    int			size;
    boolean_t		success = FALSE;

    if (getifaddrs(&addrs) < 0) {
	goto done;
    }
    size = count_ifaddrs(addrs);
    interfaces->list 
	= (interface_t *)malloc(size * sizeof(*(interfaces->list)));
    if (interfaces->list == NULL) {
	goto done;
    }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	goto done;
    }
    interfaces->count = 0;
    interfaces->size = size;
    
    for (ifap = addrs; ifap != NULL; ifap = ifap->ifa_next) {
	const char *	name;
	if (ifap->ifa_addr == NULL) {
	    continue;
	}
	name = ifap->ifa_name;
	switch (ifap->ifa_addr->sa_family) {
	  case AF_INET: {
	      inet_addrinfo_t	info;
	      interface_t *	entry;

	      entry = ifl_find_name(interfaces, name);
	      if (entry == NULL) { /* new entry */
		  entry = S_next_entry(interfaces, name);
		  if (entry == NULL) {
		      /* NOT REACHED */
		      IPConfigLog(LOG_ERR,
				  "interfaces: S_next_entry returns NULL"); 
		      continue;
		  }
		  entry->flags = ifap->ifa_flags;
	      }
	      bzero(&info, sizeof(info));
	
	      /* ALIGN: getifaddrs should align, cast ok. */	
	      info.addr = ((struct sockaddr_in *)
			   (void *)ifap->ifa_addr)->sin_addr;

	      if (ifap->ifa_netmask != NULL) {
		  /* ALIGN: getifaddrs should align, cast ok. */
		  info.mask 
		      = ((struct sockaddr_in *)
			 (void *)ifap->ifa_netmask)->sin_addr;
	      }
	      if (entry->flags & IFF_BROADCAST && ifap->ifa_broadaddr != NULL) {
		  /* ALIGN: getifaddrs should align, cast ok. */
		  info.broadcast 
		      = ((struct sockaddr_in *)(void *)
				ifap->ifa_broadaddr)->sin_addr;
	      }
	      info.netaddr.s_addr = htonl(iptohl(info.addr)
					  & iptohl(info.mask));
	      dynarray_add(&entry->inet, inet_addrinfo_copy(&info));
	      break;
	  }
	  case AF_LINK: {
	      struct sockaddr_dl * dl_p;
	      interface_t *	entry;
	      struct if_data *	if_data;
		
	      /* ALIGN: getifaddrs should align, cast ok. */
	      dl_p = (struct sockaddr_dl *)
		     (void *)ifap->ifa_addr;

	      entry = ifl_find_name(interfaces, name);
	      if (entry == NULL) { /* new entry */
		  entry = S_next_entry(interfaces, name);
		  if (entry == NULL) {
		      /* NOT REACHED */
		      IPConfigLog(LOG_ERR,
				  "interfaces: S_next_entry returns NULL"); 
		      continue;
		  }
		  entry->flags = ifap->ifa_flags;
	      }
	      if (dl_p->sdl_alen > sizeof(entry->link_address.addr)) {
		  IPConfigLog(LOG_ERR,
			      "%s: link type %d address length %d > %ld", name,
			      dl_p->sdl_type, dl_p->sdl_alen,
			      sizeof(entry->link_address.addr));
		  entry->link_address.length = sizeof(entry->link_address.addr);
	      }
	      else {
		  entry->link_address.length = dl_p->sdl_alen;
	      }
	      bcopy(dl_p->sdl_data + dl_p->sdl_nlen, 
		    entry->link_address.addr,
		    entry->link_address.length);
	      entry->link_address.type = dl_p->sdl_type;
	      entry->link_address.index = dl_p->sdl_index;
	      if_data = (struct if_data *)ifap->ifa_data;
	      if (if_data != NULL) {
		  entry->type = if_data->ifi_type;
	      }
	      else {
		  entry->type = dl_p->sdl_type;
	      }
	      if (S_get_ifmediareq_s(s, name, &ifmr)) {
		  if (entry->type == IFT_ETHER) {

		      if (S_ifmediareq_get_is_wireless(&ifmr)) {
			  entry->type_flags |= kInterfaceTypeFlagIsWireless;
			  if (S_is_awdl(s, name)) {
			      entry->type_flags |= kInterfaceTypeFlagIsAWDL;
			  }
		      }
		  }
		  entry->link_status
		      = S_ifmediareq_get_link_status(&ifmr);
	      }
	      break;
	  }
	}
    }
    /* make it the "right" size (plus 1 more in case it's 0) */
    interfaces->list = (interface_t *)
	realloc(interfaces->list, 
		sizeof(*(interfaces->list)) * (interfaces->count + 1));
    success = TRUE;

 done:
    if (addrs != NULL) {
	freeifaddrs(addrs);
    }
    if (success == FALSE) {
	if (interfaces->list != NULL) {
	    free(interfaces->list);
	}
	interfaces->list = NULL;
    }
    if (s >= 0) {
	close(s);
    }
    return (success);
}

int
ifl_count(interface_list_t * list_p)
{
    return (list_p->count);
}

interface_t *		
ifl_at_index(interface_list_t * list_p, int i)
{
    if (i >= list_p->count || i < 0)
	return (NULL);
    return (list_p->list + i);
}

int
ifl_index(interface_list_t * list_p, interface_t * if_p)
{
    return (if_p - list_p->list);
}

/*
 * Function: ifl_first_broadcast_inet
 *
 * Purpose:
 *   Return the first non-loopback, broadcast capable interface.
 */
interface_t *
ifl_first_broadcast_inet(interface_list_t * list_p)
{
    int i;
    for (i = 0; i < list_p->count; i++) {
	interface_t * if_p = list_p->list + i;

	if (dynarray_count(&if_p->inet) > 0
	    && !(if_p->flags & IFF_LOOPBACK)
	    && (if_p->flags & IFF_BROADCAST))
	    return (list_p->list + i);
    }
    return (NULL);
}

interface_t *
ifl_find_ip(interface_list_t * list_p, struct in_addr iaddr)
{
    int 	i;

    for (i = 0; i < list_p->count; i++) {
	interface_t * if_p = list_p->list + i;
	int j;

	for (j = 0; j < dynarray_count(&if_p->inet); j++) {
	    inet_addrinfo_t *	info;

	    info = dynarray_element(&if_p->inet, j);
	    if (info->addr.s_addr == iaddr.s_addr)
		return (if_p);
	}
    }
    return (NULL);
}

interface_t *
ifl_find_subnet(interface_list_t * list_p, struct in_addr iaddr)
{
    int 	i;
    u_long	addr_hl = iptohl(iaddr);

    for (i = 0; i < list_p->count; i++) {
	interface_t * if_p = list_p->list + i;
	int j;

	for (j = 0; j < dynarray_count(&if_p->inet); j++) {
	    inet_addrinfo_t *	info = dynarray_element(&if_p->inet, j);
	    u_long 		ifnetaddr_hl = iptohl(info->netaddr);
	    u_long 		ifmask_hl = iptohl(info->mask);

	    if ((addr_hl & ifmask_hl) == ifnetaddr_hl)
		return (if_p);
	}
    }
    return (NULL);
}

interface_t *
ifl_find_name(interface_list_t * list_p, const char * name)
{
    int i;

    for (i = 0; i < list_p->count; i++) {
	if (strcmp(list_p->list[i].name, name) == 0)
	    return (list_p->list + i);
    }
    return (NULL);
}

interface_t *
ifl_find_link(interface_list_t * list_p, int index)
{
    int i;

    for (i = 0; i < list_p->count; i++) {
	if (list_p->list[i].link_address.type != 0
	    && list_p->list[i].link_address.index == index)
	    return (list_p->list + i);
    }
    return (NULL);
}

interface_list_t *
ifl_init()
{
    interface_list_t * list_p = (interface_list_t *)malloc(sizeof(*list_p));
    if (list_p == NULL
	|| S_build_interface_list(list_p) == FALSE) {
	if (list_p)
	    free(list_p);
	return (NULL);
    }
    return (list_p);
}

void
ifl_free(interface_list_t * * iflist)
{
    if (iflist != NULL && *iflist != NULL) {
	int 			i;
	interface_list_t * 	list_p = *iflist;
	
	for (i = 0; i < list_p->count; i++) {
	    dynarray_free(&list_p->list[i].inet);
	}
	if (list_p->list)
	    free(list_p->list);
	free(list_p);
	*iflist = NULL;
    }
    return;
}

/*
 * Functions: if_*
 * Purpose:
 *   Interface-specific routines.
 */
const char *
if_name(interface_t * if_p)
{
    return (if_p->name);
}

uint16_t
if_flags(interface_t * if_p)
{
    return (if_p->flags);
}

int
if_inet_count(interface_t * if_p)
{
    return (dynarray_count(&if_p->inet));
}

inet_addrinfo_t *
if_inet_addr_at(interface_t * if_p, int i)
{
    return (dynarray_element(&if_p->inet, i));
}

/* get the primary address, mask, broadcast */
struct in_addr
if_inet_addr(interface_t * if_p)
{
    inet_addrinfo_t * 	info = if_inet_addr_at(if_p, 0);

    if (info == NULL) {
	struct in_addr	zeroes = { 0 };
	return (zeroes);
    }
    return (info->addr);
}

struct in_addr
if_inet_netmask(interface_t * if_p)
{
    inet_addrinfo_t * info = if_inet_addr_at(if_p, 0);
    return (info->mask);
}

struct in_addr
if_inet_netaddr(interface_t * if_p)
{
    inet_addrinfo_t * info = if_inet_addr_at(if_p, 0);
    return (info->netaddr);
}

struct in_addr
if_inet_broadcast(interface_t * if_p)
{
    inet_addrinfo_t * info = if_inet_addr_at(if_p, 0);
    return (info->broadcast);
}

void
if_setflags(interface_t * if_p, uint16_t flags)
{
    if_p->flags = flags;
    return;
}

boolean_t
if_inet_valid(interface_t * if_p)
{
    return (dynarray_count(&if_p->inet) > 0);
}

void
if_free(interface_t * * if_p_p)
{
    interface_t * if_p;

    if (if_p_p == NULL) {
	return;
    }
    if_p = *if_p_p;
    if (if_p == NULL) {
	return;
    }
    dynarray_free(&if_p->inet);
    free(if_p);
    *if_p_p = NULL;
    return;
}

interface_t *
if_dup(interface_t * intface)
{
    interface_t * new_p;

    new_p = (interface_t *)calloc(1, sizeof(*new_p));
    if (new_p == NULL) {
	return (NULL);
    }
    *new_p = *intface;
    (void)dynarray_dup(&new_p->inet, &intface->inet);
    return (new_p);
}

int
if_inet_find_ip(interface_t * if_p, struct in_addr iaddr)
{
    int i;
    for (i = 0; i < if_inet_count(if_p); i++) {
	inet_addrinfo_t *	info = if_inet_addr_at(if_p, i);
	if (info->addr.s_addr == iaddr.s_addr) {
	    return (i);
	}
    }
    return (INDEX_BAD);
}

void
if_link_copy(interface_t * dest, const interface_t * source)
{
    dest->link_status = source->link_status;
    dest->link_address = source->link_address;
    return;
}

boolean_t
if_link_update(interface_t * if_p)
{
    void *			addr;
    int				addr_length;
    char *			buf = NULL;
    size_t			buf_len = 0;
    boolean_t			changed = FALSE;
    struct sockaddr_dl *	dl_p;
    struct if_msghdr * 		ifm;
    int				mib[6];
    int				type;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_LINK;
    mib[4] = NET_RT_IFLIST;
    mib[5] = if_p->link_address.index; /* ask for exactly one interface */

    if (sysctl(mib, 6, NULL, &buf_len, NULL, 0) < 0) {
	fprintf(stderr, "sysctl() size failed: %s", strerror(errno));
	goto failed;
    }
    buf = malloc(buf_len);
    if (sysctl(mib, 6, buf, &buf_len, NULL, 0) < 0) {
	fprintf(stderr, "sysctl() failed: %s", strerror(errno));
	goto failed;
    }
    /* ALIGN: buf is aligned (from malloc), cast ok. */
    ifm = (struct if_msghdr *)(void *)buf;
    switch (ifm->ifm_type) {
    case RTM_IFINFO:
	dl_p = (struct sockaddr_dl *)(ifm + 1);
	addr = dl_p->sdl_data + dl_p->sdl_nlen;
	addr_length = dl_p->sdl_alen;
	type = dl_p->sdl_type;
	if (addr_length > sizeof(if_p->link_address.addr)) {
	    IPConfigLog(LOG_DEBUG,
			"%s: link type %d address length %d > %ld",
			if_name(if_p), 
			type,
			addr_length, 
			sizeof(if_p->link_address.addr));
	    addr_length = sizeof(if_p->link_address.addr);
	}
	if (if_p->link_address.type != type
	    || addr_length != if_p->link_address.length
	    || (addr_length != 0
		&& bcmp(addr, if_p->link_address.addr, addr_length))) {
	    changed = TRUE;
	    if_p->link_address.length = addr_length;
	    bcopy(addr, if_p->link_address.addr, addr_length);
	    if_p->link_address.type = dl_p->sdl_type;
	}
    }
 failed:
    if (buf != NULL) {
	free(buf);
    }
    return (changed);
}

int
if_ift_type(interface_t * if_p)
{
    return (if_p->type);
}

int
if_link_type(interface_t * if_p)
{
    return (if_p->link_address.type);
}

int
if_link_dhcptype(interface_t * if_p)
{
    if (if_p->link_address.type == IFT_IEEE1394) {
	return (ARPHRD_IEEE1394_EUI64);
    }
    else {
	return (dl_to_arp_hwtype(if_p->link_address.type));
    }
}

int
if_link_arptype(interface_t * if_p)
{
    return (dl_to_arp_hwtype(if_p->link_address.type));
}


void *
if_link_address(interface_t * if_p)
{
    return (if_p->link_address.addr);
}

int
if_link_length(interface_t * if_p)
{
    return (if_p->link_address.length);
}

boolean_t
if_is_wireless(interface_t * if_p)
{
    return ((if_p->type_flags & kInterfaceTypeFlagIsWireless) != 0);
}

boolean_t
if_is_awdl(interface_t * if_p)
{
    return ((if_p->type_flags & kInterfaceTypeFlagIsAWDL) != 0);
}

int
if_link_index(interface_t * if_p)
{
    return (if_p->link_address.index);
}

link_status_t 
if_get_link_status(interface_t * if_p)
{
    return (if_p->link_status);
}

static int
siocgifmedia(int sockfd, struct ifmediareq * ifmr_p,
	     const char * name)
{
    (void)memset(ifmr_p, 0, sizeof(*ifmr_p));
    (void)strlcpy(ifmr_p->ifm_name, name, sizeof(ifmr_p->ifm_name));
    return (ioctl(sockfd, SIOCGIFMEDIA, (caddr_t)ifmr_p));
}

static boolean_t
S_get_ifmediareq_s(int s, const char * name, struct ifmediareq * ifmr_p)
{
    boolean_t	ret;

    if (siocgifmedia(s, ifmr_p, name) == -1) {
	if (errno == EOPNOTSUPP) {
	    ifmr_p->ifm_status = IFM_ACTIVE | IFM_AVALID;
    	    ifmr_p->ifm_count = 1;
	    ret = TRUE;
	}
	else {
	    ret = FALSE;
	}
    }
    else {
	ret = TRUE;
    }
    return (ret);
}

static boolean_t
S_get_ifmediareq(const char * name, struct ifmediareq * ifmr_p)
{
    int		s;
    boolean_t	success = FALSE;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s >= 0) {
	success = S_get_ifmediareq_s(s, name, ifmr_p);
	close(s);
    }
    return (success);
}


static boolean_t
S_ifmediareq_get_is_wireless(struct ifmediareq * ifmr_p)
{
    return (IFM_TYPE(ifmr_p->ifm_current) == IFM_IEEE80211);
}

static link_status_t
S_ifmediareq_get_link_status(struct ifmediareq * ifmr_p)
{
    link_status_t 	link;

    bzero(&link, sizeof(link));
    if (ifmr_p->ifm_count > 0 && (ifmr_p->ifm_status & IFM_AVALID) != 0) {
	link.valid = TRUE;
	if ((ifmr_p->ifm_status & IFM_ACTIVE) != 0) {
	    link.active = TRUE;
	}
	link.wake_on_same_network
	    = ((ifmr_p->ifm_status & IFM_WAKESAMENET) != 0);
    }
    return (link);
}

static int
siocgifeflags(int sockfd, struct ifreq * ifr, const char * name)
{
    (void)memset(ifr, 0, sizeof(*ifr));
    (void)strlcpy(ifr->ifr_name, name, sizeof(ifr->ifr_name));
    return (ioctl(sockfd, SIOCGIFEFLAGS, (caddr_t)ifr));
}

static boolean_t
S_is_awdl(int sockfd, const char * name)
{
    struct ifreq	ifr;
    boolean_t		is_awdl = FALSE;

    if (siocgifeflags(sockfd, &ifr, name) == -1) {
	if (errno != ENXIO && errno != EPWROFF && errno != EINVAL) {
	    IPConfigLogFL(LOG_NOTICE,
			  "%s: SIOCGIFEFLAGS failed status, %s",
			  name, strerror(errno));
	}
    }
    else if ((ifr.ifr_eflags & IFEF_AWDL) != 0) {
	is_awdl = TRUE;
    }
    return (is_awdl);
}

link_status_t
if_link_status_update(interface_t * if_p)
{
    struct ifmediareq 		ifmr;

    if (S_get_ifmediareq(if_name(if_p), &ifmr) == FALSE) {
	if (errno != ENXIO && errno != EPWROFF && errno != EINVAL) {
	    IPConfigLogFL(LOG_NOTICE,
			  "%s: failed to get media status, %s",
			  if_name(if_p), strerror(errno));
	}
    }
    else {
	if_p->link_status = S_ifmediareq_get_link_status(&ifmr);
    }
    return (if_p->link_status);
}


#ifdef TEST_INTERFACES

#if 0
void
sockaddr_dl_print(struct sockaddr_dl * dl_p)
{
    int i;

    printf("link: len %d index %d family %d type 0x%x nlen %d alen %d"
	   " slen %d addr ", dl_p->sdl_len, 
	   dl_p->sdl_index,  dl_p->sdl_family, dl_p->sdl_type,
	   dl_p->sdl_nlen, dl_p->sdl_alen, dl_p->sdl_slen);
    for (i = 0; i < dl_p->sdl_alen; i++) 
	printf("%s%x", i ? ":" : "", 
	       ((unsigned char *)dl_p->sdl_data + dl_p->sdl_nlen)[i]);
    printf("\n");
}
#endif

void
link_addr_print(link_addr_t * link)
{
    int i;

    printf("link: index %d type 0x%x alen %d%s", link->index, link->type,
	   link->length, link->length > 0 ? " addr" : "");
    for (i = 0; i < link->length; i++) {
	printf("%c%x", i ? ':' : ' ', link->addr[i]);
    }
    printf("\n");
}

void
ifl_print(interface_list_t * list_p)
{
    int i;
    int count = 0;
    
    printf("Interface count = %d\n", list_p->count);
    for (i = 0; i < list_p->count; i++) {
	interface_t * 	if_p = list_p->list + i;
	int		j;
	
	if (i > 0)
	    printf("\n");
	
	printf("%s: type %d\n", if_name(if_p), if_ift_type(if_p));
	
	for (j = 0; j < if_inet_count(if_p); j++) {
	    inet_addrinfo_t * info = if_inet_addr_at(if_p, j);
	    
	    printf("inet: %s", inet_ntoa(info->addr));
	    printf(" netmask %s", inet_ntoa(info->mask));
	    if (if_flags(if_p) & IFF_BROADCAST)
		printf(" %s\n", inet_ntoa(info->broadcast));
	    else
		printf("\n");
	}
	if (if_p->link_address.type != 0) {
	    link_addr_print(&if_p->link_address);
	    if (if_p->link_status.valid) {
		printf("Link is %s%s\n",
		       if_p->link_status.active ? "active" : "inactive",
		       if_p->link_status.wake_on_same_network
		       ? " [wake on same network]" : "");
	    }
	    if (if_is_wireless(if_p)) {
		printf("wireless%s\n",
		       if_is_awdl(if_p) ? " AWDL" : "");
	    }
	}
	count++;
    }
    return;
}

int
main()
{
    interface_list_t * list_p = ifl_init();
    if (list_p != NULL) {
	ifl_print(list_p);
    }
    exit(0);
}
#endif /* TEST_INTERFACES */
