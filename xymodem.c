/**
 *******************************************************************************************************************************************
 * @file        xymodem.c
 * @brief       X / Y modem transport protocol
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
#include <string.h>
#include "xymodem.h"

/*******************************************************************************************************************************************
 * Private Prototype
 *******************************************************************************************************************************************/
/* Special byte definition of XYmodem protocal */
#define SOH                     (0x01) /**< (Sender) start of 128-byte data packet */
#define STX                     (0x02) /**< (Sender) start of 1024-byte data packet */
#define EOT                     (0x04) /**< (Sender) end of transmission */
#define ACK                     (0x06) /**< (Receiver) acknowledge */
#define NAK                     (0x15) /**< (Receiver) negative acknowledge */
#define CANCEL                  (0x18) /**< (Sender / Receiver) two of these in succession aborts transfer */
#define CRC16_FLAG              (0x43) /**< (Receiver) 'C' == 0x43, request 16-bit CRC */
#define CTRLZ                   (0x1A) /**< (Sender) End-of-file indicated by ^Z (one or more) */

/* X/Y modem verify data */
static uint16_t xymodem_verify_data(const xym_session_t *p, const uint8_t *data, const uint32_t cnt);

/*******************************************************************************************************************************************
 * Public Function
 *******************************************************************************************************************************************/
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
                               uint32_t send_timeout, uint32_t recv_timeout, uint8_t error_max_retry)
{
    if (!(p && send && recv))
    {
        return XYM_ERROR_UNKNOWN;
    }
    p->send = send;
    p->recv = recv;
    p->crc16 = crc16;
    p->send_timeout = send_timeout;
    p->recv_timeout = recv_timeout;
    p->error_max_retry = error_max_retry;
    return XYM_OK;
}

/**
 * @brief  X/Y modem active cancel session
 * @param  p                 : session control struct
 * @retval XYM_CANCEL_ACTIVE : success over
 * @retval XYM_ERROR_HW      : hardware error
 */
xym_sta_t xymodem_active_cancel(xym_session_t *p)
{
    uint8_t cancel_singal_cnt = 3; /* third time lucky */
    const uint8_t reply_msg = CANCEL;
    for (; cancel_singal_cnt > 0; --cancel_singal_cnt)
    {
        if (XYM_OK != p->send(&reply_msg, 1, p->send_timeout))
        {
            return XYM_ERROR_HW;
        }
    }
    return XYM_CANCEL_ACTIVE;
}

/**
 * @brief  Xmodem session init
 * @param  p : session control struct
 * @retval \
 */
