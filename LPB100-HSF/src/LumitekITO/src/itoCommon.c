/*
******************************
*Company:Lumitek
*Data:2014-10-07
*Author:Meiyusong
******************************
*/

#include "../inc/lumitekConfig.h"

#ifdef CONFIG_LUMITEK_DEVICE
#include <hsf.h>
#include <string.h>
#include <stdio.h>

#include "../inc/itoCommon.h"
#include "../inc/deviceGpio.h"
#include "../inc/aes.h"
#include "../inc/socketSendList.h"
#include "../inc/deviceMisc.h"
#include "../inc/asyncMessage.h"
#include "../inc/lumTimeData.h"
#include "../inc/lumTimer.h"
#ifdef ENTER_UPGRADE_BY_AMARM
#include "../inc/deviceUpgrade.h"
#endif
#include "../inc/lumLog.h"

#ifdef RN8209C_SUPPORT
#include "../inc/rn8209c.h"
#endif
#if defined(LUM_UDP_SOCKET_LOG) || defined(LUM_RN8209C_UDP_LOG)
#include "../inc/localSocketUdp.h"
#endif
#ifdef LUM_FACTORY_TEST_SUPPORT
#include "../inc/lumFactoryTest.h"
#endif
#ifdef SX1208_433M_SUPPORT
#include "../inc/lum_sx1208.h"
#endif


static S8 g_udp_recv_buf[NETWORK_MAXRECV_LEN];
static S8 g_tcp_recv_buf[NETWORK_MAXRECV_LEN];

static GLOBAL_CONFIG_DATA g_deviceConfig;


S8* USER_FUNC getUdpRecvBuf(BOOL setZero)
{
	if(setZero)
	{
		memset(g_udp_recv_buf, 0, NETWORK_MAXRECV_LEN);
	}
	return g_udp_recv_buf;
}


S8* USER_FUNC getTcpRecvBuf(BOOL setZero)
{
	if(setZero)
	{
		memset(g_tcp_recv_buf, 0, NETWORK_MAXRECV_LEN);
	}
	return g_tcp_recv_buf;
}



U16 USER_FUNC getSocketSn(BOOL needIncrease)
{
	if(needIncrease)
	{
		g_deviceConfig.globalData.socketSn++;
		if(g_deviceConfig.globalData.socketSn >= INVALID_SN_NUM)
		{
			g_deviceConfig.globalData.socketSn = 0;
		}
	}
	return g_deviceConfig.globalData.socketSn;
}



void USER_FUNC setDeviceConnectInfo(DEVICE_CONN_TYPE connType, BOOL value)
{
	switch (connType)
	{
	case DHPC_OK_BIT:
		g_deviceConfig.globalData.connInfo.dhcpOK = value;
		break;

	case BALANCE_CONN_BIT:
		g_deviceConfig.globalData.connInfo.balanceOK = value;
		break;

	case SERVER_ADDR_BIT:
		g_deviceConfig.globalData.connInfo.serverAdd = value;
		break;

	case SERVER_CONN_BIT:
		g_deviceConfig.globalData.connInfo.serverConn = value;
		break;

	default:
		break;
	}
}



BOOL USER_FUNC getDeviceConnectInfo(DEVICE_CONN_TYPE connType)
{
	U8 ret;


	switch (connType)
	{
	case DHPC_OK_BIT:
		ret = g_deviceConfig.globalData.connInfo.dhcpOK;
		break;

	case BALANCE_CONN_BIT:
		ret = g_deviceConfig.globalData.connInfo.balanceOK;
		break;

	case SERVER_ADDR_BIT:
		ret = g_deviceConfig.globalData.connInfo.serverAdd;
		break;

	case SERVER_CONN_BIT:
		ret = g_deviceConfig.globalData.connInfo.serverConn;
		break;

	default:
		ret = 0;
		break;
	}

	return (ret == 0)?FALSE:TRUE;
}


static BOOL USER_FUNC getIpAddrFromModual(U32* ipAddr)
{
	char *words[5]={NULL};
	char rsp[128]={0};
	unsigned char addr[42]={0};
	BOOL ret = FALSE;


	hfat_send_cmd("AT+WANN\r\n",sizeof("AT+WANN\r\n"),rsp,128);

	if(hfat_get_words(rsp,words, 5)>0)
	{
		if((rsp[0]=='+')&&(rsp[1]=='o')&&(rsp[2]=='k'))
		{
			strcpy((char*)addr,(char*)words[2]);
			*ipAddr = inet_addr((char*)addr);
			lumi_debug("Device ip=%08X \n",*ipAddr);
			ret = TRUE;
		}
	}
	return ret;
}


static void USER_FUNC setDeviceIpAddress(BOOL bClear, U32 ipAddr)
{
	if(bClear)
	{
		g_deviceConfig.globalData.ipAddr = 0;
	}
	else
	{
		getIpAddrFromModual(&g_deviceConfig.globalData.ipAddr);
		if(g_deviceConfig.globalData.ipAddr == 0 || g_deviceConfig.globalData.ipAddr == 0xFFFFFFFF)
		{
			g_deviceConfig.globalData.ipAddr = ipAddr;
		}
	}
}


U32 USER_FUNC getDeviceIpAddress(void)
{
	if(g_deviceConfig.globalData.ipAddr == 0 || g_deviceConfig.globalData.ipAddr == 0xFFFFFFFF)
	{
		getIpAddrFromModual(&g_deviceConfig.globalData.ipAddr);
	}
	return g_deviceConfig.globalData.ipAddr;
}


U32 USER_FUNC getBroadcastAddr(void)
{
	U32 broadcaseAddr;
	
	broadcaseAddr = g_deviceConfig.globalData.ipAddr;
	broadcaseAddr |= 0xFF000000;
	return broadcaseAddr;
}


void USER_FUNC setFlagAfterDhcp(U32 ipAddr)
{
	if(!getDeviceConnectInfo(DHPC_OK_BIT))
	{
		setDeviceConnectInfo(DHPC_OK_BIT, TRUE);
		setDeviceIpAddress(FALSE, ipAddr);
		if(!lum_bEnterFactoryTest())
		{
#ifdef DEVICE_NO_KEY
			cancelCheckSmartLinkTimer();
#endif
#ifdef DEVICE_WIFI_LED_SUPPORT
			setWifiLedStatus(WIFILED_CLOSE);
#endif
#ifdef CHANGE_BRIGHTNESS_SUPPORT
			lum_setLedLightStatus(LIGHT_LED_CLOSE);
#endif

		}
#ifdef LUM_FACTORY_TEST_SUPPORT
		else
		{
			lum_factoryTestDhcpSucc();
		}
#endif
	}
}


void USER_FUNC setFlagAfterApDisconnect(void)
{
	setDeviceConnectInfo(DHPC_OK_BIT, FALSE);
	setDeviceIpAddress(TRUE, 0);
	if(!lum_bEnterFactoryTest())
	{
		setDeviceConnectInfo(SERVER_CONN_BIT, FALSE);
#ifdef DEVICE_NO_KEY
		checkNeedEnterSmartLink();
#endif
#ifdef DEVICE_WIFI_LED_SUPPORT
		setWifiLedStatus(WIFI_LED_AP_DISCONNECT);
#endif
#ifdef CHANGE_BRIGHTNESS_SUPPORT
		lum_setLedLightStatus(LIGHT_NOT_UNCONNECT);
#endif

	}
	lum_stopFactoryResetTimer();
}


void USER_FUNC setServerAddr(SOCKET_ADDR* pSocketAddr)
{
	memcpy(&g_deviceConfig.globalData.tcpServerAddr, pSocketAddr, sizeof(SOCKET_ADDR));
}


