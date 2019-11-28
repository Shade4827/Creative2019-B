#ifndef _GPIO_H_
#define _GPIO_H_

extern void gpioExport(int);	//gpioの有効化関数
extern void gpioUnexport(int); //gpioの有効化解除の関数
extern int gpioOpen(int, char*, int);	//gpioの設定ファイルを開く関数

#endif