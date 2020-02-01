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
clock_t backCount;
char SwitchMode();	//モード切替
void AttackMode(char[],int*,clock_t*);	//攻撃側の行動関数
void DeffenceMode(char[],int*,int*,clock_t*);	//防衛側の行動関数
int IsProgress(double,clock_t*);	//時間経過を判定する関数
/**************************/

//PWM
/**************************/
#define OCP_NUM 3 //ocp.▲の▲に該当する番号
#define PWM_PERIOD 10000000
#define BONE_CAPEMGR_NUM 9 //bone_capemgr.●の●に該当char PIN_PWM[2][7]={{"P9_14"},{"P9_22"}}; //PWM有効化後の番号
char PIN_PWM[2][7]={{"P9_14"},{"P9_22"}}; //PWM有効化後の番号	0:左 1:右
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
int SENSOR_GPIO_NUM=44;
void SetSensorGpio(struct pollfd*);	//超音波センサ用のGPIOを有効化
void SoundSensor(int,struct pollfd*);	//超音波センサの動作関数
//pulseのON時間を時間から算出(単位合わせ)
#define RC_LAP(n,o) ((n.tv_sec - o.tv_sec)*1000000+(n.tv_nsec - o.tv_nsec)/1000)
/**************************/

//ライントレーサ
/**************************/
//右から2つ目を使用せず
int LINE_GPIO_NUM[8]={49,115,3,27,30,47,45};	//ライントレーサで使用するGPIO番号
void IsRidingLine(int[],char[],int);	//ライントレーサの判定
/**************************/

//移動関数
/**************************/
#define FAST 2000000	//速いduty 2000000
#define SLOW 1500000	//遅いduty 1500000
#define MAG 1.1
#define MAG_C 1.25
#define MAG_R 0.4
double magR=1.0;
double magL=1.0;
void MoveRight();	//右旋回
void MoveLeft();	//左旋回
void FixRight();	//右修正
void FixLeft();		//左修正
void MoveStraight();	//直進
void MoveBack();	//後退
void Stop();	//停止
void MoveOver();
/**************************/

