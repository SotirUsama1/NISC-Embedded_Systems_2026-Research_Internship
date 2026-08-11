/*
 * ph_temp_sensor_cc1352.c
 *
 * CC1352P7-1 LaunchPad port of the original AVR pH/Temperature probe reader.
 *
 * Behavior mirrors the AVR original:
 *   1. Send a fixed 8-byte Modbus-RTU "read holding registers" query to the
 *      probe over an RS-485 link (direction pin toggled around the transfer).
 *   2. Receive the 17-byte response.
 *   3. Pull the pH float out of bytes [3..6] and the Temp float out of
 *      bytes [11..14] (byte-swapped, exactly as the AVR code did).
 *   4. Format both as strings and print them out (originally to an LCD;
 *      here, out a UART so you can view it in a serial terminal).
 *   5. Wait ~10s and repeat.
 *
 * SysConfig requirements for this file to build:
 *   - Two UART2 instances:
 *       CONFIG_UART2_0  -> wired to the RS-485 transceiver / probe
 *       CONFIG_UART2_1  -> the LaunchPad's XDS110 backchannel (console out)
 *   - One GPIO output for RS-485 direction control:
 *       CONFIG_GPIO_RS485_DIR
 *   - (Optional, kept from the original example) two GPIO outputs for
 *     activity LEDs: CONFIG_GPIO_RLED, CONFIG_GPIO_GLED
 *
 * Rename these to match whatever SysConfig actually generates for your
 * board file if it picks different default names.
 */

/***** Includes *****/
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>     /* sleep() */

/* TI Drivers */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>

/* Board Header file (SysConfig generated) */
#include "ti_drivers_config.h"

/***** Defines *****/
#define SENSOR_CMD_LEN      8   /* Modbus query length */
#define SENSOR_RESP_LEN     17  /* addr+func+bytecount+12 data bytes+CRC16 */
#define PRINT_BUF_LEN        64

/***** Globals *****/
static UART2_Handle uartSensor;   /* RS-485 link to the probe */
static UART2_Handle uartConsole;  /* Backchannel, for human-readable output */

/* Fixed Modbus query, identical to the AVR original */
static const uint8_t sensorCommand[SENSOR_CMD_LEN] =
    {0x03, 0x03, 0x00, 0x00, 0x00, 0x06, 0xC4, 0x2A};

/***** Function declarations *****/
static void float_to_string(float *value, char *buffer, int precision);
static void rs485SetTxMode(void);
static void rs485SetRxMode(void);

