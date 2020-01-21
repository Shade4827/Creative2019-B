//compile: gcc trace.c -o trace
//execute: ./trace

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
#include <time.h>

//モード変更
/**************************/
int MODE_GPIO_NUM[2]={26,44};
char SwitchMode();	//モード切替
void OutputCurrent(double);	//電流出力時の設定
void AttackMode(char[],int*);	//攻撃側の行動関数
void DeffenceMode(char[],int*,int*,clock_t*);	//防衛側の行動関数
/**************************/

//PWM
/**************************/
#define OCP_NUM 3 //ocp.▲の▲に該当する番号
#define PWM_PERIOD 10000000
#define BONE_CAPEMGR_NUM 9 //bone_capemgr.●の●に該当char PIN_PWM[2][7]={{"P9_14"},{"P9_22"}}; //PWM有効化後の番号
char PIN_PWM[2][7]={{"P9_14"},{"P9_22"}}; //PWM有効化後の番号
int PWM_PIN_NUM[2]={15,16}; //PWMに使用するのBBBピン番号
int MOTOR_GPIO_NUM[2][2]={{61,60},{65,46}}; //モータで使用するGPIO番号=32×A+B(GPIO0_3→3)
void initPwm(int); //PWM初期化関数 motorNum=0もしくは1
void runPwm(int,int,int); //モータ用出力関数 motorNum=0もしくは1/driveMode 0:停止，1:正転，-1:逆転
void closePwm(int); //PWM終了関数
/**************************/

//GPIO
/**************************/
void gpioExport(int);	//gpioの有効化関数
void gpioUnexport(int); //gpioの有効化解除の関数
int gpioOpen(int, char*, int);	//gpioの設定ファイルを開く関数
/**************************/

//超音波センサ
/**************************/
int SENSOR_GPIO_NUM=20;
/**************************/

//ライントレーサ
/**************************/
int LINE_GPIO_NUM[8]={49,115,3,27,48,47,30,45};	//ライントレーサで使用するGPIO番号
void isRidingLine(int[],char[],int);	//ライントレーサの判定
/**************************/

//移動関数
/**************************/
//右:左=100:78
const int RIGHT1 100000	//右旋回
const int RIGHT2 1000	//左旋回
const int LEFT1 1000	//右旋回
const int LEFT2 100000	//左旋回
void MoveRight();	//右旋回
void MoveLeft();	//左旋回
void MoveStraight();	//直進
void MoveBack();	//後退
void Stop();	//停止
/**************************/

int main(){
	int i,lineNum=8;
	char isRide[8];	//'0':白 '1':黒
	int turnFlag=0;	//0:旋回しない 1:右旋回 -1:左旋回
	int startFlag=1;	//発射時のフラグ
	int count=0;	//防衛時行動管理変数
	clock_t start;
	
	//モータを起動
	initPwm(0);
	initPwm(1);

	//GPIOをセット
	for(i=0;i<8;i++){
		gpioExport(LINE_GPIO_NUM[i]);
	}
	/* 攻守切り替え部分(スイッチ取り付け後使用)
	OutputCurrent(n);	//電流出力(n秒間)

	int mode=SwitchMode();	//攻守のモード 0:攻撃 1:防衛
	*/
	int mode=0;

	while(1){
		//ライントレーサによる判定
		isRidingLine(LINE_GPIO_NUM,isRide,lineNum);

		//色付きマスから前進して抜け出す
		if(startFlag==1){
			if(isRide=="00011000"){
				startFlag=0;
			}
			MoveStraight();
		}
		//攻撃モード
		else if(mode==0){
			AttackMode(isRide,&turnFlag);
		}
		//防衛モード
		else if(mode==1){
			DeffenceMode(isRide,&turnFlag,&count,&start);
		}
	}

	//モータを停止
	Stop():
	closePwm(0);
	closePwm(1);

	return 0;
}

/* モード変更 */
//攻守切替
char SwitchMode(){
	int i,fd;
	char path[60],mode=0;
	FILE *fp;

	//出力側を設定
	fd=gpioOpen(MODE_GPIO_NUM[0], "direction", O_WRONLY);
	write(fd, "out", 3);
	close(fd);

	sprintf(path, "/sys/class/gpio/gpio%d/value", MODE_GPIO_NUM[0]);
	fp = fopen(path, "w");
	fprintf(fp, "%d", 0);
	fclose(fp);

	//入力側を設定
	fd = gpioOpen(MODE_GPIO_NUM[1], "value", O_RDONLY);
	read(fd, &mode, 1);
	close(fd);

	for(i=0;i<2;i++){
		gpioUnexport(MODE_GPIO_NUM[i]);
	}

	return mode;
}

//電流出力時の設定
void OutputCurrent(double time){
	int i,fd;
	char path[60];
	FILE *fp;

	for(i=0;i<2;i++){
		gpioExport(MODE_GPIO_NUM[i]);	
	}

	//出力側を設定
	fd=gpioOpen(MODE_GPIO_NUM[0], "direction", O_WRONLY);
	write(fd, "out", 3);
	close(fd);

	sprintf(path, "/sys/class/gpio/gpio%d/value", MODE_GPIO_NUM[0]);
	fp = fopen(path, "w");
	fprintf(fp, "%d", 1);
	fclose(fp);

	//入力側を設定
	fd=gpioOpen(MODE_GPIO_NUM[0], "direction", O_WRONLY);
	write(fd, "in", 3);
	close(fd);

	sleep(time);	//time秒間停止
}

