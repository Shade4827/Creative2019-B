//compile: gcc -lrt trace.c -o trace
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
int MODE_GPIO_NUM=26;
int SwitchMode(struct pollfd*);	//モード切替
void AttackMode(char[],int*);	//攻撃側の行動関数
void DeffenceMode(char[],int*,int*,clock_t*);	//防衛側の行動関数
int IsProgress(double,clock_t*);	//時間経過を判定する関数
/**************************/

//PWM
/**************************/
#define OCP_NUM 3 //ocp.▲の▲に該当する番号
#define PWM_PERIOD 10000000
#define BONE_CAPEMGR_NUM 9 //bone_capemgr.●の●に該当char PIN_PWM[2][7]={{"P9_14"},{"P9_22"}}; //PWM有効化後の番号
char PIN_PWM[2][7]={{"P9_14"},{"P9_22"}}; //PWM有効化後の番号
int PWM_PIN_NUM[2]={15,16}; //PWMに使用するのBBBピン番号
int MOTOR_GPIO_NUM[2][2]={{61,60},{65,46}}; //モータで使用するGPIO番号=32×A+B(GPIO0_3→3)
void InitPwm(int); //PWM初期化関数 motorNum=0もしくは1
void RunPwm(int,int,int); //モータ用出力関数 motorNum=0もしくは1/driveMode 0:停止，1:正転，-1:逆転
void ClosePwm(int); //PWM終了関数
/**************************/

//GPIO
/**************************/
void GpioExport(int);	//gpioの有効化関数
void GpioUnexport(int); //gpioの有効化解除の関数
int GpioOpen(int, char*, int);	//gpioの設定ファイルを開く関数
/**************************/

//超音波センサ
/**************************/
int SENSOR_GPIO_NUM=20;
void SetSensorGpio(struct pollfd*);	//超音波センサ用のGPIOを有効化
void SoundSensor(int,struct pollfd*);	//超音波センサの動作関数
/**************************/

//ライントレーサ
/**************************/
//右から2つ目を使用せず
int LINE_GPIO_NUM[8]={49,115,3,27,30,47,45};	//ライントレーサで使用するGPIO番号
void IsRidingLine(int[],char[],int);	//ライントレーサの判定
/**************************/

//移動関数
/**************************/
#define RIGHT1 10000000	//右旋回
#define RIGHT2 1000000	//左旋回
#define LEFT1 1000000	//右旋回
#define LEFT2 10000000	//左旋回
void MoveRight();	//右旋回
void MoveLeft();	//左旋回
void MoveStraight();	//直進
void MoveBack();	//後退
void Stop();	//停止
/**************************/

int main(){
	int fd;
	int i,lineNum=7;
	char isRide[7];	//'0':白 '1':黒 7番:旋回フラグ 8番:後退フラグ
	int movingMode=0;	//0:旋回しない 1:右旋回 -1:左旋回 2:攻撃→後退 防衛→スルー
	int startFlag=1;	//発射時のフラグ
	int count=0;	//防衛時行動管理変数
	clock_t start;
	struct pollfd pfd;
	
	//モータを起動
	InitPwm(0);
	InitPwm(1);

	//GPIOを有効化
	for(i=0;i<8;i++){
		GpioExport(LINE_GPIO_NUM[i]);
	}

	/* 攻守切り替え部分
	int mode=SwitchMode(&pfd);	//攻守のモード 0:攻撃 1:防衛
	*/

	int mode=0;

	//超音波センサのGPIOを有効化
	SetSensorGpio(&pfd);

	while(1){
		//ライントレーサによる判定
		IsRidingLine(LINE_GPIO_NUM,isRide,lineNum);

		for(i=0;i<8;i++){
			printf("%c ",isRide[i]);
		}
		printf("\n");

		SoundSensor(fd,&pfd);

		//色付きマスから前進して抜け出す
		if(startFlag==1){
			MoveStraight();
			if(isRide[0]=='1' && isRide[1]=='1' && isRide[2]=='1' && isRide[3]=='0' 
				&& isRide[4]=='0' && isRide[5]=='1' && isRide[6]=='1'){
				startFlag=0;
				printf("change\n");
			}
		}
		//攻撃モード
		else if(mode==0){
			AttackMode(isRide,&movingMode);
		}
		//防衛モード
		else if(mode==1){
			DeffenceMode(isRide,&movingMode,&count,&start);
		}
	}

	//モータを停止
	Stop();
	ClosePwm(0);
	ClosePwm(1);

	return 0;
}

