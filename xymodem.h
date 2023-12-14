/**
 *******************************************************************************************************************************************
 * @file        xymodem.h
 * @brief       X / Y modem transport protocol
 * @since       Change Logs:
 * Date         Author       Notes
 * 2023-11-30   lzh          the first version
 * 2023-12-10   lzh          Fix possible dead loops in EOT response of [ymodem_transmit]
 * 2023-12-14   lzh          sync-change [retry_max] uint32_t => uint8_t, update enum xym_sta: add [XYM_ERROR_INVALID_DATA], del [XYM_ERROR_UNKNOWN]
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
#ifndef __XYMODEM_H__
#define __XYMODEM_H__

#include <stdint.h>

#define XYM_PKT_SIZE_128      (128)  /**< packet valid data size : 128 Bytes */
#define XYM_PKT_SIZE_1024     (1024) /**< packet valid data size : 1024 Bytes */

/** enum X/Y modem session state */
typedef enum xym_sta
{
    XYM_OK = 0,             /**< normal return */
    XYM_END,                /**< protocol exit */
    XYM_FIL_GET,            /**< ymodem file info packet get */
    XYM_FIL_SET,            /**< ymodem file info packet set */
    XYM_CANCEL_REMOTE,      /**< remote cancel */
    XYM_CANCEL_ACTIVE,      /**< active cancel */
    XYM_ERROR_TIMEOUT,      /**< communication timeout */
    XYM_ERROR_RETRANS,      /**< retrans over max-times */
    XYM_ERROR_INVALID_DATA, /**< invalid data */
    XYM_ERROR_HW,           /**< hardware error */
} xym_sta_t;

/**
 * @brief  send data within the set time
 * @param  data  : data
 * @param  cnt   : data size / Bytes
 * @param  tick  : send 1 Bytes timeout / tick
 * @retval enum xym_sta
 */
typedef xym_sta_t xym_cb_send(const uint8_t *data, const uint32_t cnt, const uint32_t tick);

/**
 * @brief  receive data within the set time
 * @param  data  : data
 * @param  cnt   : data size / Bytes
 * @param  tick  : receive 1 Bytes timeout / tick
 * @retval enum xym_sta
 */
typedef xym_sta_t xym_cb_recv(uint8_t *data, const uint32_t cnt, const uint32_t tick);

/**
 * @brief  CRC16 verify data
 * @remark Provide more efficient CRC16, eg: Hardware-CRC16
 * @param  data     : data
 * @param  cnt      : data size / Bytes
 * @retval verify result
 */
typedef uint16_t xym_cb_crc16(const uint8_t *data, const uint32_t cnt);

/** X/Y modem session control struct(Private / Anonymous) */
typedef struct xym_session
{
    uint32_t send_timeout;   /**< How many ticks wait for send 1 Byte */
    uint32_t recv_timeout;   /**< How many ticks wait for receive 1 Byte */
    uint8_t error_max_retry; /**< How many times to retry when an error occurs */
    uint8_t handshake;       /**< Handshake flag : 0-No Handshake; 1-Handshake OK */
    uint8_t crc_flag;        /**< Parity : 0-checksum; 1-CRC16 */
    uint8_t reply_msg;       /**< Reply message for the current package */
    uint32_t seqno;          /**< Packet sequence(xmodem start is 1, ymodem start is 0) */
    xym_cb_send *send;       /**< send data callback(necessary) */
    xym_cb_recv *recv;       /**< recv data callback(necessary) */
    xym_cb_crc16 *crc16;     /**< CRC16 verify data callback(optional) */

} xym_session_t; /* Note: The structure does not allow users to access directly from outside. */

/**
 * @brief  X/Y modem session initialization(register callback and config parameter)
 * @param  send data callback(necessary)
 * @param  recv data callback(necessary)
 * @param  CRC16 verify data callback(optional)
 * @param  How many ticks wait for send 1 Byte
 * @param  How many ticks wait for receive 1 Byte
 * @param  How many times to retry when an error occurs
 * @retval enum xym_sta
 */
xym_sta_t xymodem_session_init(xym_session_t *p,
                               xym_cb_send *send, xym_cb_recv *recv, xym_cb_crc16 *crc16,
                               uint32_t send_timeout, uint32_t recv_timeout, uint8_t error_max_retry);

/**
 * @brief  X/Y modem active cancel session
 * @param  p                 : session control struct
 * @retval XYM_CANCEL_ACTIVE : success over
 * @retval XYM_ERROR_HW      : hardware error
 */
xym_sta_t xymodem_active_cancel(xym_session_t *p);

/**
 * @brief  Xmodem session init
 * @param  p : session control struct
 * @retval \
 */
void xmodem_init(xym_session_t *p);

/**
 * @brief  Xmodem receive data
 * @param  p      : session control struct
 * @param  buff   : returned data buffer (128 or 1024 Bytes)
 * @param  size   : size of returned data (/ Bytes)
 * @retval XYM_OK : return a packet of valid data
 * @retval other  : session over (normal or error)
 * @note   The function needs to be continuously polled until the end
 * @remark Support Xmodem-1K and Xmodem-128 (standard), depending on the sender settings
 */
xym_sta_t xmodem_receive(xym_session_t *p, uint8_t *buff, uint16_t *size);

/**
 * @brief  Xmodem transmit data
 * @param  p      : session control struct
 * @param  buff   : data buffer (128 or 1024 Bytes)
 * @param  size   : size of data (/ Bytes), If the size is 0, exec end.
 * @retval XYM_OK : transmit OK, continue to the next transmit
 * @retval other  : session over (normal or error)
 * @note   The function needs to be continuously polled until the end
 * @remark Support Xmodem-1K and Xmodem-128 (standard), depending on the sender settings
 */
xym_sta_t xmodem_transmit(xym_session_t *p, uint8_t *buff, const uint16_t size);

/**
 * @brief  Ymodem session init
 * @param  p : session control struct
 * @retval \
 */
void ymodem_init(xym_session_t *p);

/**
 * @brief  Ymodem receive data
 * @param  p      : session control struct
 * @param  buff   : returned data buffer (128 or 1024 Bytes)
 * @param  size   : size of returned data (/ Bytes)
 * @retval XYM_OK      : return a packet of valid data
 * @retval XYM_FIL_GET : return a packet of file info
 * @retval other       : session over (normal or error)
 * @note   The function needs to be continuously polled until the end
 * @remark No support Ymodem-g, because it is easy to cause buffer-overflow
 */
xym_sta_t ymodem_receive(xym_session_t *p, uint8_t *buff, uint16_t *size);

/**
 * @brief  Ymodem transmit data
 * @param  p      : session control struct
 * @param  buff   : data buffer (128 or 1024 Bytes)
 * @param  size   : size of data (/ Bytes), If the size is 0, exec next file transmit or end.
 * @retval XYM_OK      : transmit OK, continue to the next transmit
 * @retval XYM_FIL_SET : set file info packet
 * @retval other       : session over (normal or error)
 * @note   The function needs to be continuously polled until the end
 * @remark No support Ymodem-g, because it is easy to cause buffer-overflow
 */
xym_sta_t ymodem_transmit(xym_session_t *p, uint8_t *buff, const uint16_t size);

#endif /* __XYMODEM_H__ */
