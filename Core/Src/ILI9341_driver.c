/*
 * ILI9341_driver.c
 *
 *  Created on: Jul 18, 2025
 *      Author: Felipe
 */


#include "ILI9341_driver.h"
#include "fonts.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_spi.h"
#include "stm32g4xx_ll_utils.h"
#include "stm32g4xx_ll_dma.h"
#include "shared.h"
//#include <stdint.h>
//#include "stm32g4xx_ll_adc.h"


// ------------------------- 		MACROS			-------------------------

// Helper macros for control lines
#define CS_LOW()    LL_GPIO_ResetOutputPin(ILI9341_GPIO_PORT, ILI9341_CS_PIN)
#define CS_HIGH()   LL_GPIO_SetOutputPin(ILI9341_GPIO_PORT, ILI9341_CS_PIN)
#define DC_LOW()    LL_GPIO_ResetOutputPin(ILI9341_DC_PORT, ILI9341_DC_PIN)
#define DC_HIGH()   LL_GPIO_SetOutputPin(ILI9341_DC_PORT, ILI9341_DC_PIN)
#define RST_LOW()   LL_GPIO_ResetOutputPin(ILI9341_GPIO_PORT, ILI9341_RST_PIN)
#define RST_HIGH()  LL_GPIO_SetOutputPin(ILI9341_GPIO_PORT, ILI9341_RST_PIN)

#define LEDON			LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_0)
#define LEDOFF			LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_0)


uint8_t areabuff[CHARHEIGHT*DISPWIDTH*2];
uint8_t DMA_rdy = 0;
uint32_t tmp = 0;
extern uint32_t shared;

// SPI transmit helper
void ILI9341_SPI_Send(uint8_t data) {
    while(!LL_SPI_IsActiveFlag_TXE(SPI1));
    LL_SPI_TransmitData8(SPI1, data);
//    while(LL_SPI_IsActiveFlag_BSY(SPI1));
}

// Set address window
void ILI9341_SetAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    DC_LOW();
    CS_LOW();
    ILI9341_SPI_Send(ILI9341_CASET);
    DC_HIGH();
    ILI9341_SPI_Send(x0 >> 8);
    ILI9341_SPI_Send(x0 & 0xFF);
    ILI9341_SPI_Send(x1 >> 8);
    ILI9341_SPI_Send(x1 & 0xFF);
    DC_LOW();
    ILI9341_SPI_Send(ILI9341_PASET);
    DC_HIGH();
    ILI9341_SPI_Send(y0 >> 8);
    ILI9341_SPI_Send(y0 & 0xFF);
    ILI9341_SPI_Send(y1 >> 8);
    ILI9341_SPI_Send(y1 & 0xFF);
    DC_LOW();
    ILI9341_SPI_Send(ILI9341_RAMWR);
    CS_HIGH();
}


// Fill screen with colour
void ILI9341_FillScreen(uint16_t color) {
	ILI9341_SetAddress(0, 0, DISPWIDTH-1, DISPHEIGHT-1);
    DC_HIGH();
    CS_LOW();
    for(uint32_t i = 0; i < DISPWIDTH*DISPHEIGHT; i++) {
        ILI9341_SPI_Send(color >> 8);   // High byte
        ILI9341_SPI_Send(color & 0xFF); // Low byte
    }
    CS_HIGH();
}


void ILI9341_FillRect(uint16_t x,
                      uint16_t y,
                      uint16_t width,
                      uint16_t height,
                      uint16_t color)
{
    if ((width == 0) || (height == 0))
        return;

    if ((x >= DISPWIDTH) || (y >= DISPHEIGHT))
        return;

    if ((x + width) > DISPWIDTH)
        width = DISPWIDTH - x;

    if ((y + height) > DISPHEIGHT)
        height = DISPHEIGHT - y;

    ILI9341_SetAddress(x,
                       y,
                       x + width - 1,
                       y + height - 1);

    DC_HIGH();
    CS_LOW();

    uint32_t pixels = (uint32_t)width * height;

    for (uint32_t i = 0; i < pixels; i++)
    {
        ILI9341_SPI_Send(color >> 8);
        ILI9341_SPI_Send(color & 0xFF);
    }

    CS_HIGH();
}

