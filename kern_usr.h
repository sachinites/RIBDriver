/* Shared Header file between userspace and kernel drivers
   pls, avoid writing any only userspace Or any only kernel space specific 
   codes in this file. 
 */

#ifndef __KERN_USR__
#define __KERN_USR__

#include "dev_majors.h"
#include <linux/fcntl.h> // for _IO Macros

#define RT_IOC_MAGIC	RT_MAJOR_NUMBER

/* Reset the routing table*/
#define RT_IOC_RTPURGE          _IO(RT_IOC_MAGIC, 0)
/* Add Entry to Routing table*/
#define RT_IOC_CR_RTENTRY       _IOW(RT_IOC_MAGIC, 1, int)
/* Delete entry from RT*/
#define RT_IOC_D_RTENTRY        _IOW(RT_IOC_MAGIC, 2, int)
/* Update RT Entry*/
#define RT_IOC_U_RTENTRY        _IOW(RT_IOC_MAGIC, 3, int)
/* Look up RT Entry*/
#define RT_IOC_LOOKUP_RTENTRY   _IOR(RT_IOC_MAGIC, 4, int)
/* Get entire RT*/
#define RT_IOC_GETRT		_IO(RT_IOC_MAGIC, 5)
/*Destry RT Table for ever*/
#define RT_IOC_RTDESTROY	_IO(RT_IOC_MAGIC, 6)
/* Open RT*/
#define RT_IOC_RTOPEN		_IO(RT_IOC_MAGIC, 7)
/* Close RT*/
#define RT_IOC_RTCLOSE		_IO(RT_IOC_MAGIC, 8)
/* Change access mode on RT*/
#define RT_IOC_SET_ACCESS_MODE	_IOW(RT_IOC_MAGIC, 9, int)
/* Get RT info*/
#define RT_IOC_GET_RT_INFO	_IOR(RT_IOC_MAGIC, 10, int)

/*Shared structures between user space and kernel space*/
struct rt_entry {
        char dst_ip[16];
        char nxt_hop_ip[16];
        char oif[16];
};

#define MAX_ENTRIES_FETCH	20

/* Reply to user space appln about RT change */

#define RT_ROUTE_UPDATE	0
#define RT_ROUTE_ADD	1
#define RT_ROUTE_DEL	2
#define RT_DELETE	3

struct rt_update_to_user_t{
	unsigned int op_code;
	struct rt_entry;
};

struct rt_info_t{
	unsigned int node_count;
	unsigned int actual_node_count;
};

#endif
