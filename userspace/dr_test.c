#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <linux/fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "../RT/rtkernusr.h"
#include "../MAC/mackernusr.h"
#include "../cmdcodes.h"
#include "threadApi.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

int rt_fd = 0, mac_fd = 0;


static void
print_rt_one_entry(unsigned int i, struct rt_entry *entry){
	printf("    |------|----------------------|----------------------|--------------|\n");
	printf("    | %-4d | %-20s | %-20s | %-12s |\n", i, entry->dst_ip, entry->nxt_hop_ip, entry->oif);
}


static void
print_mac_one_entry(unsigned int i, struct mac_entry *entry){
	printf("    |------|----------------------|----------------------|--------------|\n");
	printf("    | %-4d | %-20s | %-20s | %-12s |\n", i, entry->vlan_id, entry->mac, entry->oif);
}

void
print_rt_fetched_entries(char *buf, unsigned int size, unsigned int units){
	struct rt_update_t *update_msg = NULL;
	unsigned int j = 0;

	update_msg = (struct rt_update_t *)buf;
	for( ; j < units; j++)
		print_rt_one_entry(j+1 , &update_msg->entry);		
	printf("    |------|----------------------|----------------------|--------------|\n");
}

void
print_mac_fetched_entries(char *buf, unsigned int size, unsigned int units){
	struct mac_update_t *update_msg = NULL;
	unsigned int j = 0;
	
	update_msg = (struct rt_update_t *)buf;
	for( ; j < units; j++)
		print_mac_one_entry(j+1 , &update_msg->entry);		
	printf("    |------|----------------------|----------------------|--------------|\n");
}


static void
poll_rt_mac_table(){
	/*assume that FD are open*/
	fd_set readset;

	char rt_buf[RT_MAX_ENTRIES_FETCH * sizeof(struct rt_update_t)];
	char mac_buf[MAC_MAX_ENTRIES_FETCH * sizeof(struct mac_update_t)];

	while(1){
		FD_ZERO(&readset);
		FD_SET(rt_fd, &readset);
		FD_SET(mac_fd, &readset);
		int max_fd = 0, rc = 0;
		max_fd = (rt_fd > mac_fd) ? rt_fd : mac_fd;
		printf("%s() : blocking on select sys call\n", __FUNCTION__);	
		rc = select(max_fd + 1, &readset, NULL, NULL, NULL);
		printf("select unblocked , rc = %d\n", rc);
		if (FD_ISSET(rt_fd, &readset)){
			printf("%s() : updates notification for rt arrives\n", __FUNCTION__);
			memset(rt_buf, 0, sizeof(rt_buf));
			rc = read(rt_fd, rt_buf, sizeof(rt_buf));
			printf("Total Routes = %d\n", rc);
			print_rt_fetched_entries(rt_buf, sizeof(rt_buf), rc);
		}
		if (FD_ISSET(mac_fd, &readset)){
			printf("%s() : updates notification for mac arrives\n", __FUNCTION__);
			memset(mac_buf, 0, sizeof(mac_buf));
			rc = read(mac_fd, mac_buf, sizeof(mac_buf));
			printf("Total Routes = %d\n", rc);
			print_mac_fetched_entries(mac_buf, sizeof(mac_buf), rc);
		}
	}
}




static void
mac_open(){

	int i = 0;
	/* Here user is opening a file. User should specify the appropriate
	permissions/accessibility using flags. defined in uapi/asm-generic/fcntl.h
	flags supported are :
		1. O_APPEND // append the new data, no read access
		2. O_CREAT  // clean the older routing table if already exists 
		3. O_RDONLY // no write access
		4. O_WRONLY // no read access
		5. O_RDWR   // read and write access
		6. O_NONBLOCK // disable blocking calls	
	*/

	if ((mac_fd = open("/dev/mac", O_RDWR /*| O_NONBLOCK*/)) == -1) {
		perror("open failed");
		exit(EXIT_SUCCESS);
	}

	printf("%s() : Success\n", __FUNCTION__);

#if 0
	struct rt_entry entry[4];	
	memset(entry, 0, sizeof(entry));

	strcpy(entry[0].dst_ip,     "10.1.1.1\0");
	strcpy(entry[0].nxt_hop_ip, "10.1.1.2\0");
	strcpy(entry[0].oif, 	    "eth0\0");


	strcpy(entry[1].dst_ip,     "20.1.1.1\0");
	strcpy(entry[1].nxt_hop_ip, "20.1.1.2\0");
	strcpy(entry[1].oif, 	    "eth1\0");
	 
	
	strcpy(entry[2].dst_ip,     "30.1.1.1\0");
	strcpy(entry[2].nxt_hop_ip, "30.1.1.2\0");
	strcpy(entry[2].oif, 	    "eth2\0");
		

	strcpy(entry[3].dst_ip,     "40.1.1.1\0");
	strcpy(entry[3].nxt_hop_ip, "40.1.1.2\0");
	strcpy(entry[3].oif, 	    "eth3\0");

	for(; i < 4 ; i++)
		ioctl(fd, RT_IOC_CR_RTENTRY, &entry[i]);
#endif
}

