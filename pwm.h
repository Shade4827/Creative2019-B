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

//モーターを使用する際の設定
/**************************/
char PIN_PWM[2][7]={{"P9_14"},{"P9_22"}}; //PWM有効化後の番号
int PWM_PIN_NUM[2]={16,16}; //PWMに使用するのBBBピン番号
int MOTOR_GPIO_NUM[2][2]={{60,61},{65,46}}; //モータで使用するGPIO番号=32×A+B(GPIO0_3→3)
/**************************/

void initPwm(int motorNum); //PWM初期化関数 motorNum=0もしくは1
void runPwm(int motorNum,int duty,int driveMode); //モータ用出力関数 motorNum=0もしくは1/driveMode 0:停止，1:正転，-1:逆転
void closePwm(int motorNum); //PWM終了関数