#ifndef _GPIO_H_
#define _GPIO_H_

extern void gpioExport(int n);	//gpioの有効化関数
extern void gpioUnexport(int n); //gpioの有効化解除の関数
extern int gpioOpen(int n, char *file, int flag);	//gpioの設定ファイルを開く関数

#endif