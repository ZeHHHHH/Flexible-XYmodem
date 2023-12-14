/**
 *******************************************************************************************************************************************
 * @file        xymodem_example.c
 * @brief       X / Y modem transport protocol example 
 * @since       Change Logs:
 * Date         Author       Notes
 * 2023-11-30   lzh          the first version
 * 2023-12-10   lzh          add macro __XYM_LOG__() 
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
#include <stdio.h>
#include <string.h>
#include "xymodem.h"

/*******************************************************************************************************************************************
 * Private Define
 *******************************************************************************************************************************************/
#define RECEIVER  (1 << 0)
#define SENDER    (1 << 1)
#define X_MODEM   (1 << 2)
#define Y_MODEM   (1 << 3)
/** Example selection */
#define EXAMPLE_CONFIG   (X_MODEM | RECEIVER)

#define TEST_SIZE         (1 << 20) /* simple test data size: 1M Bytes */

#if 1 /* Automatic allocation on the Stack */
# define ATTRIBUTE_FAST_MEM       /* recommend [Stack Min_Size] >= 0x800 */
#else /* Static allocation on RAM */
# define ATTRIBUTE_FAST_MEM       static
#endif

#if 1 /* log printf */
# define __XYM_LOG__(...)      printf(__VA_ARGS__)
#else 
# define __XYM_LOG__(...)      
#endif

/*******************************************************************************************************************************************
 * Private Typedef
 *******************************************************************************************************************************************/
/** Y modem session transmission file info struct */
typedef struct
{
    uint8_t name[XYM_PKT_SIZE_128];
    uint32_t size;
} ymodem_file_t; /* The structure supports customization, 
                  * but you need to write supporting encoding and decoding functions.
                  */
/*******************************************************************************************************************************************
 * Private Function
 *******************************************************************************************************************************************/
/* 把参数 str 所指向的字符串转换为一个整数(类型为无符号整型, 为当前平台能达到的最大长度)*/
static size_t common_atoi(const uint8_t *str)
{
    size_t ret = 0, i = 0;
    for (i = 0; str[i] >= '0' && '9' >= str[i]; ++i)
    {
        ret = ret * 10 + (str[i] - '0');
        if ((size_t)ret != ret) /* numeric overflow */
        {
            return 2147483647;
        }
    }
    return ret;
}

/* 把一个无符号整数 val 参数转换至 str 所指向长度为 str_len 的字符串内, 以'\0'结尾 */
static void common_itoa(uint8_t *str, const size_t str_len, const size_t val)
{ 
    snprintf((char *)str, str_len, "%d", val);
}

/**
 * @brief   Y modem session transmission decode file info 
 * @param   f    : file info struct
 * @param   buff : data
 * @param   size : data size / Bytes
 * @retval  \
 */
static void decode_file(ymodem_file_t *f, uint8_t *buff, const uint16_t size)
{
    uint16_t i = 0;
    for (i = 0; buff[i] != 0 && i < size && i < sizeof(f->name) / sizeof(f->name[0]); ++i)
    {
        f->name[i] = buff[i];
    }
    if (i >= sizeof(f->name) / sizeof(f->name[0]))
        return ;
    f->name[i++] = '\0';

    f->size = common_atoi(&buff[i]);
}

/**
 * @brief   Y modem session transmission encode file info 
 * @param   f    : file info struct
 * @param   buff : data
 * @param   size : data size / Bytes
 * @retval  \
 */
static void encode_file(ymodem_file_t *f, uint8_t *buff, const uint16_t size)
{
    uint16_t i = 0;
    for (i = 0; f->name[i] != 0 && i < size && i < sizeof(f->name) / sizeof(f->name[0]); ++i)
    {
        buff[i] = f->name[i];
    }
    if (i >= size)
        return ;
    buff[i++] = '\0';
    
    common_itoa(&buff[i], sizeof(f->name) / sizeof(f->name[0]) - i, f->size);
}

/*******************************************************************************************************************************************
 * Public Function
 *******************************************************************************************************************************************/
/**
 * @brief   xymodem example
 * @param   \
 * @retval  0     : success
 * @retval  other : error code
 */
