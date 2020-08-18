#include <stdint.h>
#include <stdbool.h>
#include <iostm8s003.h>
#include "tuya.h"

enum TUYA_STUFF {
    TUYA_HEADER_1 = 0x55,
    TUYA_HEADER_2 = 0xAA,
    TUYA_VERSION = 0x03
};

enum {
    UART1_SR_RXNE = (1<<5),
    UART1_SR_TXE  = (1<<7)
};

enum
{
    OPCODE_HEARTBEAT = 0x00,
    OPCODE_QUERY_PRODUCT_INFO = 0x01,
    OPCODE_QUERY_MCU = 0x02,
    OPCODE_REPORT_NETWORK_STATUS = 0x03,
    OPCODE_COMMAND = 0x06,
    OPCODE_STATUS = 0x07,
    OPCODE_QUERY_STATUS = 0x08
};

typedef struct 
{
    uint8_t dpid; // datapoint id
    uint8_t type; // type. always use type=1 for bool
    uint8_t len_h; // always 0
    uint8_t len_l; // always 1
    uint8_t value; // Value of the door sensor. 1 = open/opening, 0 = closed.
} S_TUYA_DATA;

typedef struct 
{
    uint8_t dpid; // datapoint id
    uint8_t type; // type. 2 for uint32_t
    uint8_t len_h; // always 0
    uint8_t len_l; // always 4
    uint32_t value; // Value (???)
} S_TUYA_DATA_EX;

static uint8_t ChksumByte = 0;
static uint8_t TxBufferLen = 0;
static uint8_t TxBuffer[70];
static uint8_t first_heartbeat = 0;
static uint8_t lastKnownDoorState = 0;

//////////////////////////////////////////////////////////////////////

static void Tx(uint8_t byte);
static void TxChksum(void);
static void TxBytes(uint8_t* buffer, uint8_t len);
static void TxCString(char* buffer);
static void HeartBeat(void);
static void QueryProductInfo(void);
static void QueryMCU(void);
static void ReportModeAck(void);
static void Process(uint8_t opcode, uint8_t *data);
static void StatusReport_Ex(void);

//////////////////////////////////////////////////////////////////////

void UartSetup()
{
    // baud:
    const int baud = 9600;
    const uint32_t master_freq = 2000000;
    const uint16_t uart_div = master_freq / baud;

    UART1_BRR1 = (uart_div >> 4) & 0xFF;
    UART1_BRR2 = uart_div & 0xF | ((uart_div >> 12) << 4);

    UART1_CR2 = 0x0c; // enable REN and TEN
    UART1_SR &= ~UART1_SR_RXNE; // ack any would-be junk char in the uart.
}

void TxTask()
{
    static uint8_t TxBufferIndex = 0;
    // Do I care about break?
    //if (UART1_CR4 | 0x10) {...}
    
    if ((UART1_SR & UART1_SR_TXE) && TxBufferIndex < TxBufferLen)
    {
        UART1_SR &= ~UART1_SR_TXE;
        UART1_DR = TxBuffer[TxBufferIndex];
        TxBufferIndex++;
        if (TxBufferIndex >= TxBufferLen)
        {
            TxBufferIndex = 0;
            TxBufferLen = 0;
        }
    }
}

void Tx(uint8_t byte)
{
    ChksumByte += byte;
    TxBuffer[TxBufferLen] = byte;
    TxBufferLen++;
}

void TxChksum(void)
{
    Tx(ChksumByte);
    ChksumByte = 0;
}

void TxBytes(uint8_t* buffer, uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++)
    {
        Tx(buffer[i]);
    }
}

void TxCString(char* buffer)
{
    uint8_t i;
    for (i = 0; buffer[i]; i++)
    {
        Tx(buffer[i]);
    }
}

void UnkownOpcode(uint8_t opcode);

void StatusReport(bool isOpen, uint8_t dpid)
{
    S_TUYA_DATA d = {0x01, 0x01, 0x00, 0x01, 0x01};
    lastKnownDoorState = isOpen;
#if 0
    d.dpid = 0x65; // If reporting first time
#endif
    
    if (!first_heartbeat) return;

    // If sensor shows the door is closed: 0x00.
    d.value = isOpen ? 0x01 : 0x00;
    d.dpid = dpid;

    Tx(TUYA_HEADER_1);
    Tx(TUYA_HEADER_2);
    Tx(TUYA_VERSION);
    Tx(OPCODE_STATUS);
    Tx(0);
    Tx(sizeof(d));
    TxBytes((uint8_t*)&d, sizeof(d));
    TxChksum();
}

void StatusReport_Ex()
{
    S_TUYA_DATA_EX d;
    d.dpid = 7; // datapoint id
    d.type = 2; // type. 2 for uint32_t
    d.len_h = 0; // always 0
    d.len_l = sizeof(d.value);
    d.value = 0; // Value (???)
    Tx(TUYA_HEADER_1);
    Tx(TUYA_HEADER_2);
    Tx(TUYA_VERSION);
    Tx(OPCODE_STATUS);
    Tx(0);
    Tx(sizeof(d));
    TxBytes((uint8_t*)&d, sizeof(d));
    TxChksum();

}

void HeartBeat(void)
{
    Tx(TUYA_HEADER_1);
    Tx(TUYA_HEADER_2);
    Tx(TUYA_VERSION);
    Tx(OPCODE_HEARTBEAT);
    Tx(0);
    Tx(sizeof(first_heartbeat));
    Tx(first_heartbeat);
    TxChksum();
    first_heartbeat = 1;
}