static void
rt_open(){

	int i = 0;
	/* Here user is opening a file. User should specify the appropriate
	permissions/accessibility using flags. defined in uapi/asm-generic/fcntl.h
	flags supported are :
		1. O_APPEND // append the new data, no read access
		2. O_CREAT  // clean the older routing table if already exists 
		3. O_RDONLY // no write access
		4. O_WRONLY // no read access
		5. O_RDWR   // read and write access
		6. O_NONBLOCK // disable blocking calls	
	*/

	if ((rt_fd = open("/dev/rt", O_RDWR /*| O_NONBLOCK*/)) == -1) {
		perror("open failed");
		exit(EXIT_SUCCESS);
	}

	printf("%s() : Success\n", __FUNCTION__);

#if 0
	struct rt_entry entry[4];	
	memset(entry, 0, sizeof(entry));

	strcpy(entry[0].dst_ip,     "10.1.1.1\0");
	strcpy(entry[0].nxt_hop_ip, "10.1.1.2\0");
	strcpy(entry[0].oif, 	    "eth0\0");


	strcpy(entry[1].dst_ip,     "20.1.1.1\0");
	strcpy(entry[1].nxt_hop_ip, "20.1.1.2\0");
	strcpy(entry[1].oif, 	    "eth1\0");
	 
	
	strcpy(entry[2].dst_ip,     "30.1.1.1\0");
	strcpy(entry[2].nxt_hop_ip, "30.1.1.2\0");
	strcpy(entry[2].oif, 	    "eth2\0");
		

	strcpy(entry[3].dst_ip,     "40.1.1.1\0");
	strcpy(entry[3].nxt_hop_ip, "40.1.1.2\0");
	strcpy(entry[3].oif, 	    "eth3\0");

	for(; i < 4 ; i++)
		ioctl(fd, RT_IOC_CR_RTENTRY, &entry[i]);
#endif
}

static void
rt_close(){
	close(rt_fd);
}

static void rt_read_all(){	
	char buf[RT_MAX_ENTRIES_FETCH * sizeof(struct rt_update_t)];
	memset(buf, 0, sizeof(buf));
	int n = read(rt_fd, buf, sizeof(buf));
	if(n < 0)
		printf("Error in rt_read_all()\n");
	if(n == 0){
		printf("No routes found\n");
		return;
	}
	printf("Total Routes = %d\n", n);
	print_rt_fetched_entries(buf, sizeof(buf), n);
	memset(buf, 0, RT_MAX_ENTRIES_FETCH*sizeof(struct rt_update_t));
}


static void mac_read_all(){	
	char buf[MAC_MAX_ENTRIES_FETCH * sizeof(struct mac_update_t)];
	memset(buf, 0, sizeof(buf));
	int n = read(mac_fd, buf, sizeof(buf));
	if(n < 0)
		printf("Error in mac_read_all()\n");
	if(n == 0){
		printf("No routes found\n");
		return;
	}
	printf("Total Routes = %d\n", n);
	print_mac_fetched_entries(buf, sizeof(buf), n);
	memset(buf, 0, MAC_MAX_ENTRIES_FETCH*sizeof(struct mac_update_t));
}

void
ioctl_get_rt_info(){
	struct rt_info_t rt_info;
	memset(&rt_info, 0, sizeof(struct rt_info_t));

	ioctl(rt_fd, RT_IOC_GET_RT_INFO, &rt_info);
	
	printf("rt info : \n");
	printf("	node count = %u\n", rt_info.node_count);
	printf("        Actual node count = %u\n", rt_info.actual_node_count);
	printf("        # of pending updates = %u\n", rt_info.no_of_pending_updates);
	printf("        # no_of_polling_readers = %u\n", rt_info.no_of_polling_readers);
	printf("        # no_of_blacklisted_polling_readers = %u\n", rt_info.no_of_blacklisted_polling_readers);
	printf("---------------------------------\n");
}