void USER_FUNC getServerAddr(SOCKET_ADDR* pSocketAddr)
{
	memcpy(pSocketAddr, &g_deviceConfig.globalData.tcpServerAddr, sizeof(SOCKET_ADDR));
}


U8* USER_FUNC mallocSocketData(size_t size)
{
	U8* ptData = NULL;

	ptData = hfmem_malloc(size + 1);
	if(ptData == NULL)
	{
		HF_Debug(DEBUG_ERROR, "meiyusong===> Malloc mallocSocketData \n");
		return NULL;
	}
	else
	{
		g_deviceConfig.globalData.mallocCount++;
		if(g_deviceConfig.globalData.mallocCount >= 0xFFFF)
		{
			g_deviceConfig.globalData.mallocCount = 100;
		}
	}
	memset(ptData, 0, size+1);
	//lumi_debug("malloc mallocCount = %d size=%d\n", g_deviceConfig.globalData.mallocCount, size);
	return ptData;
}


void USER_FUNC FreeSocketData(U8* ptData)
{
	if(g_deviceConfig.globalData.mallocCount == 0)
	{
		HF_Debug(DEBUG_ERROR, "g_deviceConfig.globalData.mallocCount < 0 \n");
	}
	hfmem_free(ptData);
	g_deviceConfig.globalData.mallocCount--;

	//lumi_debug(" free mallocCount = %d \n", g_deviceConfig.globalData.mallocCount);
}



static void USER_FUNC saveDeviceConfigData(void)
{
	hffile_userbin_write(DEVICE_CONFIG_OFFSET_START, (char*)(&g_deviceConfig.deviceConfigData), DEVICE_CONFIG_SIZE);
}


void USER_FUNC setDeviceName(DEVICE_NAME_DATA* nameData)
{
	memcpy(&g_deviceConfig.deviceConfigData.deviceName, nameData, sizeof(DEVICE_NAME_DATA));
	saveDeviceConfigData();
}


static void USER_FUNC initDeviceNameData(void)
{
	U8 defaultNameLen = strlen(DEFAULT_MODUAL_NAME);


	//Device name init
	memcpy(g_deviceConfig.deviceConfigData.deviceName.nameData, DEFAULT_MODUAL_NAME, defaultNameLen);
	g_deviceConfig.deviceConfigData.deviceName.nameLen = defaultNameLen;

}


DEVICE_NAME_DATA* USER_FUNC getDeviceName(void)
{
	return &g_deviceConfig.deviceConfigData.deviceName;
}


static U8 USER_FUNC lum_getEmptyAlarmPlace(U8 offset)
{
	U8 i;

	for(i=0; i<MAX_ALARM_COUNT; i++)
	{
		if(g_deviceConfig.deviceConfigData.alarmData[i + offset].startHour == 0xFF && g_deviceConfig.deviceConfigData.alarmData[i + offset].stopHour == INVALID_ALARM_FLAG)
		{
			break;
		}
	}
	if(i < MAX_ALARM_COUNT)
	{
		i += offset;
	}
	else
	{
		i = ADD_ALARM_INDEX_EMPTY;
	}
	return i;
}


U8 USER_FUNC setAlarmData(ALARM_DATA_INFO* alarmData, U8 index, U8 offset)
{

	U8 tmpIndex;


	if(index == ADD_ALARM_INDEX_EMPTY)
	{
		tmpIndex = lum_getEmptyAlarmPlace(offset);
	}
	else
	{
		tmpIndex = index;
	}
	if(tmpIndex == ADD_ALARM_INDEX_EMPTY || (tmpIndex - offset) >= MAX_ALARM_COUNT)
	{
		return ADD_ALARM_INDEX_EMPTY;
	}
	
	memcpy(&g_deviceConfig.deviceConfigData.alarmData[tmpIndex], alarmData, sizeof(ALARM_DATA_INFO));

#ifdef LUM_ADD_ALARM_DATA_FAST
	{
		U8 i;
	
		if(index == ADD_ALARM_INDEX_EMPTY && alarmData->startHour == 0 && alarmData->startMinute == 0 && alarmData->stopHour == 1 && alarmData->stopMinute == 0)
		{
			for(i=offset; i<(offset+MAX_ALARM_COUNT); i++)
			{
				if(g_deviceConfig.deviceConfigData.alarmData[i].startHour == 0xFF || g_deviceConfig.deviceConfigData.alarmData[i].stopHour == INVALID_ALARM_FLAG)
				{
					memcpy(&g_deviceConfig.deviceConfigData.alarmData[i], alarmData, sizeof(ALARM_DATA_INFO));
				}
			}
		}
	}
#endif


	saveDeviceConfigData();

#if 0
	lumi_debug("AlarmData index=%d m=%d T=%d W=%d T=%d F=%d S=%d Sun=%d active=%d startHour=%d, startMinute=%d stopHour=%d stopMinute=%d size=%d\n",
			 index,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].repeatData.monday,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].repeatData.tuesday,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].repeatData.wednesday,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].repeatData.thursday,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].repeatData.firday,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].repeatData.saturday,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].repeatData.sunday,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].repeatData.bActive,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].startHour,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].startMinute,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].stopHour,
	         g_deviceConfig.deviceConfigData.alarmData[tmpIndex].stopMinute,
	         sizeof(ALARM_DATA_INFO));
#endif
	return tmpIndex - offset;
}


void USER_FUNC deleteAlarmData(U8 index, BOOL needSave)
{
	if(index >= TOTAL_ALARM_COUNT)
	{
		return;
	}
	g_deviceConfig.deviceConfigData.alarmData[index].repeatData.bActive = (U8)EVENT_INCATIVE;
	g_deviceConfig.deviceConfigData.alarmData[index].startHour= INVALID_ALARM_FLAG;
	g_deviceConfig.deviceConfigData.alarmData[index].startMinute= INVALID_ALARM_FLAG;
	g_deviceConfig.deviceConfigData.alarmData[index].stopHour= INVALID_ALARM_FLAG;
	g_deviceConfig.deviceConfigData.alarmData[index].stopMinute= INVALID_ALARM_FLAG;
	g_deviceConfig.deviceConfigData.alarmData[index].reserved = 0;

	if(needSave)
	{
		saveDeviceConfigData();
	}
}



ALARM_DATA_INFO* USER_FUNC getAlarmData(U8 index)
{
	if(index >= TOTAL_ALARM_COUNT)
	{
		return NULL;
	}
	else
	{
		return &g_deviceConfig.deviceConfigData.alarmData[index];
	}
}



static void USER_FUNC initAlarmData(void)
{
	U8 i;


	for(i=0; i<TOTAL_ALARM_COUNT; i++)
	{
		deleteAlarmData(i, FALSE);
	}
}



void USER_FUNC deleteAbsenceData(U8 index, BOOL needSave)
{
	if(index >= TOTAL_ABSENCE_COUNT)
	{
		return;
	}
	memset(&g_deviceConfig.deviceConfigData.absenceData[index], 0, sizeof(ASBENCE_DATA_INFO));
	g_deviceConfig.deviceConfigData.absenceData[index].startHour = INVALID_ALARM_FLAG;

	if(needSave)
	{
		lum_checkAbsenceWhileChange(index);
		saveDeviceConfigData();
	}
}


static void USER_FUNC initAbsenceData(void)
{
	U8 i;


	memset(&g_deviceConfig.deviceConfigData.absenceData, 0, sizeof(ASBENCE_DATA_INFO)*TOTAL_ABSENCE_COUNT);
	for(i=0; i<TOTAL_ABSENCE_COUNT; i++)
	{
		deleteAbsenceData(i, FALSE);
	}
}

