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
#include <poll.h>
#include <termios.h>
#include "gpio.h"

//ボード毎の設定
/**************************/
#define OCP_NUM 3 //ocp.▲の▲に該当する番号
#define PWM_PERIOD 10000000
#define BONE_CAPEMGR_NUM 9 //bone_capemgr.●の●に該当
/**************************/

//ライントレーサを仕様する際の設定
/**************************/
int LINE_GPIO_NUM[8]={30,48,3,49,115,27,47,45};	//ライントレーサで使用するGPIO番号
/**************************/

int kbhit(void); //キー入力関数
char isRidingLine(int n);	//ライントレーサの判定関数

/*********************************/
//initPwm(モータ番号)を呼び出して，初期化の設定を行う，モータ番号（0～接続個数-1）
//runPwm(int motorNum,int duty,int driveMode)を呼びだして動かす．motorNum：モータ番号/duty：整数値/driveMode：停止，正転，逆転
//closePwm(モータ番号)を呼び出して，終了の処理を実施
/*********************************/


//このプログラムは，モータ0番が常に停止している状態
//モータ1番に関しては何もしていない
int main(){
	int i;
	char isRide[8];
	
	//モータを起動
	initPwm(0);
	initPwm(1);
	
	while(1){
		//ライントレーサによる判定
		for(i=0;i<8;i++){
			isRide[i]=isRidingLine(LINE_GPIO_NUM[i]);
		}

		//行動決定
		if(isRide[0]==1 && isRide[1]==1 && isRide[2]==1 && isRide[3]==1){
			//右旋回
			runPwm(0,100000,-1);
			runPwm(1,100000,1);
			printf("right\n");
		}
		else if(isRide[4]==1 && isRide[5]==1 && isRide[6]==1 && isRide[7]==1){
			//左旋回
			runPwm(0,100000,1);
			runPwm(1,100000,-1);
			printf("left\n");
		}
		else if(isRide[3]==1 && isRide[4]==1){
			//前進
			runPwm(0,100000,1);
			runPwm(1,100000,1);
			printf("run\n");
		}
		else {}

		//モータを回転
		runPwm(0,100000,1);
		runPwm(1,100000,1);
		printf("run\n");

		//キー入力関数
		if(kbhit()) {
			if(getchar()=='q')	//「q」で終了
				break;
		}
	}

	//モータを停止
	runPwm(0,0,0);
	closePwm(0);
	runPwm(1,0,0);
	closePwm(1);

	return 0;
}

int kbhit(void){
	struct termios oldt, newt;
	int ch;
	int oldf;

	tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

//ライントレーサの判定関数
char isRidingLine(int n){
	char c;
	int fd = gpioOpen(gpio_number, "value", O_RDONLY);
	read(fd, &c, 1);
	close(fd);

	return c;
}	