void ILI9341_DrawRect(uint16_t x,
                      uint16_t y,
                      uint16_t width,
                      uint16_t height,
                      uint16_t thickness,
                      uint16_t color)
{
    uint16_t px;

    if ((width == 0) || (height == 0) || (thickness == 0))
        return;

    if ((x + width) > DISPWIDTH)
        return;

    if ((y + height) > DISPHEIGHT)
        return;

    ILI9341_SetAddress(
        x,
        y,
        x + width - 1,
        y + height - 1
    );

    DC_HIGH();
    CS_LOW();

    for (uint16_t row = 0; row < height; row++)
    {
        for (uint16_t col = 0; col < width; col++)
        {
            if ((row < thickness) ||
                (row >= (height - thickness)) ||
                (col < thickness) ||
                (col >= (width - thickness)))
            {
                px = color;
            }
            else
            {
                px = BLACK;
            }

            ILI9341_SPI_Send(px >> 8);
            ILI9341_SPI_Send(px & 0xFF);
        }
    }

    while (LL_SPI_IsActiveFlag_BSY(SPI1));

    CS_HIGH();
}

// Display initialisation
void ILI9341_Init(void) {
	// Set CS, DC, RST high initially
	CS_HIGH();
	DC_HIGH();
	RST_HIGH();
	LL_mDelay(100);

    // Reset sequence
    RST_LOW();
    LL_mDelay(20);
    RST_HIGH();
    LL_mDelay(120);

    CS_LOW();
    DC_LOW();
    LL_SPI_Enable(SPI1);
    ILI9341_SPI_Send(ILI9341_SWRESET);
    LL_mDelay(120);
    ILI9341_SPI_Send(ILI9341_SLPOUT);
    LL_mDelay(120);
    ILI9341_SPI_Send(ILI9341_COLMOD);
    DC_HIGH();
    ILI9341_SPI_Send(0x55); // 16-bit colour
    DC_LOW();
    ILI9341_SPI_Send(ILI9341_MADCTL);
    DC_HIGH();
    ILI9341_SPI_Send(0xE8); // RGB, landscape
    //ILI9341_SPI_Send(0x88); // RGB, portrait
    DC_LOW();
    ILI9341_SPI_Send(ILI9341_DISPON);
    LL_mDelay(120);
    CS_HIGH();
}

void uDMA1_Channel2_IRQHandler(void) {
	if(LL_DMA_IsActiveFlag_TC2(DMA1)) {
		LL_DMA_ClearFlag_TC2(DMA1);        // Clear the TC flag
	    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2); // (Optional) Disable Channel
		LL_SPI_DisableDMAReq_TX(SPI1);
		CS_HIGH();
	}
}

void spi1_dma_transmit(uint8_t *data, uint16_t size)
{
//    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
//    LL_DMA_ClearFlag_TC2(DMA1);
//    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_2, LL_DMAMUX_REQ_SPI1_TX);
    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)&(SPI1->DR));

    // Set memory address and data length
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)data);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, size);

//    LL_SPI_Enable(SPI1);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_2);
    LL_SPI_EnableDMAReq_TX(SPI1);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
}

void eraseTrace (uint16_t x, uint16_t y, uint16_t xf, uint16_t colour) {
    while ((DMA1_Channel2->CCR & DMA_CCR_EN));
	ILI9341_SetAddress(x, y, xf, y);
    DC_HIGH();
    CS_LOW();
    for (uint8_t i = x; i < xf; i++) {
    	areabuff[2 * i] = (colour >> 8);
    	areabuff[2* i + 1] = (colour & 0xFF);
    }
    spi1_dma_transmit(areabuff, (xf - x) * 2);
}

void drawTrace(uint16_t x, uint16_t y, uint16_t colour) {
    while ((DMA1_Channel2->CCR & DMA_CCR_EN));
	ILI9341_SetAddress(x, y, x + CHARWIDTH - 1, y);
    DC_HIGH();
    CS_LOW();
    for (uint8_t i = 0; i < CHARWIDTH * 2; i+=2) {
    	areabuff[i] = (colour >> 8);
    	areabuff[i + 1] = (colour & 0xFF);
    }
    spi1_dma_transmit(areabuff, CHARWIDTH * 2);
}