static void 
rt_write(){

        struct rt_update_t update_msg;
        int ret = 0;
        char consume_new_line[2];
        memset(&update_msg, 0, sizeof(struct rt_update_t));

        /*consume last \n */

        fgets((char *)consume_new_line, 2, stdin);

        printf("Enter dest ip ? ");
        if((fgets((char *)update_msg.entry.dst_ip, 15, stdin) == NULL)){
                printf("error in reading from stdin\n");
                exit(EXIT_SUCCESS);
        }

        update_msg.entry.dst_ip[strlen(update_msg.entry.dst_ip) - 1] = '\0';
        printf("\nEnter nxt hop ip ? ");


        if((fgets((char *)update_msg.entry.nxt_hop_ip, 15, stdin) == NULL)){
                printf("error in reading from stdin\n");
                exit(EXIT_SUCCESS);
        }


        update_msg.entry.nxt_hop_ip[strlen(update_msg.entry.nxt_hop_ip) - 1] = '\0';

        printf("\nEnter oif ? ");

        if((fgets((char *)update_msg.entry.oif, 15, stdin) == NULL)){
                printf("error in reading from stdin\n");
                exit(EXIT_SUCCESS);
        }

        update_msg.entry.oif[strlen(update_msg.entry.oif) - 1] = '\0';
        update_msg.op_code = RT_ROUTE_ADD;

        ret = write(rt_fd, &update_msg, sizeof(struct rt_update_t));

        if(ret == sizeof(struct rt_update_t)){
                printf("%s(): Success\n", __FUNCTION__);
                return;
        }

        printf("%s(): Failure\n", __FUNCTION__);
}


static void 
mac_write(){
        struct mac_update_t update_msg;
        int ret = 0;
        char consume_new_line[2];
        memset(&update_msg, 0, sizeof(struct mac_update_t));

        /*consume last \n */

        fgets((char *)consume_new_line, 2, stdin);

        printf("Enter vlan id ? ");
        if((fgets((char *)update_msg.entry.vlan_id, 15, stdin) == NULL)){
                printf("error in reading from stdin\n");
                exit(EXIT_SUCCESS);
        }

        update_msg.entry.vlan_id[strlen(update_msg.entry.vlan_id) - 1] = '\0';
        printf("\nEnter mac addr ? ");


        if((fgets((char *)update_msg.entry.mac, 48, stdin) == NULL)){
                printf("error in reading from stdin\n");
                exit(EXIT_SUCCESS);
        }


        update_msg.entry.mac[strlen(update_msg.entry.mac) - 1] = '\0';

        printf("\nEnter oif ? ");

        if((fgets((char *)update_msg.entry.oif, 15, stdin) == NULL)){
                printf("error in reading from stdin\n");
                exit(EXIT_SUCCESS);
        }

        update_msg.entry.oif[strlen(update_msg.entry.oif) - 1] = '\0';
        update_msg.op_code = MAC_ROUTE_ADD;

	ret = write(mac_fd, &update_msg, sizeof(struct mac_update_t));

	if(ret == sizeof(struct mac_update_t)){
                printf("%s(): Success\n", __FUNCTION__);
                return;
        }

        printf("%s(): Failure\n", __FUNCTION__);
}

static void
ioctl_add_mac_route(){
	struct mac_update_t update_msg;
	int ret = 0;
	char consume_new_line[2];
	memset(&update_msg, 0, sizeof(struct mac_update_t));
	
	/*consume last \n */
	
	fgets((char *)consume_new_line, 2, stdin);
		
	printf("Enter vlan id ? ");	
	if((fgets((char *)update_msg.entry.vlan_id, 15, stdin) == NULL)){
		printf("error in reading from stdin\n");
		exit(EXIT_SUCCESS);
	}

	update_msg.entry.vlan_id[strlen(update_msg.entry.vlan_id) - 1] = '\0';
	printf("\nEnter mac addr ? ");
	
	
	if((fgets((char *)update_msg.entry.mac, 48, stdin) == NULL)){
		printf("error in reading from stdin\n");
		exit(EXIT_SUCCESS);
	}

	
	update_msg.entry.mac[strlen(update_msg.entry.mac) - 1] = '\0';

	printf("\nEnter oif ? ");
	
	if((fgets((char *)update_msg.entry.oif, 15, stdin) == NULL)){
		printf("error in reading from stdin\n");
		exit(EXIT_SUCCESS);
	}

	update_msg.entry.oif[strlen(update_msg.entry.oif) - 1] = '\0';
	update_msg.op_code = MAC_ROUTE_ADD;	
	
	ret = ioctl(mac_fd, MAC_IOC_COMMON_UPDATE_MAC, &update_msg);

	if(ret == 0){
		printf("%s(): Success\n", __FUNCTION__);
		return;
	}

	printf("%s(): Failure\n", __FUNCTION__);
}

