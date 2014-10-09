/*
 * main.c : test demo driver
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "music.h"
#include "ioctl.h"

int main()
{
	int i = 0;
	int n = 2;
	int dev_fd;
	int div;
	dev_fd = open("/dev/fspwm",O_RDWR | O_NONBLOCK);
	if ( dev_fd == -1 ) {
		perror("open");
		exit(1);
	}
	ioctl(dev_fd,SET_PRE,255);
	ioctl(dev_fd,BEEP_ON);

	for(i = 0;i<sizeof(MumIsTheBestInTheWorld)/sizeof(Note);i++ )
	{
		div = (PCLK/256/4)/(MumIsTheBestInTheWorld[i].pitch);
		ioctl(dev_fd, SET_CNT, div);
		usleep(MumIsTheBestInTheWorld[i].dimation * 100); 
	}

	for(i = 0;i<sizeof(GreatlyLongNow)/sizeof(Note);i++ )
	{
		div = (PCLK/256/4)/(GreatlyLongNow[i].pitch);
		ioctl(dev_fd, SET_CNT, div);
		usleep(GreatlyLongNow[i].dimation * 100); 
	}

	for(i = 0;i<sizeof(FishBoat)/sizeof(Note);i++ )
	{
		div = (PCLK/256/4)/(FishBoat[i].pitch);
		ioctl(dev_fd, SET_CNT, div);
		usleep(FishBoat[i].dimation * 100); 
	}
	ioctl(dev_fd,BEEP_OFF,255);

	return 0;
}
