/* Standard C Libraries */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* For usleep */


/* TI Drivers */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/rf/RF.h>


/* Driverlib Header files */
#include DeviceFamily_constructPath(driverlib / rf_prop_mailbox.h)

/* Board Header files */
#include "ti_drivers_config.h"

/* Application Header files */
#include "RFQueue.h"
#include <ti_radio_config.h>

/***** Defines *****/
#define MAX_LENGTH 64
#define NUM_APPENDED_BYTES 2
#define MODBUS_RX_LEN 17
#define MODBUS_TX_LEN 8

/******* Global variable declarations *********/
static RF_Object rfObject;
static RF_Handle rfHandle;

/* Single UART setup for Debugging */
UART2_Handle uartConsole; // UART1 for Debug Console

/* RF Tx Packet Buffer */
static uint8_t packet[MAX_LENGTH + NUM_APPENDED_BYTES - 1];

/***** Helper Functions *****/

/* Modbus CRC-16 Calculation */
uint16_t modbusCRC16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc >>= 1;
    }
  }
  return crc;
}

/* Float conversion (Big-Endian wire format to Little-Endian ARM) */
float bytesToFloat(const uint8_t *b) {
  uint8_t swapped[4] = {b[3], b[2], b[1], b[0]};
  float val;
  memcpy(&val, swapped, sizeof(val));
  return val;
}

/***** Main Thread *****/
void *mainThread(void *arg0) {
  RF_Params rfParams;
  RF_Params_init(&rfParams);

  UART2_Params uartConsoleParams;

  /* Initialize LEDs */
  GPIO_setConfig(CONFIG_GPIO_RLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
  GPIO_write(CONFIG_GPIO_RLED, CONFIG_GPIO_LED_OFF);
  GPIO_setConfig(CONFIG_GPIO_GLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
  GPIO_write(CONFIG_GPIO_GLED, CONFIG_GPIO_LED_OFF);

  /* ==========================================================
   *  UART CONFIGURATION (Console Only)
   * ========================================================== */
  UART2_Params_init(&uartConsoleParams);
  uartConsoleParams.baudRate = 115200;
  uartConsoleParams.readMode = UART2_Mode_BLOCKING;
  uartConsoleParams.writeMode = UART2_Mode_BLOCKING;
  uartConsole = UART2_open(CONFIG_UART_CONSOLE, &uartConsoleParams);

  const char startMsg[] =
      "\r\n[SYS] ARM CC1352 Debug Mode Started (Simulated Sensor)\r\n";
  UART2_write(uartConsole, startMsg, sizeof(startMsg) - 1, NULL);

  /* ==========================================================
   *  RF CONFIGURATION
   * ========================================================== */
  RF_cmdPropTx.pPkt = packet;
  RF_cmdPropTx.startTrigger.triggerType = TRIG_NOW;

  rfHandle = RF_open(&rfObject, &RF_prop,
                     (RF_RadioSetup *)&RF_cmdPropRadioDivSetup, &rfParams);
  RF_postCmd(rfHandle, (RF_Op *)&RF_cmdFs, RF_PriorityNormal, NULL, 0);

  /* ==========================================================
   *  MAIN SIMULATION LOOP
   * ========================================================== */
  while (1) {
    char debugBuf[128];

    /* 1. Simulate the Modbus Request text output */
    const char reqMsg[] = "[SIM] Sending Modbus Request\r\n";
    UART2_write(uartConsole, reqMsg, sizeof(reqMsg) - 1, NULL);

    /* 2. Construct a simulated 17-byte response
     * Frame: [addr][func][byteCount][pH][4 unused][Temp][crc_lo][crc_hi]
     * Using IEEE 754 Big-Endian hex representations:
     * pH 7.25 = 0x40 0xE8 0x00 0x00
     * Temp 24.5 = 0x41 0xC4 0x00 0x00
     */
    uint8_t simRxBuffer[MODBUS_RX_LEN] = {
        0x03, 0x03, 0x08,       // Header
        0x40, 0xE8, 0x00, 0x00, // pH = 7.25
        0x00, 0x00, 0x00, 0x00, // Unused 4 bytes
        0x41, 0xC4, 0x00, 0x00, // Temp = 24.5
        0x00, 0x00              // CRC placeholders
    };

    /* Calculate and append the valid CRC-16 so the check passes */
    uint16_t calcCRC = modbusCRC16(simRxBuffer, MODBUS_RX_LEN - 2);
    simRxBuffer[MODBUS_RX_LEN - 2] = calcCRC & 0xFF;
    simRxBuffer[MODBUS_RX_LEN - 1] = (calcCRC >> 8) & 0xFF;

    /* 3. Process the simulated data */
    uint16_t rxCRC =
        simRxBuffer[MODBUS_RX_LEN - 2] | (simRxBuffer[MODBUS_RX_LEN - 1] << 8);

    if (rxCRC == calcCRC) {
      /* Extract floats (pH is at index 3, Temp is at index 11) */
      float ph = bytesToFloat(&simRxBuffer[3]);
      float temp = bytesToFloat(&simRxBuffer[11]);

      /* Output to Console */
      snprintf(debugBuf, sizeof(debugBuf),
               "[SIM] Success -> pH: %.2f | Temp: %.1f C\r\n", ph, temp);
      UART2_write(uartConsole, debugBuf, strlen(debugBuf) - 1, NULL);

      /* Pack data for RF Transmission */
      RF_cmdPropTx.pktLen = MODBUS_RX_LEN;
      memcpy(packet, simRxBuffer, MODBUS_RX_LEN);

      /* Send packet via Proprietary RF */
      RF_runCmd(rfHandle, (RF_Op *)&RF_cmdPropTx, RF_PriorityNormal, NULL, 0);

      /* Toggle green led to indicate Tx Success */
      GPIO_toggle(CONFIG_GPIO_GLED);
    } else {
      /* This block should never be hit in simulation, but kept for structural
       * parity */
      const char errMsg[] = "[ERR] CRC Validation Failed\r\n";
      UART2_write(uartConsole, errMsg, sizeof(errMsg) - 1, NULL);
    }

    /* 4. Wait 3 seconds before the next simulated cycle */
    sleep(3);
  }
}