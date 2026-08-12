/*
 * rf_modbus_receiver_cc1352.c
 *
 * Receives the 17-byte simulated Modbus RF frame sent by the sender
 * firmware and DECODES it (pH/Temp floats + CRC check) before printing,
 * instead of forwarding the raw binary payload to the console UART.
 *
 * This is the fix for: "receiver gets data but it's garbled/unreadable
 * with the same characters repeating" - that was raw binary being
 * printed as if it were text. Decode it the same way the sender does.
 */

/***** Includes *****/
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* TI Drivers */
#include <ti/drivers/rf/RF.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/dpl/ClockP.h>

/* Driverlib Header files */
#include DeviceFamily_constructPath(driverlib/rf_prop_mailbox.h)

/* Board Header files */
#include "ti_drivers_config.h"

/* Application Header files */
#include "RFQueue.h"
#include <ti_radio_config.h>

/***** Defines *****/
#define DATA_ENTRY_HEADER_SIZE 8
#define MAX_LENGTH              64
#define NUM_DATA_ENTRIES        2
#define NUM_APPENDED_BYTES      3   /* header byte handled by RFQueue; RSSI + status appended by radio */
#define NO_PACKET                0
#define PACKET_RECEIVED          1
#define MODBUS_RX_LEN            17
#define RSSI_PREFIX_MAX_LEN      64

/***** Globals *****/
static RF_Object rfObject;
static RF_Handle rfHandle;

UART2_Handle uart;
UART2_Params uartParams;

volatile uint8_t packetRxCb;
volatile int8_t lastRssi;
volatile uint32_t lastRssiTimestampMs;

#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN (rxDataEntryBuffer, 4);
static uint8_t rxDataEntryBuffer[RF_QUEUE_DATA_ENTRY_BUFFER_SIZE(NUM_DATA_ENTRIES, MAX_LENGTH, NUM_APPENDED_BYTES)];
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma data_alignment = 4
static uint8_t rxDataEntryBuffer[RF_QUEUE_DATA_ENTRY_BUFFER_SIZE(NUM_DATA_ENTRIES, MAX_LENGTH, NUM_APPENDED_BYTES)];
#elif defined(__GNUC__)
static uint8_t rxDataEntryBuffer[RF_QUEUE_DATA_ENTRY_BUFFER_SIZE(NUM_DATA_ENTRIES, MAX_LENGTH, NUM_APPENDED_BYTES)]
                                                  __attribute__((aligned(4)));
#else
#error This compiler is not supported.
#endif

static dataQueue_t dataQueue;
static rfc_dataEntryGeneral_t* currentDataEntry;
static uint8_t packetLength;
static uint8_t* packetDataPointer;
static uint8_t packet[MAX_LENGTH + NUM_APPENDED_BYTES - 1];
static char uartTxBuffer[RSSI_PREFIX_MAX_LEN + MAX_LENGTH];

/***** Function declarations *****/
static void ReceivedOnRFcallback(RF_Handle h, RF_CmdHandle ch, RF_EventMask e);
static uint16_t modbusCRC16(const uint8_t *data, size_t len);
static float bytesToFloat(const uint8_t *b);

