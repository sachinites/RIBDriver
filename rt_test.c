#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <linux/fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "kern_usr.h"

int fd = 0;


static void
print_rt_one_entry(unsigned int i, struct rt_entry *entry){
	printf("    |------|----------------------|----------------------|--------------|\n");
	printf("    | %-4d | %-20s | %-20s | %-12s |\n", i, entry->dst_ip, entry->nxt_hop_ip, entry->oif);
}


void
print_rt_fetched_entries(char *buf, unsigned int size, unsigned int units){
	struct rt_entry *entry = (struct rt_entry *)buf;
	unsigned int j = 0;
	for( ; j < units; j++)
		print_rt_one_entry(j+1 , entry + j);		
	printf("    |------|----------------------|----------------------|--------------|\n");
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

	if ((fd = open("/dev/rt", O_RDWR)) == -1) {
		perror("open failed");
		exit(EXIT_SUCCESS);
	}

	printf("%s() : Success\n", __FUNCTION__);


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
}

static void
rt_close(){
	close(fd);
}

static void rt_read_all(){	
	char buf[MAX_ENTRIES_FETCH * sizeof(struct rt_entry)];
	memset(buf, 0, sizeof(buf));
	int n = read(fd, buf, sizeof(buf));
	if(n < 0)
		printf("Error in rt_read_all()\n");
	if(n == 0){
		printf("No routes found\n");
		return;
	}
	printf("Total Routes = %d\n", n);
	print_rt_fetched_entries(buf, sizeof(buf), n);
}


void
ioctl_get_rt_info(){
	struct rt_info_t rt_info;
	memset(&rt_info, 0, sizeof(struct rt_info_t));
	unsigned int bytes = 0;

	printf("user address = 0x%x\n", (unsigned int)&rt_info);
	bytes = ioctl(fd, RT_IOC_GET_RT_INFO, &rt_info);
	
	printf("rt info : \n");
	printf("	node count = %u\n", rt_info.node_count);
	printf("        Actual node count = %u\n", rt_info.actual_node_count);
	printf("---------------------------------\n");
}

static void 
rt_write(){}

static void
ioctl_add_route(){
	struct rt_entry entry;
	int ret = 0;
	char consume_new_line[2];
	memset(&entry, 0, sizeof(struct rt_entry));
	
	/*consume last \n */
	
	fgets((char *)consume_new_line, 2, stdin);
		
	printf("Enter dest ip ? ");	
	if((fgets((char *)entry.dst_ip, 15, stdin) == NULL)){
		printf("error in reading from stdin\n");
		exit(EXIT_SUCCESS);
	}

	entry.dst_ip[strlen(entry.dst_ip) - 1] = '\0';
	printf("\nEnter nxt hop ip ? ");
	
	
	if((fgets((char *)entry.nxt_hop_ip, 15, stdin) == NULL)){
		printf("error in reading from stdin\n");
		exit(EXIT_SUCCESS);
	}

	
	entry.nxt_hop_ip[strlen(entry.nxt_hop_ip) - 1] = '\0';

	printf("\nEnter oif ? ");
	
	if((fgets((char *)entry.oif, 15, stdin) == NULL)){
		printf("error in reading from stdin\n");
		exit(EXIT_SUCCESS);
	}

	entry.oif[strlen(entry.oif) - 1] = '\0';
	
	ret = ioctl(fd, RT_IOC_CR_RTENTRY, &entry);

	if(ret == 0){
		printf("%s(): Success\n", __FUNCTION__);
		return;
	}

	printf("%s(): Failure\n", __FUNCTION__);
}

static void
ioctl_purge_device(){
	int ret = 0;	
	ret = ioctl(fd, RT_IOC_RTPURGE, NULL);
	if(ret == 0){
		printf("%s(): Success\n", __FUNCTION__);
		return;
	}

	printf("%s(): Failure\n", __FUNCTION__);
}


void 
main_menu(){
	int choice;

	/* Add static entries to routing table*/


	while(1){
		printf("Main Menu\n");
		printf("1. open RT\n");
		printf("2. purge RT\n");
		printf("3. Block read\n");
		printf("4. read All from RT\n");
		printf("5. close RT\n");
		printf("6. fork a new process\n");
		printf("7. IOCTL add route\n");
		printf("8. IOCTL delete route \n");
		printf("9. IOCTL update route\n");
		printf("10. IOCTL rt info\n");
		printf("Enter choice (1-9)\n");
		scanf("%d", &choice);
		switch (choice){
			case 1:
				rt_open();
				break;
			case 2:
				ioctl_purge_device();
				break;
			case 3:
				//ioctl_add_route();
				break;
			case 4:
				rt_read_all();
				break;
			case 5:
				rt_close();
				break;
			case 6:
				if(fork() == 0){
					//close(fd);
					scanf("\n");
				}
				break;
			case 7:
				ioctl_add_route();
				break;
			case 8:
				break;
			case 9:
				break;
			case 10:
				ioctl_get_rt_info();
				break;	
			default:
			
				;
		}	
	}
}


int main(int argc, char **argv) {
	main_menu();
	return 0;
}