uint8_t xymodem_example(void)
{
    xym_sta_t res_sta = XYM_OK; 
    uint16_t len = 0; /* the data length of packet / Bytes */
    uint32_t cnt = 0; /* session transmission count / Bytes */

    /* The session control struct remains valid throughout the entire lifecycle of the session.
     * Note: If two or more sessions exist, 
     * their session handles must be independent of each other and cannot be shared.
     */
    ATTRIBUTE_FAST_MEM xym_session_t session;

    /* Data Cache: The size depends on the sender's settings, 
     * and it is recommended to set it to 1K(1024 Bytes) to prevent overflow when the sender's configuration is unknown.
     */
    ATTRIBUTE_FAST_MEM uint8_t buff[XYM_PKT_SIZE_1024];
    
    /* Xmodem expected size / Bytes */
    const uint32_t xmodem_size = TEST_SIZE;
    
    /* Ymodem Var */
	ymodem_file_t file_list[3] = {
        {"ymodem_test_file_0.bin", TEST_SIZE},
        {"ymodem_test_file_1.bin", TEST_SIZE},
        {"ymodem_test_file_2.bin", TEST_SIZE},
    }; /* support file_list */
    const uint32_t file_list_max_num = sizeof(file_list) / sizeof(file_list[0]);
    uint32_t file_num = 0;
    ymodem_file_t *file_p = file_list;
    
extern xym_sta_t xymodem_port_init(void);
extern xym_sta_t xymodem_port_send_data(const uint8_t *data, const uint32_t cnt, const uint32_t tick);
extern xym_sta_t xymodem_port_recv_data(uint8_t *data, const uint32_t cnt, const uint32_t tick);
extern xym_sta_t xymodem_port_crc16(const uint8_t *data, const uint32_t cnt);
    
    if (XYM_OK != xymodem_port_init())
    {
        __XYM_LOG__("X / Y modem hardware init error, please check [port_init]!\r\n");
        return 1;
    }
    __XYM_LOG__("X / Y modem example test!\r\n");

    /* session init */
    xymodem_session_init(&session,
                        xymodem_port_send_data, xymodem_port_recv_data, NULL/* xymodem_port_crc16 */,
                        1000, 1000, 10);

Xmodem_Receiver:
#if (EXAMPLE_CONFIG & (X_MODEM | RECEIVER))
    __XYM_LOG__("X modem receive start, size = [%d]\r\n", xmodem_size);
    for (xmodem_init(&session); res_sta == XYM_OK; cnt += len)
    {
        res_sta = xmodem_receive(&session, buff, &len);
        /* Exceeding user's expected size */
        if (cnt >= xmodem_size)
        {
            res_sta = xymodem_active_cancel(&session);
            break;
        }
        /* The last package exceeded the preset size,
         * the user can choose whether to intercept the excess part,
         * which is usually filled 0x1A.
         */
        len = (cnt > xmodem_size - len) ? (xmodem_size - len) : len;
        // process_data(buff, len);
    }
#endif

Xmodem_Sender:
#if (EXAMPLE_CONFIG & (X_MODEM | SENDER))
    __XYM_LOG__("X modem send start, size = [%d]\r\n", xmodem_size);
    for (xmodem_init(&session); res_sta == XYM_OK; cnt += len)
    {
        len = (xmodem_size - cnt > sizeof(buff) / sizeof(buff[0])) ? (sizeof(buff) / sizeof(buff[0])) : (xmodem_size - cnt);
        if (len > 0)
        {
            memset(buff, 0xAA, len);
            // set_data(buff, len);
        }
        res_sta = xmodem_transmit(&session, buff, len);
    }
#endif
    
Ymodem_Receiver:
#if (EXAMPLE_CONFIG & (Y_MODEM | RECEIVER))
    __XYM_LOG__("Y modem receive start!\r\n");
    file_num = 0;
    for (ymodem_init(&session); res_sta == XYM_OK; cnt += len)
    {
        res_sta = ymodem_receive(&session, buff, &len);
       /* When starting a new file transfer... */
        if (res_sta == XYM_FIL_GET)
        {
            /* get a new file struct(custom) */
            decode_file(&file_p[file_num], buff, len);
			
            //++file_num;

            /* If you are using a file system, 
             * you need to use a file name to open the corresponding file operation handle
             * (eg: f_handle = f_open(file.name) )
             */

            cnt = 0; /* new file restart */
            len = 0;
            res_sta = XYM_OK;
            continue;
        }
        /* Exceeding user's expected size */
        if (cnt >= file_p[file_num].size)
        {
            res_sta = xymodem_active_cancel(&session);
            break;
        }
        /* The last package exceeded the preset size,
         * the user can choose whether to intercept the excess part,
         * which is usually filled 0x1A.
         */
        len = (cnt > file_p[file_num].size - len) ? (file_p[file_num].size - len) : len;
        // process_data(buff, len);
        memset(buff, 0, len);
    }
#endif

Ymodem_Sender: 
#if (EXAMPLE_CONFIG & (Y_MODEM | SENDER))
    __XYM_LOG__("Y modem send start, file_num = [%d], size = [%d]\r\n", file_num, file_p[file_num].size);
	file_num = 0;
    for (ymodem_init(&session); res_sta == XYM_OK; cnt += len)
    {
        if (cnt > 0)
        {
            len = (file_p[file_num].size + XYM_PKT_SIZE_128 - cnt > sizeof(buff) / sizeof(buff[0])) ? (sizeof(buff) / sizeof(buff[0])) : (file_p[file_num].size + XYM_PKT_SIZE_128 - cnt);
            if (len > 0)
            {
                memset(buff, 0xAA, len);
                // set_data(buff, len);
            }
        }
        else /* first f_name or end pkt  */
        {
            len = (file_num < file_list_max_num) ? XYM_PKT_SIZE_128 : 0;
            if (len > 0)
            {
                memset(buff, 0, len);
                /* set a new file struct(custom) */
                encode_file(&file_p[file_num], buff, len);

                /* If you are using a file system, 
                * you need to use a file name to open the corresponding file operation handle
                * (eg: f_handle = f_open(file.name) )
                */
            }
        }
        res_sta = ymodem_transmit(&session, buff, len);
        /* When starting a new file transfer... */
        if (res_sta == XYM_FIL_SET)
        {
            ++file_num;
            cnt = 0; /* new file restart */
            len = 0;
            res_sta = XYM_OK;
        }
    }
#endif

Session_End:
    if (res_sta != XYM_END)
    {
        __XYM_LOG__("X / Y modem session error termination, error code[%d]!\r\n", res_sta);
        return res_sta;
    }
    __XYM_LOG__("X / Y modem session normal end!\r\n");
    return 0;
}
