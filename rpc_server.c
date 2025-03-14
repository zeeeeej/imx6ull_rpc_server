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
#include "cfg.h"
#include <MQTTClient.h>
#include "cJSON.h"
#include <jsonrpc-c.h>

static struct jrpc_server my_server;
#define PORT 			1234

#define QOS        		0
#define TOPIC_UP 		"/iot/up"
#define TOPIC_DOWN 		"/iot/down"
#define CA_CERTIFICATE_FILE_Path "/etc/mqtt.crt"




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
    //jrpc_register_procedure(&my_server, server_led_read, "led_read", NULL );
	
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


/* MQTT START*/
static MQTTClient client;
static volatile MQTTClient_deliveryToken deliveredtoken;

MQTTClient_SSLOptions configureSSLOptions() {
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    ssl_opts.enableServerCertAuth = 1;
    // CA_CERTIFICATE_FILE_Path specify server CA
     ssl_opts.trustStore = CA_CERTIFICATE_FILE_Path;
     //ssl_opts.verify=0;
    return ssl_opts;
}

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
	printf("======> mqtt msgarrvd !\n");
	int rc;
    int i;
    char* payloadptr;
    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");
    payloadptr = message->payload;
    /* 解析控制设备 */
	/* 消息格式：{"cmd":"set","params":{"led0":1}} */
	cJSON *root = cJSON_Parse(payloadptr);
	cJSON *cmd = cJSON_GetObjectItem(root,"cmd");
	cJSON *params = cJSON_GetObjectItem(root,"params");
	if(params){
		cJSON *led0 = cJSON_GetObjectItem(params,"led0");
		if(led0){
			led_control(led0->valueint);
		}
	}
	cJSON_Delete(root);
	
	
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}
void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

void*publish_thread(void*arg){
	char buf[1000];

	while(1){

		unsigned char humi,temp,led0;
		/* read */
		int ret =  dht11_read(&temp,&humi);
		int ret2 =  led_read(&led0);
		if(ret==0 && ret2==0){
				printf("publishing...\n");
				MQTTClient_message pubmsg = MQTTClient_message_initializer;
				char * PAYLOAD = "\
						\"cmd\":\"report\",\
						\"params\":{\
							\"humi\":\"%d\",\
							\"temp\":\"%d\",\
							\"led0\":\"%d\"\
						}\
					";
				sprintf(buf,humi,temp,led0);
				pubmsg.payload = buf;
				pubmsg.payloadlen = strlen(buf);
				pubmsg.qos = QOS;
				pubmsg.retained = 0;
				MQTTClient_publishMessage(client, TOPIC_UP, &pubmsg, &deliveredtoken);
		}
		sleep(5);
	}
	return NULL;
}



void mqtt_init(){

	/* 1.读取mqtt配置文件 */
	char uri[1000];
	char clientId[1000];
	char username[1000];
	char password[1000];
	char productKey[1000];
	char deviceName[1000];
	int index=0;
	while(index<5){
		if(0!=mqtt_read_cfg( uri,  clientId, username,  password,  productKey,  deviceName)){
			printf("mqtt_read_cfg fail\n");
			sleep(1);
			index++;
		}else{
			break;	
		}
	}
	/* 链接mqtt */
	printf("======> mqtt start imx6ull sync !\n");
	int rc;
	MQTTClient_create(&client, uri, clientId, MQTTCLIENT_PERSISTENCE_NONE, NULL);
 	MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	conn_opts.username = username;
	conn_opts.password = password;
	printf("mqtt version :%d\n",conn_opts.MQTTVersion);
	//conn_opts.MQTTVersion = MQTTVERSION_5;
	conn_opts.MQTTVersion = MQTTVERSION_3_1_1;
	MQTTClient_SSLOptions ssl_opts = configureSSLOptions();
	conn_opts.ssl = &ssl_opts;
	// 
	while(1){
		if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
    			printf("Failed to connect, return code %d\n", rc);
		//	exit(-1);
			sleep(1);
		} else {
   	 		printf("Connected to MQTT Broker!\n");
			break;
		}
	}
	
	/* create publish thread. */
	/* 读取led 温湿度信息，发布到broker	*/
	pthread_t thread_id;
	int ret = pthread_create(&thread_id,NULL,publish_thread,NULL);
	printf("create publish thread ret = %d\n",ret);
	/* 订阅消息 控制设备 */
	printf("Subscribing to topic %s for client %s using QoS%d\n\n", TOPIC_DOWN, clientId, QOS);
    MQTTClient_subscribe(client, TOPIC_DOWN, QOS);
	
	MQTTClient_waitForCompletion(client,deliveredtoken,10000);
//	MQTTClient_disconnect(client, 10000);
   // MQTTClient_destroy(&client);
	printf(">>>>>>>>>>>>>>>>>>>>>>>> mqtt_init end!!!!!!!!\n");
	
}

/* MQTT END*/


int main(int argc, char **argv)
{
	printf("<<<<<<<<< led_init <<<<<<<<<<<<\n");
	led_init();
	printf("<<<<<<<<< dht11_init <<<<<<<<<<<<\n");
	dht11_init();	
	printf("<<<<<<<<< mqtt_init <<<<<<<<<<<<\n");
	mqtt_init();
	printf("<<<<<<<<< rpc_server_init <<<<<<<<<<<<\n");
	RPC_Server_Init();
	return 0;
}