void xmodem_init(xym_session_t *p)
{
    p->handshake = 0;
    p->crc_flag = 1;
    p->reply_msg = (p->handshake == 0 && p->crc_flag != 0) ? CRC16_FLAG : NAK;
    p->seqno = 1; /* xmodem start is 1, ymodem start is 0 */
}

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
xym_sta_t xmodem_receive(xym_session_t *p, uint8_t *buff, uint16_t *size)
{
    uint8_t header[3] = {0};    /* header[Special byte, Packet sequence, ~Packet sequence] */
    uint8_t tail[2] = {0};      /* tail[CheckSum / CRC16_H, Reserve / CRC16_L] */
    uint32_t retry = 0;         /* retry counter */
    uint16_t pkt_data_size = 0; /* the valid data length of packet */
    uint8_t handshake_flag = 0; /* two handshakes(CRC16 or CheckSum) */

    *size = 0; /* zero clearing */

    for (retry = 0; retry <= p->error_max_retry; ++retry)
    {
        /* reply */
        if (XYM_OK != p->send(&p->reply_msg, 1, p->send_timeout))
        {
            continue;
        }
        /* get special byte */
        if (XYM_OK != p->recv(header, 1, p->recv_timeout))
        {
            if (p->handshake == 0 && handshake_flag == 0 && retry >= p->error_max_retry)
            {
                ++handshake_flag;
                retry = 0;
                p->crc_flag ^= 1; /* Replace handshake command */
            }
            p->reply_msg = (p->handshake == 0 && p->crc_flag != 0) ? CRC16_FLAG : NAK;
            continue;
        }
        p->handshake = 1;
        /* parsing special byte */
        switch (header[0])
        {
        case SOH:
            pkt_data_size = XYM_PKT_SIZE_128;
            break;
        case STX:
            pkt_data_size = XYM_PKT_SIZE_1024;
            break;
        case EOT:
            p->reply_msg = ACK;
            p->send(&p->reply_msg, 1, p->send_timeout);
            return XYM_END;
        case CANCEL:
            if (XYM_OK == p->recv(header, 1, p->recv_timeout))
            {
                if (header[0] == CANCEL)
                {
                    p->reply_msg = ACK;
                    p->send(&p->reply_msg, 1, p->send_timeout);
                    return XYM_CANCEL_REMOTE;
                }
            }
        default:
            xymodem_active_cancel(p);
            return XYM_ERROR_UNKNOWN;
        }
        /* get packet sequence */
        if (XYM_OK != p->recv(&header[1], 2, p->recv_timeout))
        {
            p->reply_msg = NAK;
            continue;
        }
        /* get valid data */
        if (XYM_OK != p->recv(buff, pkt_data_size, p->recv_timeout))
        {
            p->reply_msg = NAK;
            continue;
        }
        /* get verify value */
        if (XYM_OK != p->recv(tail, (p->crc_flag != 0) ? 2 : 1, p->recv_timeout))
        {
            p->reply_msg = NAK;
            continue;
        }
        /* verify packet sequence complement */
        if (header[1] != (~header[2] & 0xFF))
        {
            p->reply_msg = NAK;
            continue;
        }
        /* select CRC16[MSB] or CheckSum[zero clearing] */
        if (((p->crc_flag != 0) ? ((tail[0] << 8) | tail[1]) : ((0x00 << 8) | tail[0])) != xymodem_verify_data(p, buff, pkt_data_size))
        {
            p->reply_msg = NAK;
            continue;
        }
        /* verify packet sequence */
        if ((p->seqno & 0xFF) != header[1])
        {
            p->reply_msg = (((p->seqno & 0xFF) - 1) == header[1]) ? ACK : NAK; /* It could be the previous package */
            continue;
        }
        /* it is valid data */
        p->seqno++;
        p->reply_msg = ACK;
        *size = pkt_data_size;
        return XYM_OK;
    }
    xymodem_active_cancel(p);
    return XYM_ERROR_RETRANS;
}

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
xym_sta_t xmodem_transmit(xym_session_t *p, uint8_t *buff, const uint16_t size)
{
    uint8_t header[3] = {0};    /* header[Special byte, Packet sequence, ~Packet sequence] */
    uint8_t tail[2] = {0};      /* tail[CheckSum / CRC16_H, Reserve / CRC16_L] */
    uint32_t retry = 0;         /* retry counter */
    uint16_t pkt_data_size = 0; /* the data length of packet */
    uint16_t check_sum = 0;     /* check sum or CRC16 result */

    /* EOT */
    if (size == 0)
    {
        header[0] = EOT;
        for (retry = 0; retry <= p->error_max_retry; ++retry)
        {
            if (XYM_OK != p->send(header, 1, p->send_timeout))
            {
                continue;
            }
            /* wait ACK */
            if (XYM_OK != p->recv(&p->reply_msg, 1, p->recv_timeout))
            {
                continue;
            }
            if (p->reply_msg == ACK)
            {
                return XYM_END;
            }
        }
        xymodem_active_cancel(p);
        return XYM_ERROR_RETRANS;
    }
    /* Handshake */
    for (retry = 0; p->handshake == 0 && retry <= p->error_max_retry; retry += (p->handshake == 0) ? 1 : 0)
    {
        /* wait handshake */
        if (XYM_OK != p->recv(&p->reply_msg, 1, p->recv_timeout))
        {
            continue;
        }
        /* parsing handshake */
        switch (p->reply_msg)
        {
        case CRC16_FLAG:
            p->crc_flag = 1;
            p->handshake = 1;
            break;
        case NAK:
            p->crc_flag = 0;
            p->handshake = 1;
            break;
        case CANCEL:
            if (XYM_OK == p->recv(&p->reply_msg, 1, p->recv_timeout))
            {
                if (p->reply_msg == CANCEL)
                {
                    return XYM_CANCEL_REMOTE;
                }
            }
        case ACK:
        default:
            xymodem_active_cancel(p);
            return XYM_ERROR_UNKNOWN;
        }
    }
    if (retry > p->error_max_retry)
    {
        xymodem_active_cancel(p);
        return XYM_ERROR_RETRANS;
    }

    /* packet init */
    pkt_data_size = (size > XYM_PKT_SIZE_128) ? XYM_PKT_SIZE_1024 : XYM_PKT_SIZE_128;
    header[0] = (pkt_data_size == XYM_PKT_SIZE_128) ? SOH : STX;
    header[1] = p->seqno & 0xFF;
    header[2] = ~header[1];
    /* End-of-file indicated by ^Z */
    if (size != pkt_data_size)
    {
        memset(&buff[size], CTRLZ, pkt_data_size - size);
    }
    /* select CRC16[MSB] or CheckSum[zero clearing] */
    check_sum = xymodem_verify_data(p, buff, pkt_data_size);
    tail[0] = (check_sum >> 8) & 0xFF;
    tail[1] = check_sum & 0xFF;

    for (retry = 0; retry <= p->error_max_retry; ++retry)
    {
        /* send header */
        if (XYM_OK != p->send(header, sizeof(header) / sizeof(header[0]), p->send_timeout))
        {
            return XYM_ERROR_HW;
        }
        /* send valid data */
        if (XYM_OK != p->send(buff, pkt_data_size, p->send_timeout))
        {
            return XYM_ERROR_HW;
        }
        /* send checksum */
        if (XYM_OK != p->send(tail, (p->crc_flag != 0) ? 2 : 1, p->send_timeout))
        {
            return XYM_ERROR_HW;
        }
        /* wait reply */
        if (XYM_OK != p->recv(&p->reply_msg, 1, p->recv_timeout))
        {
            continue;
        }
        /* parsing reply msg */
        switch (p->reply_msg)
        {
        case ACK:
            p->seqno++;
            return XYM_OK;
        case NAK:
        case CRC16_FLAG:
            break;
        case CANCEL:
            if (XYM_OK == p->recv(&p->reply_msg, 1, p->recv_timeout))
            {
                if (p->reply_msg == CANCEL)
                {
                    return XYM_CANCEL_REMOTE;
                }
            }
        default:
            xymodem_active_cancel(p);
            return XYM_ERROR_UNKNOWN;
        }
    }
    xymodem_active_cancel(p);
    return XYM_ERROR_RETRANS;
}