/* ======== mainThread ======== */
void *mainThread(void *arg0)
{
    uint8_t response[SENSOR_RESP_LEN];
    char    phStr[16];
    char    tempStr[16];
    char    printBuf[PRINT_BUF_LEN];
    float   value;
    uint8_t pHBytes[4];
    uint8_t tempBytes[4];
    size_t  bytesWritten = 0;
    size_t  bytesRead = 0;
    int_fast16_t status;

    /* Activity LEDs (optional, kept from the original example) */
    GPIO_setConfig(CONFIG_GPIO_RLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(CONFIG_GPIO_RLED, CONFIG_GPIO_LED_OFF);

    GPIO_setConfig(CONFIG_GPIO_GLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(CONFIG_GPIO_GLED, CONFIG_GPIO_LED_OFF);

    /* RS-485 direction control pin, starts in receive mode */
    GPIO_setConfig(CONFIG_GPIO_RS485_DIR, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* --- Sensor UART: blocking mode, same idea as the AVR's polled USART --- */
    UART2_Params sensorParams;
    UART2_Params_init(&sensorParams);
    sensorParams.baudRate   = 9600;   /* set to match your probe's Modbus baud */
    sensorParams.readMode   = UART2_Mode_BLOCKING;
    sensorParams.writeMode  = UART2_Mode_BLOCKING;

    uartSensor = UART2_open(CONFIG_UART2_0, &sensorParams);
    if (uartSensor == NULL)
    {
        while (1) {}
    }

    /* --- Console UART: for printing results, replaces the LCD --- */
    UART2_Params consoleParams;
    UART2_Params_init(&consoleParams);
    consoleParams.baudRate  = 115200;
    consoleParams.readMode  = UART2_Mode_BLOCKING;
    consoleParams.writeMode = UART2_Mode_BLOCKING;

    uartConsole = UART2_open(CONFIG_UART2_1, &consoleParams);
    if (uartConsole == NULL)
    {
        while (1) {}
    }

    const char startMsg[] = "\r\npH/Temp sensor bridge started (CC1352P7):\r\n";
    UART2_write(uartConsole, startMsg, sizeof(startMsg), NULL);

    while (1)
    {
        /* --- Transmit the query --- */
        rs485SetTxMode();

        status = UART2_write(uartSensor, sensorCommand, SENSOR_CMD_LEN, &bytesWritten);
        if (status != UART2_STATUS_SUCCESS)
        {
            /* UART2_write() failed */
            while (1) {}
        }
        GPIO_toggle(CONFIG_GPIO_GLED);

        /* UART2_write() in blocking mode doesn't return until the transfer
         * is complete, so (unlike the AVR code) there's no need to poll a
         * "transmit complete" flag here before flipping direction. */
        rs485SetRxMode();

        /* --- Receive the fixed-length response --- */
        status = UART2_read(uartSensor, response, SENSOR_RESP_LEN, &bytesRead);
        if (status != UART2_STATUS_SUCCESS || bytesRead != SENSOR_RESP_LEN)
        {
            /* Malformed/short response - skip this cycle */
            const char errMsg[] = "Sensor read error/timeout\r\n";
            UART2_write(uartConsole, errMsg, sizeof(errMsg), NULL);
            sleep(10);
            continue;
        }
        GPIO_toggle(CONFIG_GPIO_RLED);

        /* --- Parse pH (bytes 3..6, reversed) --- */
        pHBytes[0] = response[6];
        pHBytes[1] = response[5];
        pHBytes[2] = response[4];
        pHBytes[3] = response[3];
        memcpy(&value, pHBytes, sizeof(value));
        float_to_string(&value, phStr, 2);

        /* --- Parse Temp (bytes 11..14, reversed) --- */
        tempBytes[0] = response[14];
        tempBytes[1] = response[13];
        tempBytes[2] = response[12];
        tempBytes[3] = response[11];
        memcpy(&value, tempBytes, sizeof(value));
        float_to_string(&value, tempStr, 2);

        /* --- Print result out the console UART --- */
        int len = snprintf(printBuf, sizeof(printBuf),
                            "pH: %s   Temp: %s\r\n", phStr, tempStr);
        if (len > 0)
        {
            UART2_write(uartConsole, printBuf,
                        (len < (int)sizeof(printBuf)) ? (size_t)len : sizeof(printBuf) - 1,
                        NULL);
        }

        /* --- Same 10 s cadence as the AVR original --- */
        sleep(10);
    }
}

static void rs485SetTxMode(void)
{
    GPIO_write(CONFIG_GPIO_RS485_DIR, 1);
}

static void rs485SetRxMode(void)
{
    GPIO_write(CONFIG_GPIO_RS485_DIR, 0);
}

/* Unchanged from the AVR version - plain C, no MCU-specific bits */
static void float_to_string(float *value, char *buffer, int precision)
{
    float val = *value;
    int is_negative = val < 0;
    if (is_negative)
        val = -val;

    long int_part = (long)val;
    float frac_part = val - int_part;

    char *p = buffer;
    if (is_negative)
        *p++ = '-';

    char temp[16];
    int i = 0;
    if (int_part == 0)
        temp[i++] = '0';
    while (int_part > 0 && i < 15)
    {
        temp[i++] = '0' + (int_part % 10);
        int_part /= 10;
    }
    while (i > 0)
        *p++ = temp[--i];

    *p++ = '.';
    for (int d = 0; d < precision; d++)
    {
        frac_part *= 10;
        int digit = (int)frac_part;
        *p++ = '0' + digit;
        frac_part -= digit;
    }
    *p = '\0';
}