/* モード変更 */
//攻守切替
int SwitchMode(struct pollfd *pfd){
	int fd,org,mode;
	char path[60];

	GpioExport(MODE_GPIO_NUM);

	//出力を設定
	fd=GpioOpen(MODE_GPIO_NUM, "direction", O_WRONLY);
	write(fd, "in", 2);
	close(fd);

	fd=gpio_open(MODE_GPIO_NUM,"value",O_WRONLY);
    write(fd,"both",4);
    close(fd);

    fd=gpio_open(MODE_GPIO_NUM,"value",O_WRONLY);
    *pfd->fd=fd;
    *pfd->events=POLLPRI;

	org=poll(pfd,1,10000);

    while(1){
        lseek(fd,0,SEEK_SET);
        mode=poll(pfd,1,10000);

		if(mode!=org){
			GpioUnexport(MODE_GPIO_NUM);
			break;
		}
        
        read(fd,&mode,1);
    }

	return mode;
}

//攻撃側の行動関数
void AttackMode(char isRide[],int *movingMode){
	//後退
	if(*movingMode==2){
		MoveBack();
		if(isRide[5]=='0' && isRide[6]=='0'){
			*movingMode=0;
		}
		printf("back\n");
	}
	//右旋回
	if(*movingMode==1){
		MoveRight();
		if(isRide[3]=='0' && isRide[4]=='0'){
			*movingMode=0;
		}
		printf("turning R\n");
	}
	//左旋回
	else if(*movingMode==-1){
		MoveLeft();
		if(isRide[3]=='0' && isRide[4]=='0'){
			*movingMode=0;
		}
		printf("turning L\n");
	}
	//反応なし→後退
	if(isRide[0]=='1' && isRide[1]=='1' && isRide[2]=='1' && isRide[3]=='1'
		&& isRide[4]=='1' && isRide[5]=='1' && isRide[6]=='1'){
		MoveBack();
		printf("back\n");
	}
	//右側3つ反応→右旋回
	else if(isRide[4]=='0' && isRide[5]=='0' && isRide[6]=='0'){
		MoveRight();
		*movingMode=1;
		printf("turn R1\n");
	}
	//道から外れていれば右に修正
	else if(isRide[4]=='0' && isRide[5]=='0'){
		MoveRight();
		printf("turn R2\n");
	}
	//道から外れていれば左に修正
	else if(isRide[1]=='1' && isRide[2]=='0' && isRide[3]=='0'){
		MoveLeft();
		printf("turn L2\n");
	}
	//真ん中2つ反応→前進
	else if(isRide[3]=='0' || isRide[4]=='0'){
		MoveStraight();
		printf("Straight\n");
	}
	//左側3つ反応→左旋回
	else if(isRide[0]=='0' && isRide[2]=='0' && isRide[3]=='0'){
		MoveLeft();
		*movingMode=-1;
		printf("turn L1\n");
	}
	else{
		Stop();
		printf("Stop\n");
	}
}

//防衛側の行動関数
void DeffenceMode(char isRide[],int *movingMode,int *count,clock_t *start){
	//前左左前右右フラグ前左ループ
	//次の角まで直進
	if(*movingMode==2){
		MoveStraight();
		if(isRide[2]=='1' && isRide[3]=='1'){
			*movingMode==0;
		}
	}
	//左旋回
	if(*movingMode==-1){
		MoveLeft();
		if(isRide[3]=='0' && isRide[4]=='0'){
			*movingMode=0;
			clock_t end=clock();
			count+=IsProgress(2.0,start);
		}	
	}
	//右旋回
	else if(*movingMode==1){
		MoveRight();
		if(isRide[3]=='0' && isRide[4]=='0'){
			*movingMode=0;
			count+=IsProgress(2.0,start);
		}
	}
	//左側3つ反応→左旋回
	else if(isRide[0]=='0' && isRide[2]=='0' && isRide[3]=='0'){
		//4回目ならばスルー
		if(*count==4){
			*movingMode=2;
		}
		else{
			MoveLeft();
			*movingMode=-1;
			*start=clock();
		}
	}
	//道から外れていれば右に修正
	else if(isRide[4]=='0' && isRide[5]=='0'){
		MoveRight();
	}
	//道から外れていれば左に修正
	else if(isRide[1]=='1' && isRide[2]=='0' && isRide[3]=='0'){
		MoveLeft();
	}
	//真ん中2つ反応→前進
	else if(isRide[3]=='0' && isRide[4]=='0'){
		MoveStraight();
	}
	//右側3つ反応→右旋回
	else if(isRide[4]=='0' && isRide[5]=='0' && isRide[6]=='0'){
		MoveRight();
		*movingMode=1;
		*start=clock();
	}
	else{
		Stop();
	}
}