/**
 * @brief  Ymodem session init
 * @param  p : session control struct
 * @retval \
 */
void ymodem_init(xym_session_t *p)
{
    p->handshake = 0;
    p->crc_flag = 1;
    p->reply_msg = (p->handshake == 0 && p->crc_flag != 0) ? CRC16_FLAG : NAK;
    p->seqno = 0; /* xmodem start is 1, ymodem start is 0 */
}

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
xym_sta_t ymodem_receive(xym_session_t *p, uint8_t *buff, uint16_t *size)
{
    uint8_t header[3] = {0};    /* header[Special byte, Packet sequence, ~Packet sequence] */
    uint8_t tail[2] = {0};      /* tail[CRC16_H, CRC16_L] */
    uint32_t retry = 0;         /* retry counter */
    uint16_t pkt_data_size = 0; /* the valid data length of packet */
    uint8_t eot_flag = 0;       /* wave twice */
    uint8_t continue_reply = 0; /* continue reply flag */

    *size = 0; /* zero clearing */

    for (retry = 0; retry <= p->error_max_retry; retry += (continue_reply == 0) ? 1 : 0)
    {
        continue_reply = 0;
        /* reply */
        if (XYM_OK != p->send(&p->reply_msg, 1, p->send_timeout))
        {
            continue;
        }
        /* continue reply(After First Filename packet || After the second EOT) */
        if (p->handshake == 0 && p->reply_msg == ACK)
        {
            p->reply_msg = CRC16_FLAG;
            continue_reply = 1; /* it is not an error */
            continue;
        }
        /* get special byte */
        if (XYM_OK != p->recv(header, 1, p->recv_timeout))
        {
            p->reply_msg = (p->handshake == 0) ? CRC16_FLAG : NAK;
            continue;
        }
        p->handshake = 1;
        /* parsing special byte */
        switch (header[0])
        {
        case SOH:
            pkt_data_size = XYM_PKT_SIZE_128;
            break;
        case STX:
            pkt_data_size = XYM_PKT_SIZE_1024;
            break;
        case EOT:
            p->reply_msg = (eot_flag == 0) ? NAK : ACK;
            if (++eot_flag == 2)
            {
                eot_flag = 0;
                /* restart a new file */
                p->handshake = 0;
                p->seqno = 0;
            }
            continue_reply = 1; /* it is not an error */
            continue;

        case CANCEL:
            if (XYM_OK == p->recv(header, 1, p->recv_timeout))
            {
                if (header[0] == CANCEL)
                {
                    p->reply_msg = ACK;
                    p->send(&p->reply_msg, 1, p->send_timeout);
                    return XYM_CANCEL_REMOTE;
                }
            }
        default:
            xymodem_active_cancel(p);
            return XYM_ERROR_UNKNOWN;
        }
        /* get packet sequence */
        if (XYM_OK != p->recv(&header[1], 2, p->recv_timeout))
        {
            p->reply_msg = NAK;
            continue;
        }
        /* get valid data */
        if (XYM_OK != p->recv(buff, pkt_data_size, p->recv_timeout))
        {
            p->reply_msg = NAK;
            continue;
        }
        /* get verify value */
        if (XYM_OK != p->recv(tail, 2, p->recv_timeout))
        {
            p->reply_msg = NAK;
            continue;
        }
        /* verify packet sequence complement */
        if (header[1] != (~header[2] & 0xFF))
        {
            p->reply_msg = NAK;
            continue;
        }
        /* CRC16[MSB] */
        if (((tail[0] << 8) | tail[1]) != xymodem_verify_data(p, buff, pkt_data_size))
        {
            p->reply_msg = NAK;
            continue;
        }
        /* verify packet sequence */
        if ((p->seqno & 0xFF) != header[1])
        {
            p->reply_msg = (((p->seqno & 0xFF) - 1) == header[1]) ? ACK : NAK; /* It could be the previous package */
            continue;
        }
        /* Filename packet is first */
        if (p->seqno == 0)
        {
            /* Filename packet is empty, end session */
            if (buff[0] == 0 && tail[0] == 0 && tail[1] == 0)
            {
                p->reply_msg = ACK;
                p->send(&p->reply_msg, 1, p->send_timeout);
                return XYM_END;
            }
            /* Filename packet has valid data */
            p->handshake = 0;
        }
        /* it is valid data */
        p->seqno++;
        p->reply_msg = ACK;
        *size = pkt_data_size;
        return (p->handshake) ? XYM_OK : XYM_FIL_GET;
    }
    xymodem_active_cancel(p);
    return XYM_ERROR_RETRANS;
}

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
xym_sta_t ymodem_transmit(xym_session_t *p, uint8_t *buff, const uint16_t size)
{
    uint8_t header[3] = {0};    /* header[Special byte, Packet sequence, ~Packet sequence] */
    uint8_t tail[2] = {0};      /* tail[CheckSum / CRC16_H, Reserve / CRC16_L] */
    uint32_t retry = 0;         /* retry counter */
    uint16_t pkt_data_size = 0; /* the data length of packet */
    uint8_t eot_flag = 0;       /* wave twice */
    uint16_t check_sum = 0;     /* check sum or CRC16 result */
    uint8_t f_pkt_flag = 0;     /* file pkt flag */

    /* EOT */
    if (size == 0 && p->handshake > 0)
    {
        header[0] = EOT;
        for (retry = 0; retry <= p->error_max_retry; retry += (eot_flag != 1) ? 1 : 0)
        {
            if (XYM_OK != p->send(header, 1, p->send_timeout))
            {
                continue;
            }
            /* wait ACK */
            if (XYM_OK != p->recv(&p->reply_msg, 1, p->recv_timeout))
            {
                continue;
            }
            if (p->reply_msg == NAK)
            {
                ++eot_flag; /* eot_flag == 0 is true, eot_flag > 0 is an error */
                continue;
            }
            if (p->reply_msg == ACK)
            {
                p->handshake = 0;
                p->seqno = 0;
                return XYM_FIL_SET;
            }
        }
        if (retry > p->error_max_retry)
        {
            xymodem_active_cancel(p);
            return XYM_ERROR_RETRANS;
        }
    }

    /* Handshake */
    for (retry = 0; p->handshake == 0 && retry <= p->error_max_retry; retry += (p->handshake == 0) ? 1 : 0)
    {
        /* wait handshake */
        if (XYM_OK != p->recv(&p->reply_msg, 1, p->recv_timeout))
        {
            continue;
        }
        /* parsing handshake */
        switch (p->reply_msg)
        {
        case CRC16_FLAG:
            p->crc_flag = 1;
            p->handshake = 1;
            f_pkt_flag = 1;
            break;
        case CANCEL:
            if (XYM_OK == p->recv(&p->reply_msg, 1, p->recv_timeout))
            {
                if (p->reply_msg == CANCEL)
                {
                    return XYM_CANCEL_REMOTE;
                }
            }
        case NAK:
        case ACK:
        default:
            xymodem_active_cancel(p);
            return XYM_ERROR_UNKNOWN;
        }
    }
    if (retry > p->error_max_retry)
    {
        xymodem_active_cancel(p);
        return XYM_ERROR_RETRANS;
    }

    /* packet init */
    pkt_data_size = (size > XYM_PKT_SIZE_128) ? XYM_PKT_SIZE_1024 : XYM_PKT_SIZE_128;
    header[0] = (pkt_data_size == XYM_PKT_SIZE_128) ? SOH : STX;
    header[1] = p->seqno & 0xFF;
    header[2] = ~header[1];
    /* End-of-file indicated by ^Z or 0x00 */
    if (size != pkt_data_size)
    {
        memset(&buff[size], (size > 0) ? CTRLZ : 0x00, pkt_data_size - size);
    }
    /* select CRC16[MSB] or CheckSum[zero clearing] */
    check_sum = xymodem_verify_data(p, buff, pkt_data_size);
    tail[0] = (check_sum >> 8) & 0xFF;
    tail[1] = check_sum & 0xFF;

    for (retry = 0; retry <= p->error_max_retry; ++retry)
    {
        /* send header */
        if (XYM_OK != p->send(header, sizeof(header) / sizeof(header[0]), p->send_timeout))
        {
            return XYM_ERROR_HW;
        }
        /* send valid data */
        if (XYM_OK != p->send(buff, pkt_data_size, p->send_timeout))
        {
            return XYM_ERROR_HW;
        }
        /* send checksum */
        if (XYM_OK != p->send(tail, (p->crc_flag != 0) ? 2 : 1, p->send_timeout))
        {
            return XYM_ERROR_HW;
        }
        /* wait reply */
        if (XYM_OK != p->recv(&p->reply_msg, 1, p->recv_timeout))
        {
            continue;
        }
        /* parsing reply msg */
        switch (p->reply_msg)
        {
        case ACK:
            if (f_pkt_flag > 0 && p->seqno == 0)
            {
                p->handshake = 0;
            }
            p->seqno++;
            return (size > 0) ? XYM_OK : XYM_END;
        case NAK:
        case CRC16_FLAG:
            break;
        case CANCEL:
            if (XYM_OK == p->recv(&p->reply_msg, 1, p->recv_timeout))
            {
                if (p->reply_msg == CANCEL)
                {
                    return XYM_CANCEL_REMOTE;
                }
            }
        default:
            xymodem_active_cancel(p);
            return XYM_ERROR_UNKNOWN;
        }
    }
    xymodem_active_cancel(p);
    return XYM_ERROR_RETRANS;
}