/* ======== mainThread ======== */
void *mainThread(void *arg0)
{
    packetRxCb = NO_PACKET;
    lastRssi = 0;
    lastRssiTimestampMs = 0;

    RF_Params rfParams;
    RF_Params_init(&rfParams);

    if (RFQueue_defineQueue(&dataQueue, rxDataEntryBuffer, sizeof(rxDataEntryBuffer),
                             NUM_DATA_ENTRIES, MAX_LENGTH + NUM_APPENDED_BYTES))
    {
        while (1);
    }

    GPIO_setConfig(CONFIG_GPIO_RLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(CONFIG_GPIO_RLED, CONFIG_GPIO_LED_OFF);
    GPIO_setConfig(CONFIG_GPIO_GLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(CONFIG_GPIO_GLED, CONFIG_GPIO_LED_OFF);

    RF_cmdPropRx.pQueue = &dataQueue;
    RF_cmdPropRx.rxConf.bAutoFlushIgnored = 1;
    RF_cmdPropRx.rxConf.bAutoFlushCrcErr = 1;
    RF_cmdPropRx.maxPktLen = MAX_LENGTH;
    RF_cmdPropRx.pktConf.bRepeatOk = 1;
    RF_cmdPropRx.pktConf.bRepeatNok = 1;
    RF_cmdPropRx.rxConf.bAppendRssi = 1;

    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;
    uartParams.readMode = UART2_Mode_BLOCKING;
    uartParams.writeMode = UART2_Mode_BLOCKING;
    uart = UART2_open(CONFIG_UART2_0, &uartParams);

    const char startMsg[] = "\r\n[SYS] RF Modbus receiver started (CC1352P7):\r\n";
    UART2_write(uart, startMsg, sizeof(startMsg), NULL);

    rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup*)&RF_cmdPropRadioDivSetup, &rfParams);
    RF_postCmd(rfHandle, (RF_Op*)&RF_cmdFs, RF_PriorityNormal, NULL, 0);
    RF_postCmd(rfHandle, (RF_Op*)&RF_cmdPropRx, RF_PriorityNormal, &ReceivedOnRFcallback, RF_EventRxEntryDone);

    while (1)
    {
        if (packetRxCb)
        {
            GPIO_toggle(CONFIG_GPIO_GLED);

            if (packetLength == MODBUS_RX_LEN)
            {
                uint16_t calcCRC = modbusCRC16(packet, MODBUS_RX_LEN - 2);
                uint16_t rxCRC = packet[MODBUS_RX_LEN - 2] | (packet[MODBUS_RX_LEN - 1] << 8);

                if (rxCRC == calcCRC)
                {
                    /* Same offsets the sender used: pH at byte 3, Temp at byte 11 */
                    float ph = bytesToFloat(&packet[3]);
                    float temp = bytesToFloat(&packet[11]);

                    int len = snprintf(uartTxBuffer, sizeof(uartTxBuffer),
                                        "[t=%lu ms][RSSI: %d dBm] pH: %.2f | Temp: %.1f C\r\n",
                                        (unsigned long)lastRssiTimestampMs, lastRssi, ph, temp);
                    if (len > 0)
                    {
                        UART2_write(uart, uartTxBuffer,
                                    (len < (int)sizeof(uartTxBuffer)) ? (size_t)len : sizeof(uartTxBuffer) - 1,
                                    NULL);
                    }
                }
                else
                {
                    const char errMsg[] = "[ERR] CRC mismatch on received packet\r\n";
                    UART2_write(uart, errMsg, sizeof(errMsg), NULL);
                }
            }
            else
            {
                char errBuf[64];
                int n = snprintf(errBuf, sizeof(errBuf),
                                  "[ERR] Unexpected packet length: %u\r\n", packetLength);
                if (n > 0)
                {
                    UART2_write(uart, errBuf, (size_t)n, NULL);
                }
            }

            packetRxCb = NO_PACKET;
        }
    }
}

/* Callback function called when data is received via RF */
void ReceivedOnRFcallback(RF_Handle h, RF_CmdHandle ch, RF_EventMask e)
{
    if (e & RF_EventRxEntryDone)
    {
        GPIO_toggle(CONFIG_GPIO_RLED);

        currentDataEntry = RFQueue_getDataEntry();

        packetLength      = *(uint8_t*)(&currentDataEntry->data);
        packetDataPointer = (uint8_t*)(&currentDataEntry->data + 1);

        lastRssi = (int8_t)packetDataPointer[packetLength];
        lastRssiTimestampMs = (uint32_t)(((uint64_t)ClockP_getSystemTicks() *
                                           ClockP_getSystemTickPeriod()) / 1000ULL);

        memcpy(packet, packetDataPointer, (packetLength + 1));

        RFQueue_nextEntry();

        packetRxCb = PACKET_RECEIVED;
    }
}

/* Must match the sender's CRC routine exactly */
static uint16_t modbusCRC16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* Must match the sender's byte order exactly (big-endian wire -> little-endian ARM) */
static float bytesToFloat(const uint8_t *b)
{
    uint8_t swapped[4] = {b[3], b[2], b[1], b[0]};
    float val;
    memcpy(&val, swapped, sizeof(val));
    return val;
}