static U8 USER_FUNC lum_getAbsenceEmptyPlace(U8 offset)
{
	U8 i;


	for(i=0; i<MAX_ABSENCE_COUNT; i++)
	{
		if(g_deviceConfig.deviceConfigData.absenceData[i+offset].startHour == INVALID_ALARM_FLAG)
		{
			break;
		}
	}
	if(i < MAX_ABSENCE_COUNT)
	{
		i += offset;
	}
	else
	{
		i = ADD_ALARM_INDEX_EMPTY;
	}
	return i;
}


U8 USER_FUNC setAbsenceData(ASBENCE_DATA_INFO* absenceData, U8 index, U8 offset)
{
	U8 tmpIndex;


	if(index == ADD_ALARM_INDEX_EMPTY)
	{
		tmpIndex = lum_getAbsenceEmptyPlace(offset);
	}
	else
	{
		tmpIndex = index;
	}
	if(tmpIndex == ADD_ALARM_INDEX_EMPTY || (tmpIndex - offset) >= MAX_ABSENCE_COUNT)
	{
		return ADD_ALARM_INDEX_EMPTY;
	}
	
#if 1
#ifdef ENTER_UPGRADE_BY_AMARM
	if(absenceData->startHour == 2 && absenceData->startMinute == 10
		&& absenceData->endHour == 3 && absenceData->endMinute == 11) //G8 10:10
	{
#ifdef DEVICE_UPGRADE_BY_CONFIG
		S8* URL = "http://122.227.207.66/yyy/";
#elif defined(DEVICE_UPGRADE_BY_DOWNLOAD_BIN)
		S8* URL = "http://122.227.207.66/yyy/LPBS2W_UPGARDE.bin";
#else
		#error
#endif
		U8 urlLen = strlen(URL);
		
		setSoftwareUpgradeUrl(URL, urlLen);
		return tmpIndex;
	}
#endif

#ifdef SAVE_LOG_TO_FLASH
		if(absenceData->startHour == 3 && absenceData->startMinute == 11
			&& absenceData->endHour== 4 && absenceData->endMinute== 12) //G8 11:11
		{
			readFlashLog();
			return tmpIndex;
		}
#endif

#ifdef RN8209C_SUPPORT
		if(absenceData->startHour == 4 && absenceData->startMinute == 12
			&& absenceData->endHour == 5 && absenceData->endMinute == 13) //G8 12:12
		{
			rn8209cClearCalibraterData();
			return tmpIndex;
		}
#endif

#ifdef LUM_FACTORY_TEST_SUPPORT
		if(absenceData->startHour == 5 && absenceData->startMinute == 13
			&& absenceData->endHour == 6 && absenceData->endMinute == 14) //G8 13:13
		{
			lum_setFactoryTestFlag(TRUE); //Clear
			return tmpIndex;
		}
		else if(absenceData->startHour == 6 && absenceData->startMinute == 14
			&& absenceData->endHour == 7 && absenceData->endMinute == 15) //G8 14:14
		{
			lum_setFactoryTestFlag(FALSE); //Set
			return tmpIndex;
		}
#endif
		if(absenceData->startHour == 7 && absenceData->startMinute == 15
			&& absenceData->endHour == 8 && absenceData->endMinute == 16) //G8 15:15
		{
			lum_setUdpLogFlag(FALSE); //close
			saveDeviceConfigData();
			return tmpIndex;
		}
		else if(absenceData->startHour == 8 && absenceData->startMinute == 16
			&& absenceData->endHour == 9 && absenceData->endMinute == 17) //G8 16:16
		{
			lum_setUdpLogFlag(TRUE); //enable
			saveDeviceConfigData();
			return tmpIndex;
		}
#else

#endif

	memcpy(&g_deviceConfig.deviceConfigData.absenceData[tmpIndex], absenceData, sizeof(ASBENCE_DATA_INFO));

#ifdef LUM_ADD_ABSENCE_DATA_FAST
	{
		U8 i;
	
		if(index == ADD_ALARM_INDEX_EMPTY && absenceData->startHour == 9 && absenceData->startMinute == 17
			&& absenceData->endHour == 10 && absenceData->endMinute == 18)
		{
			for(i=offset; i<(offset+ADD_ALARM_INDEX_EMPTY); i++)
			{
				if(g_deviceConfig.deviceConfigData.absenceData[i].startHour == INVALID_ALARM_FLAG)
				{
					memcpy(&g_deviceConfig.deviceConfigData.absenceData[i], absenceData, sizeof(ASBENCE_DATA_INFO));
				}
			}
		}
	}
#endif


	saveDeviceConfigData();
	lum_checkAbsenceWhileChange(tmpIndex);
#if 0	
	lumi_debug("AbsenceData  index=%d m=%d T=%d W=%d T=%d F=%d S=%d Sun=%d active=%d Shour=%d, Sminute=%d Ehour=%d, Eminute=%d time=%d size=%d\n",
			 index,
	         g_deviceConfig.deviceConfigData.absenceData[index].repeatData.monday,
	         g_deviceConfig.deviceConfigData.absenceData[index].repeatData.tuesday,
	         g_deviceConfig.deviceConfigData.absenceData[index].repeatData.wednesday,
	         g_deviceConfig.deviceConfigData.absenceData[index].repeatData.thursday,
	         g_deviceConfig.deviceConfigData.absenceData[index].repeatData.firday,
	         g_deviceConfig.deviceConfigData.absenceData[index].repeatData.saturday,
	         g_deviceConfig.deviceConfigData.absenceData[index].repeatData.sunday,
	         g_deviceConfig.deviceConfigData.absenceData[index].repeatData.bActive,
	         g_deviceConfig.deviceConfigData.absenceData[index].startHour,
	         g_deviceConfig.deviceConfigData.absenceData[index].startMinute,
	         g_deviceConfig.deviceConfigData.absenceData[index].endHour,
	         g_deviceConfig.deviceConfigData.absenceData[index].endMinute,
	         g_deviceConfig.deviceConfigData.absenceData[index].timeData,
	         sizeof(ASBENCE_DATA_INFO));
#endif
return tmpIndex - offset;
}


ASBENCE_DATA_INFO* USER_FUNC getAbsenceData(U8 index)
{
	if(index >= TOTAL_ABSENCE_COUNT)
	{
		return NULL;
	}
	else
	{
		return &g_deviceConfig.deviceConfigData.absenceData[index];
	}
}



static void USER_FUNC initCountDownData(void)
{
	memset(&g_deviceConfig.deviceConfigData.countDownData, 0, sizeof(COUNTDOWN_DATA_INFO)*TOTAL_COUNTDOWN_COUNT);
}



void USER_FUNC setCountDownData(COUNTDOWN_DATA_INFO* countDownData, U8 index)
{
	if(index >= TOTAL_COUNTDOWN_COUNT)
	{
		return;
	}

	memcpy(&g_deviceConfig.deviceConfigData.countDownData[index], countDownData, sizeof(COUNTDOWN_DATA_INFO));
	saveDeviceConfigData();

	//checkCountDownTimerAfterChange(index);
	lumi_debug("countDownData active=%d, action=%d, count=%0xX\n", countDownData->flag.bActive,
	         countDownData->action, countDownData->count);
}



void USER_FUNC deleteCountDownData(U8 index)
{
	if(index >= TOTAL_COUNTDOWN_COUNT)
	{
		return;
	}
	
	memset(&g_deviceConfig.deviceConfigData.countDownData[index], 0, sizeof(COUNTDOWN_DATA_INFO));
	//checkCountDownTimerAfterChange(index);
	saveDeviceConfigData();
}



