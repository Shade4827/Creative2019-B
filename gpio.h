#ifndef _GPIO_H_
#define _GPIO_H_

void gpioExport(int);	//gpioの有効化関数
void gpioUnexport(int); //gpioの有効化解除の関数
int gpioOpen(int, char*, int);	//gpioの設定ファイルを開く関数

#endif