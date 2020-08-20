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
    OPCODE_RESET_WIFI = 0x04,
    OPCODE_SET_PAIRING_MODE = 0x05,
    OPCODE_COMMAND = 0x06,
    OPCODE_STATUS = 0x07,
    OPCODE_QUERY_STATUS = 0x08
};

enum
{
    TUYA_TYPE_BOOL = 0x01,
    TUYA_TYPE_UINT32 = 0x02
};

typedef struct 
{
    uint8_t dpid; // datapoint id.
    uint8_t type; // type: type=1 for bool
    uint8_t len_h; // always 0
    uint8_t len_l; // always 1
    uint8_t value; // Value of the door sensor. 1 = open/opening, 0 = closed.
} S_TUYA_DATA_BOOL;

typedef struct 
{
    uint8_t dpid; // datapoint id.
    uint8_t type; // type 2 for uint32_t
    uint8_t len_h; // always 0
    uint8_t len_l; // always 4
    uint32_t value; // Value (of what??)
} S_TUYA_DATA_UINT32;

static uint8_t ChksumByte = 0;
static uint8_t TxBufferLen = 0;
static uint8_t TxBuffer[70];
static uint8_t first_heartbeat = 0;
static uint8_t lastKnownDoorState = 0;
static uint8_t pairingMode = 0;

bool wifiResetInProgress = 0;

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
static void UnkownOpcode(uint8_t opcode);
static void RequestPairingMode(uint8_t mode);

//////////////////////////////////////////////////////////////////////

void UartSetup()
{
    // Set baud registers:
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

void StatusReport(bool isOpen, uint8_t dpid)
{
    S_TUYA_DATA_BOOL d;
    lastKnownDoorState = isOpen;
    
    if (!first_heartbeat) return;

    d.dpid = dpid;
    d.len_h = 0;
    d.len_l = sizeof(uint8_t);
    d.type = TUYA_TYPE_BOOL;
    d.value = isOpen ? 0x01 : 0x00; // If sensor shows the door is closed: 0x00.

    Tx(TUYA_HEADER_1);
    Tx(TUYA_HEADER_2);
    Tx(TUYA_VERSION);
    Tx(OPCODE_STATUS);
    Tx(0);
    Tx(sizeof(d));
    TxBytes((uint8_t*)&d, sizeof(d));
    TxChksum();
}

void WifiReset(uint8_t mode)
{
    pairingMode = mode;
    Tx(TUYA_HEADER_1);
    Tx(TUYA_HEADER_2);
    Tx(TUYA_VERSION);
    Tx(OPCODE_RESET_WIFI);
    Tx(0);
    Tx(0);
    TxChksum();

    // Pairing mode will be set later.
    wifiResetInProgress = true;
}

void RequestPairingMode(uint8_t mode)
{
    pairingMode = mode;
    Tx(TUYA_HEADER_1);
    Tx(TUYA_HEADER_2);
    Tx(TUYA_VERSION);
    Tx(OPCODE_SET_PAIRING_MODE);
    Tx(0);
    Tx(sizeof(mode));
    Tx(mode);
    TxChksum();
    
}

void StatusReport_Ex()
{
    S_TUYA_DATA_UINT32 d;
    d.dpid = 7; // This is the datapoint id the stock firmware reports.
    d.type = TUYA_TYPE_UINT32;
    d.len_h = 0;
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
    #define SAMPLE_PRODUCT_INFO  "{\"p\":\"REDACTEDREDACTED\",\"v\":\"1.0.0\",\"m\":0}"
    const uint8_t product_info_len = sizeof(SAMPLE_PRODUCT_INFO)-1;
    char* key = (char*)0x9A58; // This is where the key is located in the stock firmware.

    Tx(TUYA_HEADER_1);
    Tx(TUYA_HEADER_2);
    Tx(TUYA_VERSION);
    Tx(OPCODE_QUERY_PRODUCT_INFO);
    Tx(0);
    Tx(product_info_len);

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
            S_TUYA_DATA_BOOL* d = (S_TUYA_DATA_BOOL*)data;
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
            StatusReport(lastKnownDoorState, 1);
            StatusReport_Ex(); // Must send this dummy data, or else this doesn't work.
        }
        break;

        case OPCODE_REPORT_NETWORK_STATUS:
        {
            ReportModeAck();
        }
        break;
        
        case OPCODE_RESET_WIFI:
        {
            RequestPairingMode(pairingMode);
        }
        break;

        case OPCODE_SET_PAIRING_MODE:
            wifiResetInProgress = false; // This is an ACK.
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

    if (UART1_SR & UART1_SR_RXNE)
    {
        uint8_t rx = UART1_DR;
        UART1_SR &= ~UART1_SR_RXNE;
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
    }
}