COUNTDOWN_DATA_INFO* USER_FUNC getCountDownData(U8 index)
{
	if(index >= TOTAL_COUNTDOWN_COUNT)
	{
		return NULL;
	}
	else
	{
		return &g_deviceConfig.deviceConfigData.countDownData[index];
	}
}


#ifdef LUM_FACTORY_TEST_SUPPORT
//首次上电自动配置到默认AP
static void USER_FUNC lum_setDefaultApData(void)
{
	char rsp[64]={0};
	S8 sendData[50];
	

	memset(rsp, 0, sizeof(rsp));
	memset(sendData, 0, sizeof(sendData));

	sprintf(sendData, "AT+WSSSID=%s\r\n", DEFAULT_AP_SSID);	
	hfat_send_cmd(sendData, strlen(sendData), rsp, 32);
	if(((rsp[0]=='+')&&(rsp[1]=='o')&&(rsp[2]=='k')))
	{
		memset(rsp, 0, sizeof(rsp));
		memset(sendData, 0, sizeof(sendData));

		sprintf(sendData, "AT+WSKEY=WPA2PSK,AES,%s\r\n", DEFAULT_AP_PASSWORD);
		hfat_send_cmd(sendData, strlen(sendData), rsp, 32);
		if(((rsp[0]=='+')&&(rsp[1]=='o')&&(rsp[2]=='k')))
		{
			hfsys_reset();
		}
	}
}
#endif


#ifdef COLOR_TEMPERATURE_SUPPORT
void USER_FUNC lum_setTemperatureLevel(U8 tempLevel)
{
	g_deviceConfig.deviceConfigData.tempLevel = tempLevel;
	saveDeviceConfigData();
}


U8 USER_FUNC lum_getTemperatureLevel(void)
{
	return g_deviceConfig.deviceConfigData.tempLevel;
}
#endif


#ifdef CHANGE_BRIGHTNESS_SUPPORT
void USER_FUNC lum_setBrightnessLevel(U8 brightLevel)
{
	g_deviceConfig.deviceConfigData.brightsLevel= brightLevel;
	saveDeviceConfigData();
}


U8 USER_FUNC lum_getBrightnessLevel(void)
{
	return g_deviceConfig.deviceConfigData.brightsLevel;
}
#endif



void USER_FUNC globalConfigDataInit(BOOL factoryReset)
{
	memset(&g_deviceConfig, 0, sizeof(GLOBAL_CONFIG_DATA));
	if(!factoryReset)
	{
		hffile_userbin_read(DEVICE_CONFIG_OFFSET_START, (char*)(&g_deviceConfig.deviceConfigData), DEVICE_CONFIG_SIZE);
	}
	if(g_deviceConfig.deviceConfigData.lumitekFlag != LUMITEK_SW_FLAG)
	{
		//Device  first power on flag
		hffile_userbin_zero();
		memset(&g_deviceConfig, 0, sizeof(GLOBAL_CONFIG_DATA));
		g_deviceConfig.deviceConfigData.lumitekFlag = LUMITEK_SW_FLAG;

#ifdef COLOR_TEMPERATURE_SUPPORT
		g_deviceConfig.deviceConfigData.tempLevel = DEFAULT_TEMP_LEVEL;
#endif
#ifdef CHANGE_BRIGHTNESS_SUPPORT
		g_deviceConfig.deviceConfigData.brightsLevel = DEFAULT_BRIGHT_LEVEL;
#endif
		initDeviceNameData();
		initAlarmData();
		initAbsenceData();
		initCountDownData();
		if(!factoryReset)
		{
			saveDeviceConfigData();
#ifdef LUM_FACTORY_TEST_SUPPORT
			msleep(100);
			lum_setDefaultApData();
#endif
		}
	}
}


void USER_FUNC lum_deviceFactoryReset(void)
{
#ifdef RN8209C_SUPPORT
	RN8209C_CALI_DATA rn8209cData;
#endif


#ifdef RN8209C_SUPPORT
	memcpy(&rn8209cData, &g_deviceConfig.deviceConfigData.rn8209cData, sizeof(RN8209C_CALI_DATA));
#endif
	globalConfigDataInit(TRUE);
#ifdef RN8209C_SUPPORT
	memcpy(&g_deviceConfig.deviceConfigData.rn8209cData, &rn8209cData, sizeof(RN8209C_CALI_DATA));
#endif
	g_deviceConfig.deviceConfigData.needSmartLink = 1;

	saveDeviceConfigData();

	msleep(150);
	sendSmartLinkCmd();
}



U8* USER_FUNC lum_getUserName(void)
{
	return g_deviceConfig.deviceConfigData.userName;
}


void USER_FUNC lum_setUserName(U8* userName)
{
	U8 len;


	len = strlen((S8*)userName);
	if(len >= MAX_USER_NAME_LEN)
	{
		len = MAX_USER_NAME_LEN-2;
	}
	memset(g_deviceConfig.deviceConfigData.userName, 0, MAX_USER_NAME_LEN);
	if(len != 0)
	{
		memcpy(g_deviceConfig.deviceConfigData.userName, userName, len);
	}
	saveDeviceConfigData();
}


#ifdef RN8209C_SUPPORT
void USER_FUNC rn8209cClearCalibraterData(void)
{
	memset(&g_deviceConfig.deviceConfigData.rn8209cData, 0, sizeof(RN8209C_CALI_DATA));
	saveDeviceConfigData();
#ifdef LUM_RN8209C_UDP_LOG
	saveNormalLogData("Reset for Clear rn8209C calibrater flag");
#endif
	msleep(100);
	hfsys_reset();
}


RN8209C_CALI_DATA* USER_FUNC lum_rn8209cGetCaliData(void)
{
	return &g_deviceConfig.deviceConfigData.rn8209cData;
}


void USER_FUNC lum_rn8209cSaveCaliData(void)
{
	saveDeviceConfigData();
}

#endif


GLOBAL_CONFIG_DATA* USER_FUNC getGlobalConfigData(void)
{
	return &g_deviceConfig;
}


void USER_FUNC changeDeviceSwVersion(U8 swVersion)
{
	if(g_deviceConfig.deviceConfigData.swVersion!= swVersion)
	{
		g_deviceConfig.deviceConfigData.swVersion= swVersion;
		saveDeviceConfigData();
	}
}


U8 USER_FUNC getDeviceSwVersion(void)
{
	return g_deviceConfig.deviceConfigData.swVersion;
}



void USER_FUNC setSoftwareUpgradeUrl(S8* url, U8 urlLen)
{
	if(urlLen >= MAX_UPGRADE_URL_LEN)
	{
		lumi_error("Upgrade Url too long\n");
		return;
	}
	memset(g_deviceConfig.deviceConfigData.upgradeData.urlData, 0, MAX_UPGRADE_URL_LEN);
	strcpy(g_deviceConfig.deviceConfigData.upgradeData.urlData, url);
	g_deviceConfig.deviceConfigData.upgradeData.upgradeFlag = SOFTWARE_UPGRADE_FLAG;
	saveDeviceConfigData();
	insertLocalMsgToList(MSG_LOCAL_EVENT, NULL, 0, MSG_CMD_MODULE_UPGRADE);
}


void USER_FUNC clearSoftwareUpgradeFlag(void)
{
	g_deviceConfig.deviceConfigData.upgradeData.upgradeFlag = 0;
	saveDeviceConfigData();
}


