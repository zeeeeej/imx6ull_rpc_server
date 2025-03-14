#include "led.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

static int fd;

static unsigned char my_status=0;

/*
 * ./led_test <0|1|2|..>  on
 * ./led_test <0|1|2|..>  off
 * ./led_test <0|1|2|..>
 */

void led_init()
{

    fd = open("/dev/100ask_led",O_RDWR);
    if(fd<0){
        printf( "led_init open /dev/100ask_led fail");
        return;
    }
}

void led_control(int on)
{
    char buf[2];
    if(fd<0){
        printf( "led_control /dev/100ask_led not opened.");
        return;
    }
    // open first led.
    buf[0]=0;
    if(on){
        buf[1] = 0;
	my_status = 0;
    }else{
      buf[1] = 1;
      my_status = 1;
    }
    write(fd,buf,2);
}

int led_read(unsigned char*status){
    *status = my_status;
}