void QueryProductInfo(void)
{
    #define PRODUCT_STATUS  "{\"p\":\"REDACTEDREDACTED\",\"v\":\"1.0.0\",\"m\":0}"
    const char* product_info = PRODUCT_STATUS;
    const uint8_t product_info_len = sizeof(PRODUCT_STATUS)-1;
    char* key = (char*)0x9A58; // This is where the key is located in the stock firmware.

    Tx(TUYA_HEADER_1);
    Tx(TUYA_HEADER_2);
    Tx(TUYA_VERSION);
    Tx(OPCODE_QUERY_PRODUCT_INFO);
    Tx(0);
    Tx(product_info_len);
    //TxBytes(product_info,product_info_len);
    TxCString("{\"p\":");
    TxCString("\"");
    TxCString(key);
    TxCString("\"");
    TxCString(",\"v\":\"1.0.0\",\"m\":0}");
    TxChksum();
}

void QueryMCU(void)
{
    Tx(TUYA_HEADER_1);
    Tx(TUYA_HEADER_2);
    Tx(TUYA_VERSION);
    Tx(OPCODE_QUERY_MCU);
    Tx(0); // 0x0000 = module self-processing mode. (i.e. tuya has no reset pin connected to its GPIO)
    Tx(0);
    TxChksum();
}

void ReportModeAck(void)
{
    Tx(TUYA_HEADER_1);
    Tx(TUYA_HEADER_2);
    Tx(TUYA_VERSION);
    Tx(OPCODE_REPORT_NETWORK_STATUS);
    Tx(0);
    Tx(0);
    TxChksum();
}

void UnkownOpcode(uint8_t opcode)
{
	ChksumByte = 0;
}

void Process(uint8_t opcode, uint8_t *data)
{
    switch (opcode)
    {
        case OPCODE_HEARTBEAT:
        {
            HeartBeat();
        }
        break;

        case OPCODE_COMMAND:
        {
            S_TUYA_DATA* d = (S_TUYA_DATA*)data;
            if (d->value == 1) RxCommand_open = true;
            if (d->value == 0) RxCommand_close = true;
        }
        break;

        case OPCODE_QUERY_PRODUCT_INFO:
        {
            QueryProductInfo();
        }
        break;

        case OPCODE_QUERY_MCU:
        {
            QueryMCU();
        }
        break;

        case OPCODE_QUERY_STATUS:
        {
            StatusReport(lastKnownDoorState, 1); // !
            StatusReport_Ex(); // !
        }
        break;

        case OPCODE_REPORT_NETWORK_STATUS:
        {
            ReportModeAck();
        }
        break;
        
        default:
        {
            UnkownOpcode(opcode);
        }
        break;
    }
}

void RxTask(void)
{
    enum {
        HDR_BYTE_1,
        HDR_BYTE_2,
        MODULE_VER,
        OPCODE_BYTE,
        LEN_BYTE_H,
        LEN_BYTE_L,
        DATA_BYTES,
        CSUM_BYTE,
        INITIAL_STATE = HDR_BYTE_1
    };

    static int state = INITIAL_STATE;
    static uint8_t opcode;
    static uint8_t msgLen;
    static uint8_t data[6];
    static uint8_t dataIndex;
#if 0    
    static uint8_t rxringbuf[16];
    static uint8_t rxringbuf_index;
#endif
    if (UART1_SR & UART1_SR_RXNE)
    {
        uint8_t rx = UART1_DR;
        UART1_SR &= ~UART1_SR_RXNE;
#if 0        
        rxringbuf[rxringbuf_index] = rx;
        rxringbuf_index = (rxringbuf_index + 1);
        if (rxringbuf_index == 16)
        {
            rxringbuf_index = 0;
        }
#else
        switch (state)
        {
            case HDR_BYTE_1: 
                if (rx == TUYA_HEADER_1)
                {
                    state++;
                }
                else
                {
                    state = INITIAL_STATE;
                }
                break;

            case HDR_BYTE_2: 
                if (rx == TUYA_HEADER_2)
                {
                    state++;
                }
                else
                {
                    state = INITIAL_STATE;
                }
                break;

            case MODULE_VER: 
                if (rx == 0x00)
                {
                    state++;
                }
                else
                {
                    state = INITIAL_STATE;
                }
                break;

            case OPCODE_BYTE:
                {
                    opcode = rx;
                    state++;
                }
                break;

            case LEN_BYTE_H:
                if (rx == 0x00)
                {
                    msgLen = 0;
                    dataIndex = 0;
                    state++;
                } 
                else
                {
                    state = INITIAL_STATE;
                }	
                break;

            case LEN_BYTE_L:
                {
                    msgLen = rx;
                    state++;
                    if (msgLen > sizeof(data))
                    {
                        state = INITIAL_STATE;
                    }
                    else if (msgLen > 0)
                    {
                        state = DATA_BYTES;
                    }
                    else
                    {
                        state = CSUM_BYTE;
                    }
  
                }
                break;

            case DATA_BYTES:
                {
                    data[dataIndex] = rx;
                    dataIndex++;
                    if (msgLen == dataIndex)
                    {
                        state++;
                    }
                }
                break;

            case CSUM_BYTE:
                {
                    Process(opcode, data);
                    state = INITIAL_STATE;
                }
                break;
        }
#endif
    }
}
