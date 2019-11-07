#ifndef _PWM_H_
#define _PWM_H_

//モーターを使用する際の設定
/**************************/
char PIN_PWM[2][7]={{"P9_14"},{"P9_22"}}; //PWM有効化後の番号
int PWM_PIN_NUM[2]={14,15}; //PWMに使用するのBBBピン番号
int MOTOR_GPIO_NUM[2][2]={{60,61},{65,46}}; //モータで使用するGPIO番号=32×A+B(GPIO0_3→3)
/**************************/

extern void initPwm(int); //PWM初期化関数 motorNum=0もしくは1
extern void runPwm(int,int,int); //モータ用出力関数 motorNum=0もしくは1/driveMode 0:停止，1:正転，-1:逆転
extern void closePwm(int); //PWM終了関数

#endif