// Draws area of dwidth * dheight pixels with the contents of areabuff[] starting at x, y
void drawD(uint16_t x, uint16_t y, uint16_t dwidth, uint16_t dheight) {
	ILI9341_SetAddress(x, y, x + dwidth - 1, y + dheight - 1);
    DC_HIGH();
    CS_LOW();
    spi1_dma_transmit(areabuff, dwidth * dheight * 2);
}


void ILI9341_DrawStringDot(uint16_t x, uint16_t y, const char* str, uint8_t dec_sep, uint16_t color, uint16_t bg) {
	char str_tmp[DISPWIDTH/CHARWIDTH];
	uint16_t len = 0;
	uint8_t j = 0;
	while (str[len] != '\0') {
		len++;
	}
	if (dec_sep == 0) {
		j++;
	}
	for (uint16_t i = 0; i < len + 1; i++) {
		if (i == dec_sep + 1 && j == 0) {
			str_tmp[len - i + 1] = '.';
			j++;
		}
		str_tmp[len - i - j + 1] = str[len - i];
	}
	ILI9341_DrawStringD(x, y, str_tmp, color, bg);
}


// Creates line buffer, x and y only used to pass to the drawing function
void ILI9341_DrawStringD(uint16_t x, uint16_t y, const char* str, uint16_t colour, uint16_t bg) {
	uint8_t len = 0, ccol = 0, cntchar;
	const uint16_t* bitmap;
	uint16_t bits, totcols;
	if (str != NULL) {
		while (str[len] != '\0') {
			len++;
		}
	}
	if (len == 0) {
		return; // returns in case len == 0 (str == NULL)
	}
	totcols = CHARWIDTH * len;
    for(uint8_t row = 0; row < CHARHEIGHT; row++) {
        cntchar = 0;
    	for(uint8_t col = 0; col < totcols; col++) {
    		bitmap = font11x23[*(str + cntchar)];
            bits = bitmap[row];
            if(bits & (1<<(15-ccol))) {
            	areabuff[2 * (row * totcols + col)] = (colour >> 8);
            	areabuff[2 * (row * totcols + col) + 1] = (colour & 0xFF);
        	} else {
        		areabuff[2 * (row * totcols + col)] = (bg >> 8);
        		areabuff[2 * (row * totcols + col) + 1] = (bg & 0xFF);
        	}
        	if (ccol++ == CHARWIDTH-1) {
        		ccol = 0;
        		cntchar++;
        	}
        }
    }
if (loopcnt == 2){
   	LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_0);	// LED ON
}
    while ((DMA1_Channel2->CCR & DMA_CCR_EN));
    drawD(x, y, len * CHARWIDTH, CHARHEIGHT);
}


// Creates line buffer, x and y only used to pass to the drawing function
void ILI9341_DrawStringNoDMA(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg) {
	uint8_t len = 0, ccol = 0, cntchar;
	const uint16_t* bitmap;
	uint16_t bits, totcols, i;
	if (str != NULL) {
		while (str[len] != '\0') {
			len++;
		}
	}
	totcols = CHARWIDTH * len;
    for(uint8_t row = 0; row < CHARHEIGHT; row++) {
        cntchar = 0;
    	for(uint8_t col = 0; col < totcols; col++) {
    		bitmap = font11x23[*(str + cntchar)];
            bits = bitmap[row];
            if(bits & (1<<(15-ccol))) {
            	areabuff[2 * (row * totcols + col)] = (color >> 8);
            	areabuff[2 * (row * totcols + col) + 1] = (color & 0xFF);
        	} else {
        		areabuff[2 * (row * totcols + col)] = (bg >> 8);
        		areabuff[2 * (row * totcols + col) + 1] = (bg & 0xFF);
        	}
        	if (ccol++ == CHARWIDTH-1) {
        		ccol = 0;
        		cntchar++;
        	}
        }
    }
    ILI9341_SetAddress(x, y, x + len * CHARWIDTH - 1, y + CHARHEIGHT - 1);
    DC_HIGH();
    CS_LOW();
    for(i = 0; i < len * CHARWIDTH * CHARHEIGHT * 2; i++) {
      	ILI9341_SPI_Send(areabuff[i]);
    }
    CS_HIGH();
}

