/*
 *  Include Generic Header Files Here
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "mcu.h"

/*
 *  Include MCU Specific Header Files Here
 */


/********* MCU SPECIFIC SPI CODE STARTS HERE **********/
void mcu_spiInit(uint8_t busId)
{
    /* Add MCU specific init necessary for I2C to be used */
}

uint8_t mcu_spiTransfer(uint8_t busId, uint8_t csGPIOId, uint8_t count, uint8_t* txBuf, uint8_t* rxBuf)
{
    /*
     *  Add MCU specific SPI read/write code here.
     */

    /*
     *  Add MCU specific return code for error handling
     */

    return (0);
}


/********* MCU SPECIFIC SPI CODE ENDS HERE **********/


/********* MCU SPECIFIC DELAY CODE STARTS HERE ************/
void mcu_msWait(uint16_t msWait)
{
    /*
     *  Add MCU specific wait loop for msWait. The unit is in milli-seconds
     */
}

void mcu_usWait(uint16_t usWait)
{
    /*
     *  Add MCU specific wait loop for usWait. The unit is in micro-seconds
     */
}
/********* MCU SPECIFIC DELAY CODE ENDS HERE ************/