int main(){
	int fd;
	int i,lineNum=7;
	char isRide[7];	//'0':白 '1':黒 7番:旋回フラグ 8番:後退フラグ
	//0:旋回しない 1:右旋回 -1:左旋回 
	//2:右修正 -2:左修正 3:攻撃→後退 防衛→スルー
	int movingMode=0;	
	int startFlag=1;	//発射時のフラグ
	int count=0;	//防衛時行動管理変数
	clock_t start;
	//struct pollfd pfd;
	
	//モータを起動
	InitPwm(0);
	InitPwm(1);

	//GPIOを有効化
	for(i=0;i<8;i++){
		GpioExport(LINE_GPIO_NUM[i]);
	}

	// 攻守切り替え部分
	//char mode=SwitchMode();	//攻守のモード 0:攻撃 1:防衛
	//char mode='0';

	//超音波センサのGPIOを有効化
	//SetSensorGpio(&pfd);

	while(1){
		//ライントレーサによる判定
		IsRidingLine(LINE_GPIO_NUM,isRide,lineNum);

		for(i=0;i<8;i++){
			printf("%c ",isRide[i]);
		}
		printf("\n");

		//色付きマスから前進して抜け出す
		if(startFlag==1){
			if(isRide[0]=='1' && isRide[1]=='1' && isRide[2]=='1' && isRide[3]=='0' 
				&& isRide[4]=='0' && isRide[5]=='1' && isRide[6]=='1'){
				startFlag=0;
			}
			//道から外れていれば右に修正
			else if(isRide[3]=='1' && isRide[4]=='0' && isRide[5]=='0'){
				FixRight();
			}
			//道から外れていれば左に修正
			else if(isRide[2]=='0' && isRide[3]=='0' && isRide[4]=='1'){
				FixLeft();
			}
			else{
				MoveStraight();
			}
		}
		//攻撃モード
		else{
			AttackMode(isRide,&movingMode,&start);
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
char SwitchMode(){
	int fd;
	char org,mode;
	char path[60];
	struct pollfd pfd;

	GpioExport(MODE_GPIO_NUM);

	//入力に設定
	fd=GpioOpen(MODE_GPIO_NUM, "direction", O_WRONLY);
	write(fd, "in", 2);
	close(fd);

	fd=GpioOpen(MODE_GPIO_NUM,"edge",O_WRONLY);
    write(fd,"both",4);
    close(fd);

    fd=GpioOpen(MODE_GPIO_NUM,"value",O_RDONLY);
    pfd.fd=fd;
    pfd.events=POLLPRI;

	read(fd,&org,1);
	mode=org;

    while(1){
        lseek(fd,0,SEEK_SET);

		if(mode!=org){
			close(fd);
			GpioUnexport(MODE_GPIO_NUM);
			break;
		}
        
        read(fd,&mode,1);
    }
	printf("\n");
	return mode;
}

//攻撃側の行動関数
void AttackMode(char isRide[],int *movingMode,clock_t *start){
	//袋小路後の前進
	if(*movingMode==3){
		MoveStraight();
		//通常走行にに変更
		if(IsProgress(2.0,start)==1){
			*movingMode=-4;
			//*start=clock();
		}
		printf("fix straight\n");
	}
	//袋小路後の後退
	else if(*movingMode==-4){
		MoveBack();
		if(isRide[5]=='0' && isRide[6]=='0'){
			*movingMode==1;
		}
	}
	//後退
	else if(*movingMode==-3){
		MoveBack();
		if(IsProgress(0.8,start)==1){
			*movingMode=0;
		}
		printf("back\n");
	}
	//右旋回
	else if(*movingMode==1){
		MoveRight();
		if(isRide[4]=='0' && isRide[5]=='1'){
			*movingMode=-3;
			magR=magL=1;
			*start=clock();
		}
		printf("turning R\n");
	}
	//左旋回
	else if(*movingMode==-1){
		MoveLeft();
		if(isRide[2]=='1' && isRide[3]=='0'){
			*movingMode=-3;
			magR=magL=1;
			*start=clock();
		}
		printf("turning L\n");
	}
	//右修正
	else if(*movingMode==2){
		FixRight();
		//条件が合えば右旋回に変更
		if(isRide[5]=='0' && isRide[6]=='0'){
			*movingMode=1;
			if(isRide[2]=='0'){
				magR=1.5;
				magL=0.9;	
			}
			else{
				magR=1.2;
			}
		}
		//直進に変更
		else if((isRide[2]=='1' && isRide[3]=='0') 
			|| (isRide[4]=='0' && isRide[5]=='1')){
			*movingMode=0;
		}
		else if(isRide[0]=='0' && isRide[2]=='0'){
			*movingMode=-1;
			magL=1.2;
		}
		printf("fixing R\n");
	}
	//左修正
	else if(*movingMode==-2){
		FixLeft();
		if(isRide[5]=='0' && isRide[6]=='0'){
			*movingMode=1;
			magR=1.2;
		}
		//直線に変更
		else if((isRide[2]=='1' && isRide[3]=='0') 
			|| (isRide[4]=='0' && isRide[5]=='1')){
			*movingMode=0;
		}
		//条件が合えば左旋回に変更
		else if(isRide[0]=='0' && isRide[2]=='0'){
			*movingMode=-1;
			magL=1.2;
		}
		printf("fixing L\n");
	}
	//反応なし→2秒前進
	else if((isRide[0]=='1' && isRide[1]=='1' && isRide[2]=='1' && isRide[3]=='1'
		&& isRide[4]=='1' && isRide[5]=='1' && isRide[6]=='1')
		|| (isRide[0]=='0' && isRide[6]=='0')){
		MoveBack();
		*movingMode=3;
		*start=clock;
		printf("back\n");
	}
	//右側3つ反応→右旋回
	else if(isRide[5]=='0' && isRide[6]=='0'){
		MoveRight();
		*movingMode=1;
		magR=0;
		printf("turn R\n");
	}
	//真ん中2つ反応→前進
	else if((isRide[2]=='1' && isRide[3]=='0') 
        || (isRide[4]=='0' && isRide[5]=='1')){
		MoveStraight();
		printf("Straight\n");
    }
	//左側3つ反応→左旋回
	else if(isRide[0]=='0' && isRide[2]=='0'){
		MoveLeft();
		*movingMode=-1;
		magL=0;
		printf("turn L\n");
	}
    //道から外れていれば右に修正
	else if(isRide[4]=='0' && isRide[5]=='0'){
		FixRight();
		*movingMode=2;
		printf("fix R\n");
	}
	//道から外れていれば左に修正
	else if(isRide[2]=='0' && isRide[3]=='0'){
		FixLeft();
		*movingMode=-2;
		printf("fix L\n");
	}
	else{
		Stop();
		printf("Stop\n");
	}
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

	pfd->fd = fd;
	pfd->events = POLLPRI;
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
	RunPwm(0,FAST*MAG_C*magL,1);
	RunPwm(1,SLOW*MAG_R*magR*MAG,-1);
}

//左旋回
void MoveLeft(){
	RunPwm(0,SLOW*MAG_R*magL,-1);
	RunPwm(1,FAST*MAG_C*magR*MAG,1);
}

//右修正
void FixRight(){
	RunPwm(0,FAST,1);
	RunPwm(1,SLOW*MAG,1);
}

//左修正
void FixLeft(){
	RunPwm(0,SLOW,1);
	RunPwm(1,FAST*MAG,1);
}

//前進
void MoveStraight(){
	RunPwm(0,FAST,1);
	RunPwm(1,FAST*MAG,1);
}

//後退
void MoveBack(){
	RunPwm(0,FAST,-1);
	RunPwm(1,FAST*MAG,-1);
}

//停止
void Stop(){
	RunPwm(0,0,0);
	RunPwm(1,0,0);
}

void MoveOver(){
	RunPwm(0,PWM_PERIOD,1);
	RunPwm(1,PWM_PERIOD,1);
}