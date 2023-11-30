/**
 *******************************************************************************************************************************************
 * @file        xymodem_port_swm190.c
 * @brief       X / Y modem transport protocol port [SWM190] 
 * @since       Change Logs:
 * Date         Author       Notes
 * 2023-11-30   lzh          the first version
 * @copyright (c) 2023 lzh <lzhoran@163.com>
 *                https://github.com/ZeHHHHH/Flexible-XYmodem.git
 * All rights reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************************************************************************
 */
#include "xymodem.h"

/*******************************************************************************************************************************************
 * Reference 
 *******************************************************************************************************************************************/
#include "SWM190.h"

/** CRC-16 hardware enable(if MCU support it) */
//#define CRC16_HW_ENABLE

/* enum Device Peripheral Bus */
#define HW_BUS_UART     0
//#define HW_BUS_SPI      1
//#define SW_BUS_SPI      2

#define DEV_BUS         HW_BUS_UART

/* enum Device Work Mode */
#define MODE_POLL        0
#define MODE_ISR         1
//#define MODE_DMA         2 /* DMA 不太适合 X/Ymodem 这种半双工式的阻塞轮询 */

#define DEV_MODE         MODE_POLL

/* UART Group X Attribute */
#define UART_GROUP_X             UART1
#define UART_GROUP_X_ISR_FUN     UART1_Handler
#define UART_BAUDRATE            115200 

/* UART1 TX - E7 */
#define UART1_TX_PORT       PORTE
#define UART1_TX_PIN        PIN7
#define UART1_TX_FUN        PORTE_PIN7_UART1_TX
/* UART1 RX - E5 */
#define UART1_RX_PORT       PORTE
#define UART1_RX_PIN        PIN5
#define UART1_RX_FUN        PORTE_PIN5_UART1_RX

/* if a timeout occurs, it will return [True]; otherwise, it will return [False]. */
#define IS_TIME_OUT(ticks, timestamp)            ((get_ticks() - (timestamp)) >= (ticks))

/**
 * @brief  get the elapsed tick since the session is initialised
 * @param  \
 * @retval return ticks(up)
 */
static size_t get_ticks(void)
{
#if 1 /* Warning: Users must provide implementation here */
    static volatile size_t tick = 0, temp = 0;
    __NOP();
    if (++temp >= SystemCoreClock / 1000 / 1000)
    {
        temp = 0;
        ++tick;
    }
    return tick;
#endif
}

#ifdef CRC16_HW_ENABLE
/**
 * @brief  CRC16 verify data
 * @remark Provide more efficient CRC16, eg: Hardware-CRC16
 * @param  data     : data
 * @param  cnt      : data size / Bytes
 * @retval verify result
 */
uint16_t xymodem_port_crc16(const uint8_t *data, const uint32_t cnt)
{
    /* CRC Configuration:
     * WIDTH  : 16 bit
     * POLY   : 1021 (x16 + x12 + x5 + 1)
     * INIT   : 0
     * REFIN  : false
     * REFOUT : false
     * XOROUT : 0
     */
}
#endif

/**
 * @brief  peripheral hardware init 
 * @param  \
 * @retval enum xym_sta
 */
xym_sta_t xymodem_port_init(void)
{
    PORT_Init(UART1_TX_PORT, UART1_TX_PIN, UART1_TX_FUN, 0);
    PORT_Init(UART1_RX_PORT, UART1_RX_PIN, UART1_RX_FUN, 1);

    UART_InitStructure UART_initStruct;
    UART_initStruct.Baudrate = UART_BAUDRATE;
    UART_initStruct.DataBits = UART_DATA_8BIT;
    UART_initStruct.Parity = UART_PARITY_NONE;
    UART_initStruct.StopBits = UART_STOP_1BIT;
    UART_initStruct.RXThreshold = 3;
    UART_initStruct.RXThresholdIEn = (DEV_MODE == MODE_ISR) ? 1 : 0;
    UART_initStruct.TXThreshold = 3;
    UART_initStruct.TXThresholdIEn = 0;
    UART_initStruct.TimeoutTime = 10;
    UART_initStruct.TimeoutIEn = (DEV_MODE == MODE_ISR) ? 1 : 0;
    UART_Init(UART_GROUP_X, &UART_initStruct);
    UART_Open(UART_GROUP_X);
    
#ifdef CRC16_HW_ENABLE
    /* CRC16 hardware init */
    return XYM_ERROR_HW;
#endif
    return XYM_OK;
}