SW_UPGRADE_DATA* USER_FUNC getSoftwareUpgradeData(void)
{
	return &g_deviceConfig.deviceConfigData.upgradeData;
}



void USER_FUNC changeDeviceLockedStatus(BOOL bLocked)
{
	U8 status;

	status = bLocked?1:0;
	if(g_deviceConfig.deviceConfigData.bLocked != status)
	{
		g_deviceConfig.deviceConfigData.bLocked = status;
		saveDeviceConfigData();
		lumi_debug("LOCK Device bLocked=%d\n", bLocked);
	}
}


BOOL USER_FUNC getDeviceLockedStatus(void)
{
	return (g_deviceConfig.deviceConfigData.bLocked == 0)?FALSE:TRUE;;
}



static U8 USER_FUNC macAtoi(S8 c)
{
	U8 ret = 0;


	if(c >= 'A' && c <= 'F')
	{
		ret = c - 'A' + 10;
	}
	else if(c >= 'a' && c <= 'f')
	{
		ret = c - 'a' + 10;
	}
	else if(c >= '0' && c <= '9')
	{
		ret = c - '0';
	}
	return ret;
}


static BOOL USER_FUNC readDeviceMacAddr(void)
{
	S8 *words[3]= {NULL};
	S8 rsp[64]= {0};
	BOOL ret = FALSE;
	U8 macAddr[DEVICE_MAC_LEN+1];
	U8 i;


	memset(macAddr, 0, sizeof(macAddr));
	hfat_send_cmd("AT+WSMAC\r\n",sizeof("AT+WSMAC\r\n"),rsp,64);

	if(hfat_get_words(rsp,words, 2)>0)
	{
		if((rsp[0]=='+')&&(rsp[1]=='o')&&(rsp[2]=='k'))
		{
			lumi_debug("sting mac = %s\n", words[1]);
			for(i=0; i<DEVICE_MAC_LEN; i++)
			{
				macAddr[i] = macAtoi(words[1][(i<<1)]);
				macAddr[i] = (macAddr[i]<<4) + macAtoi(words[1][(i<<1)+1]);
				//lumi_debug("int mac[%d] = %X\n", i, macAddr[i]);
			}
			ret = TRUE;
		}
	}
	memcpy(g_deviceConfig.globalData.macAddr, macAddr, DEVICE_MAC_LEN);
	return ret;
}


U8* USER_FUNC getDeviceMacAddr(U8* devMac)
{
	if(devMac != NULL)
	{
		memcpy(devMac, g_deviceConfig.globalData.macAddr, DEVICE_MAC_LEN);
	}
	return g_deviceConfig.globalData.macAddr;
}


static void USER_FUNC CreateLocalAesKey(void)
{
#if 0
	U8 i;
	U8 index = 0;

	for(i=0; i<DEVICE_MAC_LEN; i++)
	{
		if(i < (DEVICE_MAC_LEN-1))
		{
			aesKey[index++] = ((deviceMac[i]&0x2f)>>2);
			aesKey[index++] = ((deviceMac[i]&0x78)>>3);
			aesKey[index++] = ((deviceMac[i]&0x1E)>>1);
		}
		else
		{
			aesKey[index++] = ((deviceMac[i]&0x2f)>>2);
		}
	}
#else
	g_deviceConfig.globalData.keyData.localAesKeyValid = TRUE;
	memcpy(g_deviceConfig.globalData.keyData.localKey, DEFAULT_AES_KEY, AES_KEY_LEN);
#endif
}


BOOL USER_FUNC lum_checkSocketBeforeAES(U32 recvCount, U8* recvBuf)
{
	BOOL ret = TRUE;
	SOCKET_HEADER_OPEN* pOpenData;


	if(recvCount < 17) //header+cmdData
	{
		ret = FALSE;
	}
	else
	{
		pOpenData = (SOCKET_HEADER_OPEN*)recvBuf;


		if(pOpenData->pv != SOCKET_HEADER_PV)
		{
			ret = FALSE;
		}
		else if((pOpenData->dataLen + sizeof(SOCKET_HEADER_OPEN)) != recvCount) //check Socket lenth
		{
			ret = FALSE;
		}
	}
	return ret;
}


