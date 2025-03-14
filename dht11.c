#include "dht11.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static int fd;
static unsigned char g_humi,g_temp;

void *dht11_thread(void * arg){
   unsigned char buf[2] ={0};


		while(1){
		  if (read(fd, buf, 2) == 2)
			    {
			      g_humi = buf[0];
			      g_temp = buf[1];
			      

			    }
			sleep(2);

		}
	
          
}


void dht11_init(){
	pthread_t tid;
    	fd = open("/dev/mydht11", O_RDWR | O_NONBLOCK);
    	if(fd<0){
           	printf( "dht11_read open /dev/mydht1 fail fd:");
    	}else{
		pthread_create(&tid,NULL,dht11_thread,NULL);
	}
}


 int dht11_read(unsigned char*temp,unsigned char*humi)
{
	*humi = g_humi;
	*temp = g_temp;
	return 0;
  




}