/**
 * @brief  send data within the set time
 * @param  data  : data
 * @param  cnt   : data size / Bytes
 * @param  tick  : send 1 Bytes timeout / tick
 * @retval enum xym_sta
 */
xym_sta_t xymodem_port_send_data(const uint8_t *data, const uint32_t cnt, const uint32_t tick)
{
    for (uint32_t i = 0; i < cnt; ++i)
    {
        /* wait for UART_TX-FIFO not full */
        for (size_t timestamp = get_ticks(); UART_IsTXFIFOFull(UART_GROUP_X) != 0; )
        {
            if (IS_TIME_OUT(tick, timestamp))
            {
                return XYM_ERROR_TIMEOUT;
            }
        }
        UART_WriteByte(UART_GROUP_X, *data++);
    }
    /* wait for UART_TX-FIFO to complete sending */
    for (size_t timestamp = get_ticks(); UART_IsTXBusy(UART_GROUP_X) != 0; )
    {
        if (IS_TIME_OUT(tick * cnt, timestamp))
        {
            return XYM_ERROR_TIMEOUT;
        }
    }
    return XYM_OK;
}

/**
 * @brief  receive data within the set time
 * @param  data  : data
 * @param  cnt   : data size / Bytes
 * @param  tick  : receive 1 Bytes timeout / tick
 * @retval enum xym_sta
 */
xym_sta_t xymodem_port_recv_data(uint8_t *data, const uint32_t cnt, const uint32_t tick)
{
    for (uint32_t i = 0; i < cnt; )
    {
        size_t timestamp = get_ticks();
        for (uint32_t c = 0; ; ) /* set timeout and polling UART_RX-FIFO */
        {
            /* UART_RX-FIFO not empty && read / verify data */
            if (0 == UART_IsRXFIFOEmpty(UART_GROUP_X) && 0 == UART_ReadByte(UART_GROUP_X, &c))
            {
                data[i++] = c & 0xFF;
                break;
            }
            if (IS_TIME_OUT(tick, timestamp))
            {
                return XYM_ERROR_TIMEOUT;
            }
        }
    }
    return XYM_OK;
}

#if (DEV_MODE == MODE_ISR)

#define UART_RX_SIZE       (1024)
#define UART_RX_COUNT      (2)
static uint8_t UART_RX_Buffer[UART_RX_COUNT][UART_RX_SIZE];
static volatile uint32_t UART_Count_Index = 0;
static volatile uint32_t UART_Size_Index = 0;
static volatile uint8_t UART_TimeOut_Flag = 0;

void UART_GROUP_X_ISR_FUN(void)
{
	uint32_t chr = 0;
	
	if(UART_INTStat(UART_GROUP_X, UART_IT_RX_THR | UART_IT_RX_TOUT))
	{
		if(UART_INTStat(UART_GROUP_X, UART_IT_RX_TOUT))
		{
			UART_INTClr(UART_GROUP_X, UART_IT_RX_TOUT);
			
            /* IDLE Timeout */
			UART_TimeOut_Flag = 1;
		}
		while(UART_IsRXFIFOEmpty(UART_GROUP_X) == 0)
		{
			if(UART_ReadByte(UART_GROUP_X, &chr) == 0)
			{
				UART_RX_Buffer[UART_Count_Index][UART_Size_Index] = chr;
                if (++UART_Size_Index >= UART_RX_SIZE)
                {
                    UART_Size_Index = 0;
                    if (++UART_Count_Index >= UART_RX_COUNT)
                    {
                        UART_Count_Index = 0;
                    }
                }
			}
		}
	}
}
#endif
