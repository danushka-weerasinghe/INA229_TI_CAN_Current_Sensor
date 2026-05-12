/*
 *  ======== mcu.c ========
 *  MCU hardware abstraction for MSPM0G3105
 *  SPI transfer and delay implementation for INA229 driver.
 *
 *  Software CS on PA12 using SysConfig generated defines:
 *    GPIO_GRP_0_PORT     = GPIOA
 *    GPIO_GRP_0_CS_PIN   = DL_GPIO_PIN_12
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "mcu.h"
#include "ti_msp_dl_config.h"

/* Clock = 32MHz */
#define CYCLES_PER_MS   (32000U)
#define CYCLES_PER_US   (32U)

/* SPI FIFO polling timeout */
#define SPI_TIMEOUT     (10000U)

/* -----------------------------------------------------------------------
 *  Software CS — PA12
 *  Using exact defines from generated ti_msp_dl_config.h:
 *    GPIO_GRP_0_PORT    = GPIOA
 *    GPIO_GRP_0_CS_PIN  = DL_GPIO_PIN_12
 * ----------------------------------------------------------------------- */
#define SPI_CS_PORT     GPIO_GRP_0_PORT       /* GPIOA          */
#define SPI_CS_PIN      GPIO_GRP_0_CS_PIN     /* DL_GPIO_PIN_12 */

/* CS active LOW — INA229 /CS pin */
#define CS_SELECT()     DL_GPIO_clearPins(SPI_CS_PORT, SPI_CS_PIN)  /* CS LOW  = selected   */
#define CS_DESELECT()   DL_GPIO_setPins(SPI_CS_PORT, SPI_CS_PIN)    /* CS HIGH = deselected */


/********* MCU SPECIFIC SPI CODE STARTS HERE **********/

/*
 *  ======== mcu_spiInit ========
 *  SPI0 already initialized by SYSCFG_DL_init().
 *  Ensure CS starts HIGH (deselected).
 */
void mcu_spiInit(uint8_t busId)
{
    (void)busId;
    CS_DESELECT(); /* INA229 deselected on startup */
}

/*
 *  ======== mcu_spiTransfer ========
 *  Full-duplex SPI byte transfer with software CS on PA12.
 *
 *  INA229 SPI frame format:
 *    Write: [addr<<2 | 0x00] [MSB] [LSB]          (3 bytes total)
 *    Read:  [addr<<2 | 0x01] [dummy]...[dummy]     (1 + N bytes)
 *
 *  CS pulled LOW before first byte, HIGH after last byte.
 */
uint8_t mcu_spiTransfer(uint8_t busId, uint8_t csGPIOId,
                        uint8_t count, uint8_t *txBuf, uint8_t *rxBuf)
{
    uint8_t i;
    uint32_t timeout;

    (void)busId;
    (void)csGPIOId;

    /* Assert CS — select INA229 */
    CS_SELECT();

    /* Setup delay: CS low to first SCLK (~1us) */
    delay_cycles(CYCLES_PER_US);

    for (i = 0; i < count; i++)
    {
        /* --- TRANSMIT --- */
        timeout = SPI_TIMEOUT;
        while (DL_SPI_isTXFIFOFull(SPI_INST))
        {
            if (--timeout == 0)
            {
                CS_DESELECT();
                return 1; /* TX timeout */
            }
        }
        DL_SPI_transmitData8(SPI_INST, txBuf[i]);

        /* --- RECEIVE --- */
        timeout = SPI_TIMEOUT;
        while (DL_SPI_isRXFIFOEmpty(SPI_INST))
        {
            if (--timeout == 0)
            {
                CS_DESELECT();
                return 1; /* RX timeout */
            }
        }
        rxBuf[i] = DL_SPI_receiveData8(SPI_INST);
    }

    /* Wait for SPI bus to go idle */
    timeout = SPI_TIMEOUT;
    while (DL_SPI_isBusy(SPI_INST))
    {
        if (--timeout == 0)
        {
            CS_DESELECT();
            return 1; /* Bus busy timeout */
        }
    }

    /* Hold delay: last SCLK to CS high (~1us) */
    delay_cycles(CYCLES_PER_US);

    /* Deassert CS — deselect INA229 */
    CS_DESELECT();

    return 0; /* Success */
}

/********* MCU SPECIFIC SPI CODE ENDS HERE **********/


/********* MCU SPECIFIC DELAY CODE STARTS HERE ************/

void mcu_msWait(uint16_t msWait)
{
    if (msWait == 0) return;
    delay_cycles((uint32_t)msWait * CYCLES_PER_MS);
}

void mcu_usWait(uint16_t usWait)
{
    if (usWait == 0) return;
    delay_cycles((uint32_t)usWait * CYCLES_PER_US);
}

/********* MCU SPECIFIC DELAY CODE ENDS HERE ************/
