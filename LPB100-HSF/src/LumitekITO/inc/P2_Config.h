/*
******************************
*Company:Lumitek
*Data:2011-01-03
*Author:Meiyusong
******************************
*/

#ifndef __LUMITEK_DEVICE_P2_CONFIG_H__
#define __LUMITEK_DEVICE_P2_CONFIG_H__

//计量二代
#define DEEVICE_LUMITEK_P2

//计量芯片RN8209C
#define RN8209C_SUPPORT

//取消UART 串口功能
#define UART_NOT_SUPPORT

//支持磁宝石继电器
#define SPECIAL_RELAY_SUPPORT

//有按键设备
#define DEVICE_KEY_SUPPORT

//支持继电器指示灯
#define DEVICE_RELAY_LED_SUPPORT

//支持wifi指示灯
#define DEVICE_WIFI_LED_SUPPORT

//RN8209C 选择通道A
#define RN8209C_SELECT_PATH_A

//RN8209 UDP log
//#define LUM_RN8209C_UDP_LOG

//测试读取计量芯片
//#define LUM_READ_ENERGY_TEST

//自动校准
#ifdef LUM_FACTORY_TEST_SUPPORT
#define RN8209_CALIBRATE_SELF
#endif

//自动校准精机
//#define RN8209_PRECISION_MACHINE

#endif

