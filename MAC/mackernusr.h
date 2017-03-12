/* Shared Header file between userspace and kernel drivers
   pls, avoid writing any only userspace Or any only kernel space specific 
   codes in this file. 
 */

#ifndef __MAC_KERN_USR__
#define __MAC_KERN_USR__

#include "../dev_majors.h"
#include <linux/fcntl.h> // for _IO Macros

#define MAC_IOC_MAGIC	MAC_MAJOR_NUMBER

/* Reset the routing table*/
#define MAC_IOC_MACPURGE          _IO(MAC_IOC_MAGIC, 0)
/* Add Entry to Routing table*/
#define MAC_IOC_CR_MACENTRY       _IOW(MAC_IOC_MAGIC, 1, int)
/* Delete entry from MAC*/
#define MAC_IOC_D_MACENTRY        _IOW(MAC_IOC_MAGIC, 2, int)
/* Update MAC Entry*/
#define MAC_IOC_U_MACENTRY        _IOW(MAC_IOC_MAGIC, 3, int)
/* Look up MAC Entry*/
#define MAC_IOC_LOOKUP_MACENTRY   _IOR(MAC_IOC_MAGIC, 4, int)
/* Get entire MAC*/
#define MAC_IOC_GETMAC		_IO(MAC_IOC_MAGIC, 5)
/*Destry MAC Table for ever*/
#define MAC_IOC_MACDESTROY	_IO(MAC_IOC_MAGIC, 6)
/* Open MAC*/
#define MAC_IOC_MACOPEN		_IO(MAC_IOC_MAGIC, 7)
/* Close MAC*/
#define MAC_IOC_MACCLOSE		_IO(MAC_IOC_MAGIC, 8)
/* Change access mode on MAC*/
#define MAC_IOC_SET_ACCESS_MODE	_IOW(MAC_IOC_MAGIC, 9, int)
/* Get MAC info*/
#define MAC_IOC_GET_MAC_INFO	_IOR(MAC_IOC_MAGIC, 10, int)
/*Subscribe MAC*/
#define MAC_IOC_SUBSCRIBE_MAC	_IOR(MAC_IOC_MAGIC, 11, int)
/* Common CUD operations on mac*/
#define MAC_IOC_COMMON_UPDATE_MAC	_IOW(MAC_IOC_MAGIC, 12, unsigned long)

/*Shared structures between user space and kernel space*/
struct mac_entry {
        char vlan_id[16];
        char mac[48];
        char oif[16];
};

#define MAX_ENTRIES_FETCH	20

/* structure to send update from userspace to kernel and vice versa*/
struct mac_update_t{
	unsigned int op_code;
	struct mac_entry entry;
};

struct mac_info_t{
	unsigned int node_count;
	unsigned int actual_node_count;
	unsigned int no_of_pending_updates;
};

#endif