//攻撃側の行動関数
void AttackMode(char isRide[],int *turnFlag){
	//右旋回
	if(turnFlag==1){
		MoveRight();
		if(isRide[3]=='0' && isRide[4]=='0'){
			turnFlag=0;
		}
	}
	//左旋回
	else if(turnFlag==-1){
		MoveLeft();
		if(isRide[3]=='0' && isRide[4]=='0'){
			turnFlag=0;
		}
	}
	//右2つ反応
	else if(isRide[4]=='0' && isRide[5]=='0'){
		//右側3つ反応→右旋回
		if(isRide[7]=='0'){
			MoveRight();
			turnFlag=1;
		}
		//道から外れていれば左旋回
		else{
			MoveLeft();		//左
		}
	}
	//真ん中2つ反応→前進
	else if(isRide[3]=='0' && isRide[4]=='0'){
		MoveStraight();
	}
	//左2つ反応
	else if(isRide[2]=='0' && isRide[3]=='0'){
		//左側4つ反応→左旋回
		if(isRide[0]=='0'){
			MoveLeft();
			turnFlag=-1;
		}
		//道から外れていれば右旋回
		else{
			MoveRight();	//右
		}
	}
	else{
		Stop();
	}
}

//防衛側の行動関数
void DeffenceMode(char isRide[],int *turnFlag,int *count,clock_t *start){
	//前左左前右右フラグ前左ループ
	//次の角まで直進
	if(turnFlag==2)[
		MoveStraight();
		if(isRide[2]=='1' && isRide[3]=='1'){
			turnFlag==0;
		}
	]
	//左旋回
	if(turnFlag==-1){
		MoveLeft();
		if(isRide[3]=='0' && isRide[4]=='0'){
			turnFlag=0;
			end=clock();
			if((double)(end-start)/CLOCKS_PER_SEC>=2){
				count++;
				start=clock();
			}
		}	
	}
	//右旋回
	else if(turnFlag==1){
		MoveRight();
		if(isRide[3]=='0' && isRide[4]=='0'){
			turnFlag=0;
			end=clock();
			if((double)(end-start)/CLOCKS_PER_SEC>=2){
				count++;
				start=clock();
			}
		}
	}
	//左2つ反応
	else if(isRide[2]=='0' && isRide[3]=='0'){
		//左側4つ反応→左旋回
		if(count==4){
			turnFlag=2;
		}
		else if(isRide[0]=='0'){
			MoveLeft();
			turnFlag=-1;
			start=clock();
		}
		//道から外れていれば右旋回
		else{
			MoveRight();	//右
		}
	}
	//真ん中2つ反応→前進
	else if(isRide[3]=='0' && isRide[4]=='0'){
		MoveStraight();
	}
	//右2つ反応
	else if(isRide[4]=='0' && isRide[5]=='0'){
		//右側3つ反応→右旋回
		if(isRide[7]=='0'){
			MoveRight();
			turnFlag=1;
			start=clock();
		}
		//道から外れていれば左旋回
		else{
			MoveLeft();		//左
		}
	}
	else{
		Stop();
	}
}

/* PWM */
//PWM初期化関数
void initPwm(int motorNum){
	int i,fd;
	char path[60],path3[60],path4[60];
	FILE *fp;

	for(i=0;i<2;i++){
		gpioExport(MOTOR_GPIO_NUM[motorNum][i]);
		
		fd=gpioOpen(MOTOR_GPIO_NUM[motorNum][i], "direction", O_WRONLY);
		write(fd, "out", 3);
		close(fd);

		sprintf(path3, "/sys/class/gpio/gpio%d/value", MOTOR_GPIO_NUM[motorNum][i]);
		fp = fopen(path3, "w");
		fprintf(fp, "%d", 0);
		fclose(fp);
	}

	/*PWM機能の有効化*/
	
	sprintf(path4, "/sys/devices/bone_capemgr.%d/slots", BONE_CAPEMGR_NUM);
	fp = fopen(path4,"w");
	fprintf(fp, "am33xx_pwm");
	fclose(fp);

	/*ピンの設定(PIN_PWM指定のピン)*/
	sprintf(path, "bone_pwm_%s",PIN_PWM[motorNum]);
	sprintf(path4, "/sys/devices/bone_capemgr.%d/slots", BONE_CAPEMGR_NUM);
	fp = fopen(path4,"w");
	fprintf(fp, path);
	fclose(fp);

	/*安全のため，PWM出力の停止*/
	sprintf(path, "/sys/devices/ocp.%d/pwm_test_%s.%d/run",OCP_NUM, PIN_PWM[motorNum], PWM_PIN_NUM[motorNum]);
	fp = fopen(path, "wb");
	fprintf(fp, "%d", 0);
	fclose(fp);

	/*PWM周期の設定*/
	sprintf(path, "/sys/devices/ocp.%d/pwm_test_%s.%d/period",OCP_NUM, PIN_PWM[motorNum], PWM_PIN_NUM[motorNum]);
	fp = fopen(path, "wb");
	fprintf(fp, "%d", PWM_PERIOD);
	fclose(fp);

	/*PWM極性の設定*/
	sprintf(path, "/sys/devices/ocp.%d/pwm_test_%s.%d/polarity",OCP_NUM, PIN_PWM[motorNum], PWM_PIN_NUM[motorNum]);
	fp=fopen(path, "wb");
	fprintf(fp, "%d", 0);
	fclose(fp);

	/*PWM ON状態時間の初期化*/
	sprintf(path, "/sys/devices/ocp.%d/pwm_test_%s.%d/duty",OCP_NUM, PIN_PWM[motorNum], PWM_PIN_NUM[motorNum]);
	fp=fopen(path, "wb");
	fprintf(fp, "%d", 0);
	fclose(fp);

	/*PWM出力の開始*/
	sprintf(path, "/sys/devices/ocp.%d/pwm_test_%s.%d/run",OCP_NUM, PIN_PWM[motorNum], PWM_PIN_NUM[motorNum]);
	fp=fopen(path, "wb");
	fprintf(fp, "%d", 1);
	fclose(fp);

}