static void
ioctl_add_rt_route(){
	struct rt_update_t update_msg;
	int ret = 0;
	char consume_new_line[2];
	memset(&update_msg, 0, sizeof(struct rt_update_t));
	
	/*consume last \n */
	
	fgets((char *)consume_new_line, 2, stdin);
		
	printf("Enter dest ip ? ");	
	if((fgets((char *)update_msg.entry.dst_ip, 15, stdin) == NULL)){
		printf("error in reading from stdin\n");
		exit(EXIT_SUCCESS);
	}

	update_msg.entry.dst_ip[strlen(update_msg.entry.dst_ip) - 1] = '\0';
	printf("\nEnter nxt hop ip ? ");
	
	
	if((fgets((char *)update_msg.entry.nxt_hop_ip, 15, stdin) == NULL)){
		printf("error in reading from stdin\n");
		exit(EXIT_SUCCESS);
	}

	
	update_msg.entry.nxt_hop_ip[strlen(update_msg.entry.nxt_hop_ip) - 1] = '\0';

	printf("\nEnter oif ? ");
	
	if((fgets((char *)update_msg.entry.oif, 15, stdin) == NULL)){
		printf("error in reading from stdin\n");
		exit(EXIT_SUCCESS);
	}

	update_msg.entry.oif[strlen(update_msg.entry.oif) - 1] = '\0';
	update_msg.op_code = RT_ROUTE_ADD;	
	
	ret = ioctl(rt_fd, RT_IOC_COMMON_UPDATE_RT, &update_msg);

	if(ret == 0){
		printf("%s(): Success\n", __FUNCTION__);
		return;
	}

	printf("%s(): Failure\n", __FUNCTION__);
}

static void
ioctl_purge_device(){
	int ret = 0;	
	ret = ioctl(rt_fd, RT_IOC_RTPURGE, NULL);
	if(ret == 0){
		printf("%s(): Success\n", __FUNCTION__);
		return;
	}

	printf("%s(): Failure\n", __FUNCTION__);
}

static void*
rt_subscrption_fn(void * arg){
	char *buf = calloc(RT_MAX_ENTRIES_FETCH, sizeof(struct rt_update_t));
	int n = 0, i = 0; // no of routing updates
	struct rt_update_t *update_msg = NULL;

	while(1){
		printf("%s() blocked for update from kernel\n", __FUNCTION__);
		n = ioctl(rt_fd, RT_IOC_SUBSCRIBE_RT, buf);
		printf("No. of updates recieved = %d\n", n);
		// print updates here
		for(i = 0; i < n; i++){
			update_msg = buf;
			printf("%d. op_code = %d\n", i+1, update_msg->op_code);
			print_rt_one_entry(i+1, &update_msg->entry);			
		}
		memset(buf, 0, RT_MAX_ENTRIES_FETCH*sizeof(struct rt_update_t));
	}
	return NULL;
}



void ioctl_subscribe_rt(){
	_pthread_t subscriber_thread;
	int DETACHED =0;
	pthread_init(&subscriber_thread, 0 , DETACHED);
	pthread_create(&(subscriber_thread.pthread_handle), 
		&subscriber_thread.attr, rt_subscrption_fn, NULL);
}

void 
main_menu(){
	int choice;

	/* Add static entries to routing table*/

	while(1){
		printf("Main Menu\n");
		printf("1. open RT\n");
		printf("2. purge RT\n");
		printf("3. Subscribe RT (SYNC with op7/8/9)\n");
		//printf("4. read All from RT\n");
		printf("5. close RT\n");
		printf("6. fork a new process\n");
		printf("7. IOCTL add route (SYNC with 3)\n");
		printf("8. IOCTL delete route (SYNC with 3)\n");
		printf("9. IOCTL update route (SYNC with 3)\n");
		printf("10. IOCTL rt info\n");
		printf("15. write() rt (SYNC with 14)\n");
		printf("------MAC TB operations-----\n");
		printf("11. IOCTL add mac route (SYNC with 3)\n");
		printf("12. open MAC\n");
		//printf("13. read all from MAC\n");
		printf("14. poll the RT and MAC (SYNC with 14/16)\n");
		printf("16. write() mac (SYNC with 14)\n");
		printf("17. exit\n");
		printf("Enter choice (1-9)\n");
		scanf("%d", &choice);
		switch (choice){
			case 1:
				rt_open();
				break;
			case 12:
				mac_open();
				break;
			case 2:
				ioctl_purge_device();
				break;
			case 3:
				ioctl_subscribe_rt();
				break;
			case 4:
				rt_read_all();
				break;
			case 13:
				mac_read_all();
				break;
			case 5:
				rt_close();
				break;
			case 6:
				if(fork() == 0){
					//close(rt_fd);
					scanf("\n");
				}
				break;
			case 7:
				ioctl_add_rt_route();
				break;
			case 8:
				break;
			case 9:
				break;
			case 10:
				ioctl_get_rt_info();
				break;	
			case 11:
				ioctl_add_mac_route();
				break;
			case 14:
				poll_rt_mac_table();
				break;
			case 15:
				rt_write();
				break;
			case 16:
				mac_write();
				break;
			case 17:
				exit(0);
			default:
			
				;
		}	
	}
}


int main(int argc, char **argv) {
	main_menu();
	return 0;
}