//時間経過を判定する関数
int IsProgress(double time,clock_t *start){
	clock_t end=clock();
	if((double)(end-*start)/CLOCKS_PER_SEC>=2){
		*start=clock();
		return 1;
	}
	return 0;
}

/* PWM */
//PWM初期化関数
void InitPwm(int motorNum){
	int i,fd;
	char path[60],path3[60],path4[60];
	FILE *fp;

	for(i=0;i<2;i++){
		GpioExport(MOTOR_GPIO_NUM[motorNum][i]);
		
		fd=GpioOpen(MOTOR_GPIO_NUM[motorNum][i], "direction", O_WRONLY);
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
void ClosePwm(int motorNum){
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
		GpioUnexport(MOTOR_GPIO_NUM[motorNum][i]);
	}
}

//モータ用出力関数
void RunPwm(int motorNum,int duty,int driveMode){
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
void GpioExport(int n){
	int fd;
	char buf[40];

	sprintf(buf, "%d", n);

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, buf, strlen(buf));
	close(fd);
}


//gpioの有効化解除の関数
void GpioUnexport(int n){
	int fd;
	char buf[40];

	sprintf(buf, "%d", n);

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	write(fd, buf, strlen(buf));
	close(fd);
}

//gpioの設定ファイルを開く関数
int GpioOpen(int n, char *file, int flag){
	int fd;
	char buf[40];

	sprintf(buf, "/sys/class/gpio/gpio%d/%s", n, file);

	fd = open(buf, flag);
	return fd;
}

/* 超音波センサ */
//超音波センサのGPIOを有効化
void SetSensorGpio(struct pollfd *pfd){
	int fd;
	
	// gpioの有効化
	GpioExport(SENSOR_GPIO_NUM);

	//gpioを入力に設定
	fd = GpioOpen(SENSOR_GPIO_NUM, "direction", O_WRONLY);
	write(fd, "in", 2);
	close(fd);
	
	// gpio を edge に設定
	fd = GpioOpen(SENSOR_GPIO_NUM,"edge",O_WRONLY);
	write(fd,"both",4);
	close(fd);
	
	//gpioのvalueファイルを開く
	fd = GpioOpen(SENSOR_GPIO_NUM, "value", O_RDONLY);

	*pfd->fd = fd;
	*pfd->events = POLLPRI;
}

//超音波センサの動作関数
void SoundSensor(int fd,struct pollfd *pfd){
	int Ion;
	int ret, i;
	char c;
	struct timespec origin;
	struct timespec now;

	lseek(fd, 0, SEEK_SET); //読み取り位置先頭に設定
	ret = poll(pfd, 1, -1); //通知を監視
	read(fd, &c, 1); //通知状態読み込み
	printf("c:%c\n",c);

	if(c == '1'){ //パルスの立ち上がり時刻を取得
		clock_gettime(CLOCK_MONOTONIC_RAW, &origin);
	}
	else return;

	lseek(fd, 0, SEEK_SET);
	ret = poll(pfd, 1, -1);
	read(fd, &c, 1);

	if(c == '0'){ //パルスの立下り時刻を取得
		clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	}
	else return;

	Ion = RC_LAP(now, origin); //パルスのON時間算出
	printf("interval=%d[sec]\n", Ion);
	//usleep(500000);
}

/* ライントレーサ */
//ライントレーサの判定関数
void IsRidingLine(int gpioNum[],char c[],int n){
	int i,fd;
	for(i=0;i<n;i++){
		fd = GpioOpen(gpioNum[i], "value", O_RDONLY);
		lseek(fd, 0, SEEK_SET);
		read(fd, &c[i], 1);
		close(fd);
	}
}

/* 移動 */
//右旋回
void MoveRight(){
	RunPwm(0,LEFT1,1);
	RunPwm(1,RIGHT1,1);
}

//左旋回
void MoveLeft(){
	RunPwm(0,LEFT2,1);
	RunPwm(1,RIGHT2,1);
}

//前進
void MoveStraight(){
	RunPwm(0,LEFT2,1);
	RunPwm(1,RIGHT1,1);
}

//後退
void MoveBack(){
	RunPwm(0,-LEFT2,1);
	RunPwm(1,-RIGHT1,1);
}

//停止
void Stop(){
	RunPwm(0,0,0);
	RunPwm(1,0,0);
}