//PWM終了関数
void closePwm(int motorNum){
	FILE *fp;
	char path[60];
	int i;
	
	/*PWM　duty0出力*/
	sprintf(path, "/sys/devices/ocp.%d/pwm_test_%s.%d/duty",OCP_NUM, PIN_PWM[motorNum], PWM_PIN_NUM[motorNum]);
	fp=fopen(path, "wb");
	fprintf(fp, "%d", 0);
	fclose(fp);
	
	/*PWM出力の停止*/
	sprintf(path, "/sys/devices/ocp.%d/pwm_test_%s.%d/run",OCP_NUM, PIN_PWM[motorNum], PWM_PIN_NUM[motorNum]);
	fp=fopen(path, "wb");
	fprintf(fp, "%d", 0);
	fclose(fp);

	//GPIOの解放
	for(i=0;i<2;i++){
		gpioUnexport(MOTOR_GPIO_NUM[motorNum][i]);
	}
}

//モータ用出力関数
void runPwm(int motorNum,int duty,int driveMode){
	int i;
	char path[60],path3[60];
	FILE *fp;

	//一時停止
	if(driveMode==0){
		for(i=0;i<2;i++){
			sprintf(path3, "/sys/class/gpio/gpio%d/value", MOTOR_GPIO_NUM[motorNum][i]);
			fp = fopen(path3, "w");
			fprintf(fp, "%d", 1);
			fclose(fp);
		}
	}
	
	//モータ正転
	else if(driveMode==1){
		for(i=0;i<2;i++){
			sprintf(path3, "/sys/class/gpio/gpio%d/value", MOTOR_GPIO_NUM[motorNum][i]);
			fp = fopen(path3, "w");
			if(i==0){
				fprintf(fp, "%d", 1);
				fclose(fp);
			}
			else{
				fprintf(fp, "%d", 0);
				fclose(fp);
			}
		}

	}

	//モータ逆転
	else if(driveMode==-1){
		for(i=0;i<2;i++){
			sprintf(path3, "/sys/class/gpio/gpio%d/value", MOTOR_GPIO_NUM[motorNum][i]);
			fp = fopen(path3, "w");
			if(i==0){
				fprintf(fp, "%d", 0);
				fclose(fp);
			}
			else{
				fprintf(fp, "%d", 1);
				fclose(fp);
			}
		}
	}

	//入力したdutyでPWM信号を出力
	sprintf(path, "/sys/devices/ocp.%d/pwm_test_%s.%d/duty",OCP_NUM, PIN_PWM[motorNum], PWM_PIN_NUM[motorNum]);
	fp=fopen(path, "wb");
	fprintf(fp, "%d", duty);
	fclose(fp);
	usleep(200);
}

/* GPIO */
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

/* ライントレーサ */
//ライントレーサの判定関数
void isRidingLine(int gpioNum[],char c[],int n){
	int i,fd;
	for(i=0;i<n;i++){
		fd = gpioOpen(gpioNum[i], "value", O_RDONLY);
		lseek(fd, 0, SEEK_SET);
		read(fd, &c[i], 1);
		close(fd);
	}
}

/* 移動 */
//右旋回
void MoveRight(){
	runPwm(0,LEFT1,1);
	runPwm(1,RIGHT1,1);
}

//左旋回
void MoveLeft(){
	runPwm(0,LEFT2,1);
	runPwm(1,RIGHT2,1);
}

//前進
void MoveStraight(){
	runPwm(0,LEFT1,1);
	runPwm(1,RIGHT2,1);
}

//後退
void MoveBack(){
	runPwm(0,-LEFT1,1);
	runPwm(1,-RIGHT2,1);
}

//停止
void Stop(){
	runPwm(0,0,0);
	runPwm(1,0,0);
}