/*******************************************************************************************************************************************
 * Private Function
 *******************************************************************************************************************************************/
/**
 * @brief  X/Y modem verify data
 * @param  p        : session control struct
 * @param  data     : data
 * @param  cnt      : data size / Bytes
 * @retval uint16_t : verify result
 */
static uint16_t xymodem_verify_data(const xym_session_t *p, const uint8_t *data, const uint32_t cnt)
{
    uint16_t result = 0;
    uint32_t i = 0;
    uint8_t j = 0;

    /* bulid-in checksum */
    if (p->crc_flag == 0)
    {
        for (i = 0; i < cnt; ++i)
        {
            result += *data++;
        }
        return result;
    }

    if (p->crc16)
    {
        return p->crc16(data, cnt);
    }

    /* bulid-in CRC SoftWare:
     * WIDTH  : 16 bit
     * POLY   : 1021 (x16 + x12 + x5 + 1)
     * INIT   : 0
     * REFIN  : false
     * REFOUT : false
     * XOROUT : 0
     */
    for (i = 0; i < cnt; ++i)
    {
        result = result ^ (*data++ << 8);
        for (j = 0; j < 8; ++j)
        {
            if ((result & 0x8000) != 0)
            {
                result = (result << 1) ^ 0x1021;
            }
            else
            {
                result = result << 1;
            }
        }
    }
    return result;
}