BOOL USER_FUNC lum_checkSocketAfterAES(U8* socketData)
{
	SCOKET_HERADER_OUTSIDE* pHeaderOutside;
	BOOL ret = TRUE;
	BOOL factoryTest = FALSE;


	pHeaderOutside = (SCOKET_HERADER_OUTSIDE*)socketData;

#ifndef RN8209_PRECISION_MACHINE
	if(memcmp(pHeaderOutside->openData.mac, g_deviceConfig.globalData.macAddr, DEVICE_MAC_LEN) != 0)
	{
		U8 macFF[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

		if(memcmp(pHeaderOutside->openData.mac, macFF, DEVICE_MAC_LEN) == 0 && socketData[sizeof(SCOKET_HERADER_OUTSIDE)] == MSG_CMD_FOUND_DEVICE)
		{
			if(getDeviceConnectInfo(SERVER_CONN_BIT) && getDeviceLockedStatus())
			{
				ret = FALSE;
			}
		}
		else if(memcmp(pHeaderOutside->openData.mac, FACTORY_TOOL_MAC, DEVICE_MAC_LEN) == 0 && socketData[sizeof(SCOKET_HERADER_OUTSIDE)] == MSG_CMD_QUARY_MODULE_INFO)
		{
			factoryTest = TRUE;
		}
		else
		{
			ret = FALSE;
		}
	}
#endif	
	if(ret && pHeaderOutside->deviceType != SOCKET_HEADER_DEVICE_TYPE)
	{
		if(!factoryTest)
		{
			ret = FALSE;
		}
	}
	if(ret && pHeaderOutside->licenseData != SOCKET_HEADER_LICENSE_DATA)
	{
		ret = FALSE;
	}
	return ret;
}


#ifdef LUMITEK_DEBUG_SWITCH

//Notice macString lenth must >= 18  		(2*6+5+1)
S8* USER_FUNC macAddrToString(U8* macAddr, S8*macString)
{
	static S8 tempMacAddr[20];
	S8* pMacAddr;

	memset(tempMacAddr, 0, sizeof(tempMacAddr));
	pMacAddr = (macString == NULL)?tempMacAddr:macString;
	sprintf(pMacAddr, "%02X-%02X-%02X-%02X-%02X-%02X", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
	return pMacAddr;
}



void USER_FUNC showHexData(S8* descript, U8* showData, S32 lenth)
{
	U8 i;
	S8 temData[250];
	U8 index = 0;

	if(lenth <= 0)
	{
		lumi_debug("showHexData len=%d\n", lenth);
		return;
	}
	memset(temData, 0, sizeof(temData));
	for(i=0; i<lenth; i++)
	{
		if(i == 0)
		{
			//do nothing
		}
		else if(i%8 == 0)
		{
			temData[index++] = ' ';
			temData[index++] = ' ';
		}
		else if(i%2 == 0)
		{
			temData[index++] = ',';
			temData[index++] = ' ';
		}
		sprintf(temData+index, "%02X", showData[i]);
		index += 2;
		if(index > 150)
		{
			break;
		}
	}
	if(descript != NULL)
	{
		lumi_debug("%s len=%d data=%s\n", descript, lenth, temData);
	}
	else
	{
		lumi_debug("len=%d data=%s\n", lenth, temData);
	}
}




void USER_FUNC printGlobalParaStatus(S8* discript)
{
	lumi_debug("discript = %s lumitekFlag=0x%X bLocked=%d macAddr=%s\n", discript,
	         g_deviceConfig.deviceConfigData.lumitekFlag,
	         g_deviceConfig.deviceConfigData.bLocked,
	         macAddrToString(g_deviceConfig.globalData.macAddr, NULL));
}


void USER_FUNC debugShowSendData(MSG_ORIGIN msgOrigin, U8* pSocketData, U32 recvDataLen)
{
	U8* decrpytData;
	U32 dataLen = recvDataLen;


	decrpytData = encryptRecvSocketData(msgOrigin, pSocketData, &dataLen);
	if(decrpytData != NULL)
	{
		showHexData("After Des", decrpytData, dataLen);
		FreeSocketData(decrpytData);
	}
}


void USER_FUNC showSocketOutsideData(U8* pData)
{
	SCOKET_HERADER_OUTSIDE* pHearderData = (SCOKET_HERADER_OUTSIDE*)pData;


	lumi_debug("pv=%d, flag=0x%x, mac=%x-%x-%x-%x-%x-%x, len=%d  reserved=%d snIndex=0x%x, deviceType=0x%x, factoryCode=0x%x, licenseData=0x%x\n",
	         pHearderData->openData.pv,
	         pHearderData->openData.flag,
	         pHearderData->openData.mac[0],
	         pHearderData->openData.mac[1],
	         pHearderData->openData.mac[2],
	         pHearderData->openData.mac[3],
	         pHearderData->openData.mac[4],
	         pHearderData->openData.mac[5],
	         pHearderData->openData.dataLen,
	         pHearderData->reserved,
	         pHearderData->snIndex,
	         pHearderData->deviceType,
	         pHearderData->factoryCode,
	         pHearderData->licenseData);

}


U16 USER_FUNC getMallocCount(void)
{
	return g_deviceConfig.globalData.mallocCount;
}

#else
S8* USER_FUNC macAddrToString(U8* macAddr, S8*macString)
{
	return NULL;
}

void USER_FUNC showHexData(S8* descript, U8* showData, S32 lenth)
{
	return;
}


void USER_FUNC printGlobalParaStatus(S8* discript)
{
	return;
}


void USER_FUNC debugShowSendData(MSG_ORIGIN msgOrigin, U8* pSocketData, U32 recvDataLen)
{
	return;
}


void USER_FUNC showSocketOutsideData(U8* pData)
{
	return;
}


U16 USER_FUNC getMallocCount(void)
{
	return 0;
}

#endif



#if 0
//192.168.1.100 --->C4A80164
static void USER_FUNC coverIpToInt(S8* stringIP, U8* IntIP)
{
	U8 i = 0;
	U8 gapIndex = 0;

	memset(IntIP, 0, SOCKET_IP_LEN);
	while(1)
	{
		if(stringIP[i] == '.')
		{
			gapIndex++;
		}
		else if(stringIP[i] <= '9' && stringIP[i] >= '0')
		{
			IntIP[gapIndex] *= 10;
			IntIP[gapIndex] += stringIP[i] - '0';
		}
		else
		{
			break;
		}
		i++;
	}

	//lumi_debug(" coverIpToInt %d.%d.%d.%d \n", IntIP[0], IntIP[1], IntIP[2], IntIP[3]);
}



// AT+WANN
// +ok=DHCP,192.168.1.59,255.255.255.0,192.168.1.1
BOOL USER_FUNC getDeviceIPAddr(U8* ipAddr)
{
	char rsp[68]= {0};
	char *words[5]= {NULL};
	char wann_addr[20]= {0};
	BOOL ret = FALSE;



	hfat_send_cmd("AT+WANN\r\n",sizeof("AT+WANN\r\n"),rsp,68);
	if((rsp[0]!='+')&&(rsp[1]!='o')&&(rsp[2]!='k'))
	{
		memset(rsp, 0, 68);
		hfat_send_cmd("AT+WANN\r\n",sizeof("AT+WANN\r\n"),rsp,68);
	}
	if((rsp[0]=='+')&&(rsp[1]=='o')&&(rsp[2]=='k'))
	{
		if(hfat_get_words(rsp,words, 5)>0)
		{
			strcpy((char*)wann_addr,(char*)words[2]);
			//lumi_debug("IP=%s\n", wann_addr);
			if(strcmp(wann_addr,"0.0.0.0") != 0)
			{
				coverIpToInt(wann_addr, ipAddr);
				ret = TRUE;
			}
		}
	}
	return ret;
}



BOOL USER_FUNC getDeviceIPAddr(U8* ipAddr)
{
	char *words[5]={NULL};
	char rsp[128]={0};
	//unsigned long ip_addr = 0;
	U32* pIpAddr;
	unsigned char addr[42]={0};
	BOOL ret = FALSE;


	pIpAddr = (U32*)ipAddr;
	hfat_send_cmd("AT+WANN\r\n",sizeof("AT+WANN\r\n"),rsp,128);
	
	if(hfat_get_words(rsp,words, 5)>0)
	{
		if((rsp[0]=='+')&&(rsp[1]=='o')&&(rsp[2]=='k'))
		{
			strcpy((char*)addr,(char*)words[2]);		
			//ip_addr = inet_addr((char*)addr);
			//*pIpAddr = htonl(ip_addr);
			*pIpAddr = inet_addr((char*)addr);
			lumi_debug("ip=%lX \n",*pIpAddr);
			ret = TRUE;
		}
	}
	return ret;
}
#endif


void USER_FUNC lum_setUdpLogFlag(U8 bEnable)
{
	g_deviceConfig.deviceConfigData.udpLogFlag = bEnable;
}


BOOL USER_FUNC lum_getUdpLogFlag(void)
{
	return (g_deviceConfig.deviceConfigData.udpLogFlag == 0)?FALSE:TRUE;
}


static void USER_FUNC setDebuglevel(void)
{
	S32 debugLevel;

#if defined(__HF_DEBUG) && !defined(UART_NOT_SUPPORT)
	debugLevel = DEBUG_LEVEL_USER;
#else
	debugLevel = DEBUG_LEVEL_CLOSE;
#endif

	if(hfdbg_get_level() != debugLevel)
	{
		hfdbg_set_level(debugLevel);
	}
}


void USER_FUNC itoParaInit(BOOL bFactoryTest)
{
	//initDevicePin(FALSE);
	closeNtpMode();
	setDebuglevel();
	//globalConfigDataInit(FALSE);
	readDeviceMacAddr();
	CreateLocalAesKey();
	sendListInit();
	
	if(!bFactoryTest)
	{
#ifdef DEVICE_NO_KEY
		checkNeedEnterSmartLink();
#endif
#ifdef SAVE_LOG_TO_FLASH
		initFlashLog();
#endif
	}
	lum_initTimer(NOT_NTP_CHECK_TIMER_PERIOD);
	lum_initSystemTime();
#if defined(LUM_UDP_SOCKET_LOG) || defined(LUM_RN8209C_UDP_LOG)
	lum_createUdpLogSocket();
#endif
#ifdef RN8209C_SUPPORT
	lum_rn8209cInit();
#endif
#ifdef SX1208_433M_SUPPORT
	lum_sx1208ChipInit();
#endif
}



static void USER_FUNC PKCS5PaddingFillData(U8* inputData, U32* dataLen, AES_KEY_TYPE keyType)
{
	U8	fillData;

	if(keyType == AES_KEY_OPEN)
	{
		return;
	}
	fillData = AES_BLOCK_SIZE - (*dataLen%AES_BLOCK_SIZE);
	memset((inputData + *dataLen), fillData, fillData);
	*dataLen += fillData;
}



static void USER_FUNC PKCS5PaddingRemoveData(U8* inputData, U32* dataLen, AES_KEY_TYPE keyType)
{
	U8	removeData = inputData[*dataLen - 1];

	if(keyType == AES_KEY_OPEN)
	{
		return;
	}
	if(removeData != 0)
	{
		memset((inputData + *dataLen - removeData), 0, removeData);
	}
	else
	{
		HF_Debug(DEBUG_ERROR, "meiyusong===> PKCS5PaddingRemoveData Error \n");
	}
	*dataLen -= removeData;
}



AES_KEY_TYPE USER_FUNC getRecvSocketAesKeyType(MSG_ORIGIN msgOrigin, U8* pData)
{
	SOCKET_HEADER_OPEN* pOpenData = (SOCKET_HEADER_OPEN*)pData;


	return getSocketAesKeyType(msgOrigin, pOpenData->flag.bEncrypt);
}



AES_KEY_TYPE USER_FUNC getSocketAesKeyType(MSG_ORIGIN msgOrigin, U8 bEncrypt)
{
	AES_KEY_TYPE keyType = AES_KEY_OPEN ;

	if(bEncrypt == 0)
	{
		keyType = AES_KEY_OPEN;
	}
	else if(msgOrigin == MSG_FROM_UDP)
	{
		if(g_deviceConfig.globalData.keyData.localAesKeyValid)
		{
			keyType = AES_KEY_LOCAL;
		}
		else
		{
			keyType = AES_KEY_DEFAULT;
		}
	}
	else if(msgOrigin == MSG_FROM_TCP)
	{
		if(g_deviceConfig.globalData.keyData.serverAesKeyValid)
		{
			keyType = AES_KEY_SERVER;
		}
		else
		{
			keyType = AES_KEY_DEFAULT;
		}
	}
	return keyType;

}



BOOL USER_FUNC getAesKeyData(AES_KEY_TYPE keyType, U8* keyData)
{
	U8* tmpKeyData = NULL;
	BOOL needCopy = TRUE;


	if(keyType == AES_KEY_DEFAULT)
	{
		tmpKeyData = DEFAULT_AES_KEY;
	}
	else if(keyType == AES_KEY_LOCAL)
	{
		tmpKeyData = g_deviceConfig.globalData.keyData.localKey;
	}
	else if(keyType == AES_KEY_SERVER)
	{
		tmpKeyData = g_deviceConfig.globalData.keyData.serverKey;
	}
	else
	{
		needCopy = FALSE;
	}
	if(needCopy)
	{
		memcpy(keyData, tmpKeyData, AES_KEY_LEN);
	}
	return needCopy;
}



void USER_FUNC clearServerAesKey(BOOL clearAddr)
{
	g_deviceConfig.globalData.keyData.serverAesKeyValid = FALSE;
	memset(g_deviceConfig.globalData.keyData.serverKey, 0, AES_KEY_LEN);
	if(clearAddr)
	{
		g_deviceConfig.globalData.tcpServerAddr.ipAddr = INVALID_SERVER_ADDR;
		g_deviceConfig.globalData.tcpServerAddr.port = INVALID_SERVER_PORT;
	}
}


void USER_FUNC setServerAesKey(U8* serverKey)
{
	memcpy(g_deviceConfig.globalData.keyData.serverKey, serverKey, AES_KEY_LEN);
	g_deviceConfig.globalData.keyData.serverAesKeyValid = TRUE;
}


BOOL USER_FUNC lum_getServerKeyStatus(void)
{
	return g_deviceConfig.globalData.keyData.serverAesKeyValid;
}

static BOOL USER_FUNC setAesKey(Aes* dec, AES_KEY_TYPE keyType, S32 aesType)
{
	U8 aesKey[AES_KEY_LEN + 1];
	BOOL ret = TRUE;


	memset(aesKey, 0, sizeof(aesKey));
	if(!getAesKeyData(keyType, aesKey))
	{
		ret = FALSE;
	}
	else
	{
		//lumi_debug("aeskey=%s keyType=%d aesType=%d\n", aesKey, keyType, aesType);
		AesSetKey(dec, (const byte *)aesKey, AES_BLOCK_SIZE, (const byte *)aesKey, aesType);
	}
	return ret;
}


#ifdef AES_DECYPT_TEST
void USER_FUNC lum_aesTest(U8* aesData, U8 len, U8* key)
{
	U8* aesKey = "1234567890abcdef";
	U8 aesData[] = {0x01, 0x44, 0xAC, 0xCF, 0x23, 0x3F, 0x73, 0x6E, 0x10, 0xCD, 0x41, 0x5A, 0x8C, 0xEC, 0xE2, 0x62, 0x1C, 0x35, 0xE8, 0xA3, 0x0D, 0xA1, 0x56, 0x6F, 0xF8};
	//U8* aesKey = "nlrf2hhi0t9u5dxs";
	//U8 aesData[] =  {0x01, 0x44, 0xAC, 0xCF, 0x23, 0x3F, 0x73, 0x6E, 0x10, 0x5E, 0x8D, 0x4F, 0x4D, 0xBE, 0x70, 0x7F, 0xC2, 0xDA, 0xF6, 0x8C, 0xA9, 0xAA, 0xCE, 0x47, 0x81};

	aesTest(aesData+9, 16, aesKey);

}


void USER_FUNC lum_aesDecryptTest(U8* aesData, U8 len, U8* key)
{
	Aes dec;
	U8 data[128];
	U8 oriData[100];
	U8 keyData[100];
	

	memset(data,0, sizeof(data));
	memset(oriData,0, sizeof(oriData));
	memset(keyData,0, sizeof(keyData));

	memcpy(oriData, aesData, len);
	memcpy(keyData, key, 16);
	AesSetKey(&dec, (const byte *)keyData, AES_BLOCK_SIZE, (const byte *)keyData, AES_DECRYPTION);
	AesCbcDecrypt(&dec, data, oriData, len); 
}
#endif

BOOL USER_FUNC socketDataAesDecrypt(U8 *inData, U8* outData, U32* aesDataLen, AES_KEY_TYPE keyType)
{
	Aes dec;
	U32 dataLen = *aesDataLen;


	if(inData == NULL || outData == NULL)
	{
		HF_Debug(DEBUG_ERROR, "socketDataAesDecrypt input data Error \n");
		return FALSE;
	}

	if(keyType == AES_KEY_OPEN)
	{
		memcpy(outData, inData, dataLen);
	}
	else
	{
		if(!setAesKey(&dec, keyType, AES_DECRYPTION))
		{
			HF_Debug(DEBUG_ERROR, "Decrypt keyType Error keyType=%d \n", keyType);
			return FALSE;
		}
		AesCbcDecrypt(&dec, (byte *)(outData), (const byte *)inData, dataLen);
		PKCS5PaddingRemoveData(outData, aesDataLen, keyType);
	}
	return TRUE;
}



BOOL USER_FUNC socketDataAesEncrypt(U8 *inData, U8* outData, U32* aesDataLen, AES_KEY_TYPE keyType)
{
	Aes enc;


	if(inData == NULL || outData == NULL)
	{
		HF_Debug(DEBUG_ERROR, "meiyusong===> socketDataAesEncrypt input data Error \n");
		return FALSE;
	}
	if(keyType == AES_KEY_OPEN)
	{
		memcpy(outData, inData, *aesDataLen);
	}
	else
	{
		if(!setAesKey(&enc, keyType, AES_ENCRYPTION))
		{
			HF_Debug(DEBUG_ERROR, "meiyusong===> Encrypt keyType Error keyType=%d\n", keyType);
			return FALSE;
		}
		PKCS5PaddingFillData(inData, aesDataLen, keyType);
		AesCbcEncrypt(&enc, (byte *)outData , (const byte *)inData, *aesDataLen);
	}
	return TRUE;
}



static void USER_FUNC setSocketAesDataLen(U8* pData, U32 aesDataLen)
{
	SOCKET_HEADER_OPEN* pOpendata = (SOCKET_HEADER_OPEN*)pData;

	pOpendata->dataLen = aesDataLen;
}



static U8* USER_FUNC getEncryptDataBuf(U32 socketBodylen)
{
	U8* pData;
	U32 mallocLen;


	mallocLen = socketBodylen + SOCKET_HEADER_LEN + AES_BLOCK_SIZE + 1;
	pData = mallocSocketData(mallocLen);
	if(pData != NULL)
	{
		memset(pData, 0, mallocLen);
	}
	return pData;
}



U8* USER_FUNC createSendSocketData(CREATE_SOCKET_DATA* createData, U32* sendSocketLen)
{
	U8* originSocketBuf;
	SCOKET_HERADER_OUTSIDE* pSocketHeader;
	U8* pAesData;
	U32 aesDataLen;
	U8 openDataLen = SOCKET_HEADER_OPEN_DATA_LEN;


	originSocketBuf = getEncryptDataBuf(createData->bodyLen);
	if(originSocketBuf == NULL)
	{
		HF_Debug(DEBUG_ERROR, "meiyusong===> malloc Encrypt buf faild \n");
		return NULL;
	}
	pSocketHeader = (SCOKET_HERADER_OUTSIDE*)originSocketBuf;

	pSocketHeader->openData.pv = SOCKET_HEADER_PV;
	pSocketHeader->openData.flag.bEncrypt = createData->bEncrypt;
	pSocketHeader->openData.flag.bReback = createData->bReback;
	pSocketHeader->openData.flag.bLocked = g_deviceConfig.deviceConfigData.bLocked;
#ifdef RN8209_PRECISION_MACHINE
	if(createData->bodyData[0] == MSG_CMD_GET_CALIBRATE_DATA)
	{
		memcpy(pSocketHeader->openData.mac, FACTORY_TOOL_MAC, DEVICE_MAC_LEN);
	}
	else
#endif
	{
		getDeviceMacAddr(pSocketHeader->openData.mac);
	}
	pSocketHeader->openData.dataLen = (createData->bodyLen + SOCKET_HEADER_SECRET_DATA_LEN); //aesDataLen

	pSocketHeader->reserved = SOCKET_HEADER_RESERVED;
	pSocketHeader->snIndex = htons(createData->snIndex);
	pSocketHeader->deviceType = SOCKET_HEADER_DEVICE_TYPE;
	pSocketHeader->factoryCode = SOCKET_HEADER_FACTORY_CODE;
	pSocketHeader->licenseData = SOCKET_HEADER_LICENSE_DATA;

	//fill body data
	memcpy((originSocketBuf + SOCKET_HEADER_LEN), createData->bodyData, createData->bodyLen);

	pAesData = mallocSocketData(SOCKET_HEADER_LEN + createData->bodyLen + AES_BLOCK_SIZE + 1);
	if(pAesData == NULL)
	{
		HF_Debug(DEBUG_ERROR, "meiyusong===> createSendSocketData mallic faild \n");
		FreeSocketData(originSocketBuf);
		return NULL;
	}
	memcpy(pAesData, originSocketBuf, openDataLen);
	aesDataLen = pSocketHeader->openData.dataLen;
	//showHexData("before aes", originSocketBuf, (createData->bodyLen + SOCKET_HEADER_LEN));
#if defined(SAVE_LOG_TO_FLASH) || defined(LUM_UART_SOCKET_LOG) || defined(LUM_UDP_SOCKET_LOG)
	saveSocketData(FALSE, createData->msgOrigin, originSocketBuf, (createData->bodyLen + SOCKET_HEADER_LEN));
#endif
	if(socketDataAesEncrypt((originSocketBuf + openDataLen), (pAesData + openDataLen), &aesDataLen, createData->keyType))
	{
		*sendSocketLen = aesDataLen + openDataLen;
		setSocketAesDataLen(pAesData, aesDataLen);
		//showHexData("After aes", pAesData, *sendSocketLen);
		//debugShowSendData(MSG_FROM_UDP, pAesData, *sendSocketLen);
	}
	else
	{
		FreeSocketData(pAesData);
		pAesData = NULL;
		*sendSocketLen = 0;
	}
	FreeSocketData(originSocketBuf);
	return pAesData;
}




U8* USER_FUNC encryptRecvSocketData(MSG_ORIGIN msgOrigin, U8* pSocketData, U32* recvDataLen)
{
	U8* pData;
	U8 openDataLen = SOCKET_HEADER_OPEN_DATA_LEN;
	U32 dataLen = *recvDataLen;
	U32 asDataLen = *recvDataLen - openDataLen;
	AES_KEY_TYPE keyType;


	if(pSocketData == NULL || dataLen < 10)
	{
		return NULL;
	}
	pData = mallocSocketData(dataLen);
	if(pData == NULL)
	{
		return NULL;
	}

	keyType = getRecvSocketAesKeyType(msgOrigin, pSocketData);
	memcpy(pData, pSocketData, openDataLen);

	if(socketDataAesDecrypt((pSocketData + openDataLen), (pData + openDataLen), &asDataLen, keyType))
	{
		SCOKET_HERADER_OUTSIDE* pTmpData = (SCOKET_HERADER_OUTSIDE*)pData;

		*recvDataLen = asDataLen + openDataLen;
		pTmpData->snIndex = ntohs(pTmpData->snIndex);
		pTmpData->openData.dataLen = asDataLen;
		return pData;
	}
	else
	{
		FreeSocketData(pData);
		return NULL;
	}
}


DEVICE_RESET_TYPE USER_FUNC checkResetType(void)
{
	S32	resetReason;
	SW_UPGRADE_DATA* pUpgradeData;
	DEVICE_RESET_TYPE resetType = RESET_FOR_NORMAL;


	globalConfigDataInit(FALSE);
	resetReason = hfsys_get_reset_reason();
	pUpgradeData = getSoftwareUpgradeData();
	
	if(resetReason&HFSYS_RESET_REASON_SMARTLINK_START)
	{
		resetType = RESET_FOR_SMARTLINK;
	}
	else if(pUpgradeData->upgradeFlag == SOFTWARE_UPGRADE_FLAG)
	{
		resetType = RESET_FOR_UPGRADE;
	}
	else
	{
		if(resetReason&HFSYS_RESET_REASON_SMARTLINK_OK)
		{
			resetType = RESET_FOR_SMARTLINK_OK;
		}
#if defined(LUM_FACTORY_TEST_SUPPORT) && !defined(RN8209_PRECISION_MACHINE)
		if(lum_checkNeedFactoryTest())
		{
			resetType |= RESET_FOR_FACTORY_TEST;
		}
#endif
	}

	lumi_debug("resetType=%d\n", resetType);
	return resetType;
}



void USER_FUNC lum_setFactorySmartlink(BOOL needSmartlink)
{
	g_deviceConfig.deviceConfigData.needSmartLink = needSmartlink;
	saveDeviceConfigData();
}

BOOL USER_FUNC lum_getFactorySmartlink(void)
{
	return g_deviceConfig.deviceConfigData.needSmartLink;
}

#ifndef LUM_FACTORY_TEST_SUPPORT
BOOL USER_FUNC lum_bEnterFactoryTest(void)
{
	return FALSE;
}
#endif /* LUM_FACTORY_TEST_SUPPORT */

#endif