void ILI9341_DrawStringScale(uint16_t x,
                            uint16_t y,
                            const char *str,
                            uint8_t scale,
                            uint16_t color,
                            uint16_t bg)
{
    uint16_t cursor_x;

    if ((str == NULL) || (scale == 0))
        return;

    cursor_x = x;

    while (*str != '\0')
    {
        const uint16_t *bitmap;
        uint16_t char_width;
        uint16_t char_height;

        bitmap = font11x23[(uint8_t)*str];

        char_width  = CHARWIDTH  * scale;
        char_height = CHARHEIGHT * scale;

        ILI9341_SetAddress(
            cursor_x,
            y,
            cursor_x + char_width - 1,
            y + char_height - 1
        );

        DC_HIGH();
        CS_LOW();

        for (uint8_t row = 0; row < CHARHEIGHT; row++)
        {
            uint16_t bits = bitmap[row];

            /*
             * Repeat each original font row vertically.
             */
            for (uint8_t sy = 0; sy < scale; sy++)
            {
                for (uint8_t col = 0; col < CHARWIDTH; col++)
                {
                    uint16_t px;

                    if (bits & (1U << (15 - col)))
                        px = color;
                    else
                        px = bg;

                    /*
                     * Repeat each original pixel horizontally.
                     */
                    for (uint8_t sx = 0; sx < scale; sx++)
                    {
                        ILI9341_SPI_Send(px >> 8);
                        ILI9341_SPI_Send(px & 0xFF);
                    }
                }
            }
        }

        while (LL_SPI_IsActiveFlag_BSY(SPI1));

        CS_HIGH();

        cursor_x += char_width;
        str++;
    }
}

void ILI9341_DrawUpArrow(uint16_t cx,
                         uint16_t top,
                         uint16_t color)
{
    const uint16_t width  = 35;
    const uint16_t height = 18;
    uint16_t left = cx - (width / 2);

    ILI9341_SetAddress(
        left,
        top,
        left + width - 1,
        top + height - 1
    );

    DC_HIGH();
    CS_LOW();

    for (uint16_t row = 0; row < height; row++)
    {
        uint16_t half_width = row;

        for (uint16_t col = 0; col < width; col++)
        {
            int16_t dx =
                (int16_t)col - (int16_t)(width / 2);

            uint16_t px;

            if ((dx >= -(int16_t)half_width) &&
                (dx <=  (int16_t)half_width))
            {
                px = color;
            }
            else
            {
                px = BLACK;
            }

            ILI9341_SPI_Send(px >> 8);
            ILI9341_SPI_Send(px & 0xFF);
        }
    }

    while (LL_SPI_IsActiveFlag_BSY(SPI1));

    CS_HIGH();
}


void ILI9341_DrawDownArrow(uint16_t cx,
                           uint16_t top,
                           uint16_t color)
{
    const uint16_t width  = 35;
    const uint16_t height = 18;
    uint16_t left = cx - (width / 2);

    ILI9341_SetAddress(
        left,
        top,
        left + width - 1,
        top + height - 1
    );

    DC_HIGH();
    CS_LOW();

    for (uint16_t row = 0; row < height; row++)
    {
        uint16_t half_width =
            (height - 1) - row;

        for (uint16_t col = 0; col < width; col++)
        {
            int16_t dx =
                (int16_t)col - (int16_t)(width / 2);

            uint16_t px;

            if ((dx >= -(int16_t)half_width) &&
                (dx <=  (int16_t)half_width))
            {
                px = color;
            }
            else
            {
                px = BLACK;
            }

            ILI9341_SPI_Send(px >> 8);
            ILI9341_SPI_Send(px & 0xFF);
        }
    }

    while (LL_SPI_IsActiveFlag_BSY(SPI1));

    CS_HIGH();
}




