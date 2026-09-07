/*
 * ======== INA229_TI.c ========
 * INA229 Power Monitor — MSPM0G3105
 *
 * Shunt: 300A/75mV rating, measuring up to 150A
 *   R_shunt     = 75mV / 300A  = 0.00025 Ω (0.25 mΩ)
 *   CURRENT_LSB = 150A / 2^19  = 286.1 nA/LSB
 *   SHUNT_CAL   = 13107.2e6 × 286.1e-9 × 0.00025 = 938 (0x03AA)
 */

//test

#include "ti_msp_dl_config.h"
#include "INA229.h"
#include "mcu.h"
#include "config.h"

/* -----------------------------------------------------------------------
 *  Shunt Calibration for 300A/75mV shunt, 150A max
 * ----------------------------------------------------------------------- */
#define SHUNT_R_OHMS        (0.00025f)          /* 75mV / 300A = 0.25mΩ  */
#define MAX_CURRENT_A       (150.0f)            /* Maximum expected current */
#define CURRENT_LSB_A       (MAX_CURRENT_A / 524288.0f)  /* 286.1 nA/LSB */

/* Expected INA229 IDs */
#define INA229_MFG_ID_EXPECTED  (0x5449U)
#define INA229_DEV_ID_EXPECTED  (0x2290U)
#define INA229_DEV_ID_MASK      (0xFFF0U)

/* -----------------------------------------------------------------------
 *  Debug variables — keep in watch window during development
 * ----------------------------------------------------------------------- */
volatile uint8_t  gDebug_TXTimeout  = 0;
volatile uint8_t  gDebug_RXTimeout  = 0;
volatile uint8_t  gDebug_SPIBusy    = 0;
volatile uint8_t  gDebug_RxByte0    = 0;
volatile uint8_t  gDebug_RxByte1    = 0;
volatile uint8_t  gDebug_RxByte2    = 0;
volatile uint16_t gDebug_RawMfgID   = 0;
volatile uint16_t gDebug_RawDevID   = 0;

/* SPI status */
volatile uint8_t  gSPI_OK           = 0;
volatile uint8_t  gSPI_Error        = 0;

/* -----------------------------------------------------------------------
 *  Live Measurements — Add these to CCS Watch Window
 * ----------------------------------------------------------------------- */
volatile float    gVBUS_V           = 0.0f;  /* Bus voltage       (V)  */
volatile float    gVSHUNT_mV       = 0.0f;  /* Shunt voltage     (mV) */
volatile float    gCurrent_A       = 0.0f;  /* Current           (A)  */
volatile float    gPower_W         = 0.0f;  /* Power             (W)  */
volatile float    gDieTemp_C       = 0.0f;  /* Die temperature   (°C) */

/* Derived measurements */
volatile float    gPower_calc_W    = 0.0f;  /* V × I calculated  (W)  */


/* -----------------------------------------------------------------------
 *  rawSPITransfer() — Direct SPI for ID verification
 * ----------------------------------------------------------------------- */
static uint8_t rawSPITransfer(uint8_t count, uint8_t *txBuf, uint8_t *rxBuf)
{
    uint8_t i;
    uint32_t timeout;

    DL_GPIO_clearPins(GPIO_GRP_0_PORT, GPIO_GRP_0_CS_PIN); /* CS LOW */
    delay_cycles(32U);

    for (i = 0; i < count; i++)
    {
        timeout = 10000U;
        while (DL_SPI_isTXFIFOFull(SPI_INST))
        {
            if (--timeout == 0)
            {
                gDebug_TXTimeout = 1;
                DL_GPIO_setPins(GPIO_GRP_0_PORT, GPIO_GRP_0_CS_PIN);
                return 1;
            }
        }
        DL_SPI_transmitData8(SPI_INST, txBuf[i]);

        timeout = 10000U;
        while (DL_SPI_isRXFIFOEmpty(SPI_INST))
        {
            if (--timeout == 0)
            {
                gDebug_RXTimeout = 1;
                DL_GPIO_setPins(GPIO_GRP_0_PORT, GPIO_GRP_0_CS_PIN);
                return 1;
            }
        }
        rxBuf[i] = DL_SPI_receiveData8(SPI_INST);
    }

    timeout = 10000U;
    while (DL_SPI_isBusy(SPI_INST))
    {
        if (--timeout == 0)
        {
            gDebug_SPIBusy = 1;
            DL_GPIO_setPins(GPIO_GRP_0_PORT, GPIO_GRP_0_CS_PIN);
            return 1;
        }
    }

    delay_cycles(32U);
    DL_GPIO_setPins(GPIO_GRP_0_PORT, GPIO_GRP_0_CS_PIN); /* CS HIGH */
    return 0;
}

static uint16_t debugReadReg(uint8_t regAddr)
{
    uint8_t txBuf[3] = { (uint8_t)((regAddr << 2) | 0x01), 0x00, 0x00 };
    uint8_t rxBuf[3] = { 0x00, 0x00, 0x00 };

    rawSPITransfer(3, txBuf, rxBuf);

    gDebug_RxByte0 = rxBuf[0];
    gDebug_RxByte1 = rxBuf[1];
    gDebug_RxByte2 = rxBuf[2];

    return (uint16_t)((rxBuf[1] << 8) | rxBuf[2]);
}


/* -----------------------------------------------------------------------
 *  main()
 * ----------------------------------------------------------------------- */
int main(void)
{
    /* Step 1: Initialize MCU peripherals */
    SYSCFG_DL_init();

    /* Step 2: Power-on delay */
    mcu_msWait(10);

    /* Step 3: Verify SPI communication */
    gDebug_RawMfgID = debugReadReg(INA229_manufacturer_id_register);
    mcu_msWait(1);
    gDebug_RawDevID = debugReadReg(INA229_device_id_register);
    mcu_msWait(1);

    if ((gDebug_RawMfgID == INA229_MFG_ID_EXPECTED) &&
        ((gDebug_RawDevID & INA229_DEV_ID_MASK) ==
         (INA229_DEV_ID_EXPECTED & INA229_DEV_ID_MASK)))
    {
        gSPI_OK = 1; /* ✅ SPI OK */

        /* Step 4: Set CURRENT_LSB and configure INA229 */
        INA229_setCURRENT_LSB(INA229_0, CURRENT_LSB_A);
        INA229_config(INA229_0);

        /* Step 5: Wait for first ADC conversion */
        mcu_msWait(10);

        /* Step 6: Measurement loop */
        while (1)
        {
            /* Read all measurements */
            gVBUS_V    = INA229_getVBUS_V(INA229_0);
            gVSHUNT_mV = INA229_getVSHUNT_mV(INA229_0);
            gCurrent_A = INA229_getCURRENT_A(INA229_0);
            gPower_W   = INA229_getPOWER_W(INA229_0);
            gDieTemp_C = INA229_getDIETEMP_C(INA229_0);

            /* Cross-check: calculated power = V × I */
            gPower_calc_W = gVBUS_V * gCurrent_A;

            /* Over-current protection check (150A max) */
            if (gCurrent_A > MAX_CURRENT_A)
            {
                /* Add your alert handling here */
            }

            mcu_msWait(1000); /* Read every 1 second */
        }
    }
    else
    {
        gSPI_Error = 1; /* ❌ SPI failed */
        while (1)
        {
            mcu_msWait(500);
        }
    }
}
