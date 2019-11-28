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
#include "pwm.h"

#define RIGHT1 10000000	//右旋回
#define RIGHT2 100000	//左旋回
#define LEFT1 100000	//右旋回
#define LEFT2 10000000	//左旋回

//モード変更の設定
/**************************/
int MODE_GPIO_NUM[2]={26,44};
/**************************/

//超音波センサの設定
/**************************/
int SENSOR_GPIO_NUM=20;
/**************************/

//ライントレーサを使用する際の設定
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
	char isRide[8];	//0:乗っていない 1:乗っている
	char isTurn=0;	//0:旋回しない 1:右旋回 -1:左旋回
	
	//モータを起動
	initPwm(0);
	initPwm(1);
	
	while(1){
		//ライントレーサによる判定
		for(i=0;i<8;i++){
			isRide[i]=isRidingLine(LINE_GPIO_NUM[i]);
		}

		//行動決定
		//右旋回
		if(isTurn==1){
			MoveRight();
			if(isRide[3] && isRide[4]==1){
				isTurn=0;
			}
		}
		//左旋回
		else if(isTurn==-1){
			MoveLeft();
			if(isRide[3] && isRide[4]==1){
				isTurn=0;
			}
		}
		//右2つ反応
		else if(isRide[4]==1 && isRide[5]==1){
			//右側4つ反応→右旋回
			if(isRide[3]==1 && isRide[6]==1){
				MoveRight();
				isTurn=1;
			}
			//道から外れていれば左旋回
			else{
				MoveLeft();		//左
			}
		}
		//真ん中2つ反応→前進
		else if(isRide[3]==1 && isRide[4]==1){
			MoveStraight();
		}
		//左2つ反応
		else if(isRide[2]==1 && isRide[3]==1){
			//左側4つ反応→左旋回
			if(isRide[1]==1 && isRide[4]==1){
				MoveLeft();
				isTurn=-1;
			}
			//道から外れていれば右旋回
			else{
				MoveRight();	//右
			}
		}
		else{
			MoveStraight();
		}
	}

	//モータを停止
	runPwm(0,0,0);
	closePwm(0);
	runPwm(1,0,0);
	closePwm(1);

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

void MoveRight(){
	//右旋回
	runPwm(0,RIGHT1,1);
	runPwm(1,LEFT1,1);
	printf("right\n");
}

void MoveLeft(){
	//左旋回
	runPwm(0,RIGHT2,1);
	runPwm(1,LEFT2,1);
	printf("left\n");
}

void MoveStraight(){
	//前進
	runPwm(0,RIGHT1,1);
	runPwm(1,LEFT2,1);
	printf("run\n");
}