#include "touch.h"
#include "main.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_spi.h"

#define XPT2046_CMD_X   0xD0
#define XPT2046_CMD_Y   0x90
#define XPT2046_CMD_Z1  0xB0
#define XPT2046_CMD_Z2  0xC0

/* --------------------------------------------------------------------------
 * SPI transfer with timeout
 * -------------------------------------------------------------------------- */
static bool Touch_SPI_Transfer(uint8_t tx, uint8_t *rx)
{
    uint32_t timeout;

    if (rx == NULL)
        return false;

    /* Wait for transmit buffer to be empty */
    timeout = 100000;

    while (!LL_SPI_IsActiveFlag_TXE(SPI1))
    {
        if (--timeout == 0)
            return false;
    }

    /* Send byte */
    LL_SPI_TransmitData8(SPI1, tx);

    /* Wait for received byte */
    timeout = 100000;

    while (!LL_SPI_IsActiveFlag_RXNE(SPI1))
    {
        if (--timeout == 0)
            return false;
    }

    /* Read received byte */
    *rx = LL_SPI_ReceiveData8(SPI1);

    return true;
}


/* --------------------------------------------------------------------------
 * Prepare SPI1 for touch controller
 *
 * LCD uses DIV4.
 * XPT2046 is slower, so use DIV64.
 * -------------------------------------------------------------------------- */
static void Touch_SPI_Begin(void)
{
    /* Make sure LCD is deselected */
    LL_GPIO_SetOutputPin(DISP_CS_GPIO_Port, DISP_CS_Pin);

    /* Disable SPI before changing prescaler */
    LL_SPI_Disable(SPI1);

    /* Touch SPI speed */
    LL_SPI_SetBaudRatePrescaler(
        SPI1,
        LL_SPI_BAUDRATEPRESCALER_DIV64
    );

    /*
     * Re-enable SPI.
     */
    LL_SPI_Enable(SPI1);

    /*
     * Drain any stale received byte.
     */
    while (LL_SPI_IsActiveFlag_RXNE(SPI1))
    {
        (void)LL_SPI_ReceiveData8(SPI1);
    }

    /*
     * Select XPT2046.
     */
    LL_GPIO_ResetOutputPin(TCH_CS_GPIO_Port, TCH_CS_Pin);
}


/* --------------------------------------------------------------------------
 * Restore SPI1 for LCD
 * -------------------------------------------------------------------------- */
static void Touch_SPI_End(void)
{
    /* Deselect touch */
    LL_GPIO_SetOutputPin(TCH_CS_GPIO_Port, TCH_CS_Pin);

    /*
     * Return SPI1 to the LCD speed.
     *
     * IMPORTANT:
     * Your MX_SPI1_Init() currently uses DIV4.
     */
    LL_SPI_Disable(SPI1);

    LL_SPI_SetBaudRatePrescaler(
        SPI1,
        LL_SPI_BAUDRATEPRESCALER_DIV4
    );

    LL_SPI_Enable(SPI1);
}


/* --------------------------------------------------------------------------
 * Read one 12-bit XPT2046 value
 * -------------------------------------------------------------------------- */
static bool Touch_Read12(uint8_t command, uint16_t *value)
{
    uint8_t rx;
    uint8_t high;
    uint8_t low;

    if (value == NULL)
        return false;

    /*
     * Send command.
     */
    if (!Touch_SPI_Transfer(command, &rx))
        return false;

    /*
     * First data byte.
     */
    if (!Touch_SPI_Transfer(0x00, &high))
        return false;

    /*
     * Second data byte.
     */
    if (!Touch_SPI_Transfer(0x00, &low))
        return false;

    /*
     * XPT2046 returns a 12-bit ADC value packed
     * into the upper 12 bits of the 16-bit transfer.
     */
    *value = ((uint16_t)high << 8 | low) >> 4;

    return true;
}


/* --------------------------------------------------------------------------
 * Read averaged value
 * -------------------------------------------------------------------------- */
static bool Touch_ReadAverage(uint8_t command, uint16_t *value)
{
    uint32_t sum = 0;
    uint16_t sample;

    if (value == NULL)
        return false;

    for (uint8_t i = 0; i < 5; i++)
    {
        if (!Touch_Read12(command, &sample))
        {
            return false;
        }

        sum += sample;
    }

    *value = (uint16_t)(sum / 5);

    return true;
}


/* --------------------------------------------------------------------------
 * Read raw X/Y/Z values
 * -------------------------------------------------------------------------- */
void Touch_GetRaw(uint16_t *x,
                  uint16_t *y,
                  uint16_t *z1,
                  uint16_t *z2)
{
    bool ok = true;

    if ((x == NULL) ||
        (y == NULL) ||
        (z1 == NULL) ||
        (z2 == NULL))
    {
        return;
    }

    /*
     * Default values in case SPI communication fails.
     */
    *x  = 0;
    *y  = 0;
    *z1 = 0;
    *z2 = 0;

    /*
     * Switch SPI to touch speed and select XPT2046.
     */
    Touch_SPI_Begin();

    /*
     * Read pressure first.
     */
    if (!Touch_Read12(XPT2046_CMD_Z1, z1))
        ok = false;

    if (ok)
    {
        if (!Touch_Read12(XPT2046_CMD_Z2, z2))
            ok = false;
    }

    /*
     * Read X and Y.
     */
    if (ok)
    {
        if (!Touch_ReadAverage(XPT2046_CMD_X, x))
            ok = false;
    }

    if (ok)
    {
        if (!Touch_ReadAverage(XPT2046_CMD_Y, y))
            ok = false;
    }

    /*
     * Restore LCD SPI speed.
     */
    Touch_SPI_End();

    /*
     * If communication failed, return zeroes.
     */
    if (!ok)
    {
        *x  = 0;
        *y  = 0;
        *z1 = 0;
        *z2 = 0;
    }
}


/* --------------------------------------------------------------------------
 * Get calibrated touch point
 * -------------------------------------------------------------------------- */
bool Touch_GetPoint(uint16_t *x, uint16_t *y)
{
    uint16_t raw_x;
    uint16_t raw_y;
    uint16_t z1;
    uint16_t z2;

    if ((x == NULL) || (y == NULL))
        return false;

    Touch_GetRaw(&raw_x, &raw_y, &z1, &z2);

    /*
     * No touch / invalid reading.
     */
    if (z1 < 50)
        return false;

    /*
     * Make sure the raw values are inside our
     * calibrated touchscreen area.
     */
    if ((raw_x < TOUCH_X_MIN) ||
        (raw_x > TOUCH_X_MAX) ||
        (raw_y < TOUCH_Y_MIN) ||
        (raw_y > TOUCH_Y_MAX))
    {
        return false;
    }

    /*
     * X:
     * raw 460  -> screen 0
     * raw 1730 -> screen 239
     */
    *x = (uint16_t)(
        ((uint32_t)(raw_x - TOUCH_X_MIN) * 239) /
        (TOUCH_X_MAX - TOUCH_X_MIN)
    );

    /*
     * Y is inverted:
     *
     * raw 1680 -> screen 0
     * raw 265  -> screen 319
     */
    *y = (uint16_t)(
        ((uint32_t)(TOUCH_Y_MAX - raw_y) * 319) /
        (TOUCH_Y_MAX - TOUCH_Y_MIN)
    );

    return true;
}
