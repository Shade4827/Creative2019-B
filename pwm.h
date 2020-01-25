#ifndef _PWM_H_
#define _PWM_H_

void initPwm(int); //PWM初期化関数 motorNum=0もしくは1
void runPwm(int,int,int); //モータ用出力関数 motorNum=0もしくは1/driveMode 0:停止，1:正転，-1:逆転
void closePwm(int); //PWM終了関数

#endif