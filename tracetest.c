//compile: gcc Motor.c gpio.c pwm.c -o motor
//execute: ./motor

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <poll.h>
#include <termios.h>

void gpioExport(int);	//gpioの有効化関数
void gpioUnexport(int); //gpioの有効化解除の関数
int gpioOpen(int, char*, int);	//gpioの設定ファイルを開く関数

//ライントレーサを使用する際の設定
/**************************/
int LINE_GPIO_NUM[8]={49,115,3,27,48,47,30,45};	//ライントレーサで使用するGPIO番号
void isRidingLine(int[],char[],int);	//ライントレーサの判定
/**************************/

//このプログラムは，モータ0番が常に停止している状態
//モータ1番に関しては何もしていない
int main(){
	int i,n=8;
	char isRide[8];	//0:白 1:黒
	
	for(i=0;i<n;i++){
		gpioExport(LINE_GPIO_NUM[i]);
	}

	while(1){
		//ライントレーサによる判定
		isRidingLine(LINE_GPIO_NUM,isRide,n);
		for(i=0;i<n;i++){
			printf("%c ",isRide[i]);
		}
		printf("\n");
    }

	for(i=0;i<n;i++){
		gpioUnexport(LINE_GPIO_NUM[i]);
	}

	return 0;
}

//ライントレーサの判定関数
void isRidingLine(int gpio_number[],char c[],int n){
	int i,fd;
	for(i=0;i<n;i++){
		fd = gpioOpen(gpio_number[i], "value", O_RDONLY);
		lseek(fd,0,SEEK_SET);
		read(fd, &c[i], 1);
		close(fd);
	}
}

//gpioの有効化関数
void gpioExport(int n){
	int fd;
	char buf[40];

	sprintf(buf, "%d", n);

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, buf, strlen(buf));
	close(fd);
}

//gpioの有効化解除の関数
void gpioUnexport(int n){
	int fd;
	char buf[40];

	sprintf(buf, "%d", n);

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	write(fd, buf, strlen(buf));
	close(fd);
}

//gpioの設定ファイルを開く関数
int gpioOpen(int n, char *file, int flag){
	int fd;
	char buf[40];

	sprintf(buf, "/sys/class/gpio/gpio%d/%s", n, file);

	fd = open(buf, flag);
	return fd;
}