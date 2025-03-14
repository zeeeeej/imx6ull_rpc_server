#include <jsonrpc-c.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "dht11.h"
#include "led.h"

static struct jrpc_server my_server;
#define PORT 1234

// cache
static int cache_humi = -1;
static int cache_temp = -1;
static int cache_led = -1;
// cache end



/*
 * rpc_led_control
 * params:{"params":[0|1]}
 */
cJSON * server_led_control(jrpc_context * ctx, cJSON * params, cJSON *id) {
    
    cJSON *status = cJSON_GetArrayItem(params, 0);
    led_control(status->valueint);
    cache_led = status->valueint;
    return cJSON_CreateNumber(0);
}

/*
 * rpc_dht11_read
 * return:{"params":[humi,temp]}
 */
cJSON * server_dht11_read(jrpc_context * ctx, cJSON * params, cJSON *id) {
    int array[2] = {(int)cache_humi,(int)cache_temp};
    return cJSON_CreateIntArray(array,2);
}


void * dht11_read_thread(void * arg){
    unsigned char humi;
    unsigned char temp;
    while(1)
    {
    
    	while(0!=dht11_read(&humi,&temp)){
    		sleep(1);
   	}
   	//printf("rpc server dht11_read humi:%d,temp:%d\n",humi,temp);
   	cache_humi = (int)humi;
    	cache_temp = (int)temp;
	sleep(1);
    
    }
    return NULL;
}

/**
 * rpc_led_read
 * return :{"params":1}
 *
 */

cJSON * server_led_read(jrpc_context * ctx, cJSON * params, cJSON *id) {
//	unsigned char status;
//    while(0!=led_read(&status));
    return cJSON_CreateNumber((int)cache_led);
}

int RPC_Server_Init(void) 
{
    int err;
    
    err = jrpc_server_init(&my_server, PORT);
    if (err)
    {
        printf("jrpc_server_init err : %d\n", err);
    }
    
    jrpc_register_procedure(&my_server, server_led_control, "led_control", NULL );
    jrpc_register_procedure(&my_server, server_dht11_read, "dht11_read", NULL );
    jrpc_register_procedure(&my_server, server_led_read, "led_read", NULL );
	
    pthread_t threadId;
    int result = pthread_create(&threadId, NULL, dht11_read_thread, NULL);
    if (result != 0) {
        printf("Failed to create thread!\n");
        return 1;
    }    
	
    jrpc_server_run(&my_server);
    jrpc_server_destroy(&my_server);

    return 0;
}


int main(int argc, char **argv)
{
	
	led_init();
	dht11_init();
	RPC_Server_Init();
	return 0;
}


