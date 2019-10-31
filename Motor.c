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

//ボード毎の設定
/**************************/
#define OCP_NUM 3 //ocp.▲の▲に該当する番号
#define PWM_PERIOD 10000000
#define BONE_CAPEMGR_NUM 9 //bone_capemgr.●の●に該当
/**************************/

//モーターを使用する際の設定
/**************************/
char PIN_PWM[2][7]={{"P9_14"},{"P9_22"}}; //PWM有効化後の番号
int PWM_PIN_NUM[2]={16,16}; //PWMに使用するのBBBピン番号
int MOTOR_GPIO_NUM[2][2]={{60,61},{65,46}}; //モータで使用するGPIO番号=32×A+B(GPIO0_3→3)
/**************************/

//ライントレーサを仕様する際の設定
/**************************/
int LINE_GPIO_NUM[8]={30,48,3,49,115,27,47,45};	//ライントレーサで使用するGPIO番号
/**************************/

void gpioExport(int n);	//gpioの有効化関数
void gpioUnexport(int n); //gpioの有効化解除の関数
int gpioOpen(int n, char *file, int flag);	//gpioの設定ファイルを開く関数
void initPwm(int motorNum); //PWM初期化関数 motorNum=0もしくは1
void runPwm(int motorNum,int duty,int driveMode); //モータ用出力関数 motorNum=0もしくは1/driveMode 0:停止，1:正転，-1:逆転
void closePwm(int motorNum); //PWM終了関数
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
		if(isRide[3]==1 && isRide[4]==1){
			//前進
			runPwm(0,100000,1);
			runPwm(1,100000,1);
			printf("run\n");
		}
		else if(isRide[0]==1 && isRide[1]==1 && isRide[2]==1 && isRide[3]==1){
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

	/*ピンの設定（PIN_PWM指定のピン）*/
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

	/*PWM　ON状態時間の初期化*/
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