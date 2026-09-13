/*
 * ILI9341_driver.h
 *
 *  Created on: Jul 18, 2025
 *      Author: Felipe
 */

#ifndef INC_ILI9341_DRIVER_H_
#define INC_ILI9341_DRIVER_H_


//#include "ILI9341_driver.h"
#include <stdint.h>
//#include "fonts.h"
//#include "stm32g4xx_ll_gpio.h"
//#include "stm32g4xx_ll_adc.h"
//#include "stm32g4xx_ll_spi.h"
//#include "stm32g4xx_ll_utils.h"

#define	DISPWIDTH			320
#define	DISPHEIGHT			240

#define ILI9341_CS_PIN    LL_GPIO_PIN_4
#define ILI9341_RST_PIN   LL_GPIO_PIN_10
#define ILI9341_DC_PIN    LL_GPIO_PIN_0
#define ILI9341_GPIO_PORT GPIOA
#define ILI9341_DC_PORT   GPIOB

// ILI9341 Commands
#define ILI9341_SWRESET   0x01
#define ILI9341_SLPOUT    0x11
#define ILI9341_CLRINV    0x21
#define ILI9341_DISPON    0x29
#define ILI9341_CASET     0x2A
#define ILI9341_PASET     0x2B
#define ILI9341_RAMWR     0x2C
#define ILI9341_MADCTL    0x36
#define ILI9341_COLMOD    0x3A
#define ILI9341_DINVCTRL  0xB4

// Definition of basic colours
#define WHITE				0xFFFF
#define BLACK				0x0000
#define RED					0xF800
#define GREEN				0x07E0
#define BLUE				0x001F
#define YELLOW				0xFFE0
#define MAGENTA				0xF81F
#define TURQUOISE			0x07FF

void ILI9341_FillRect(uint16_t x,
                      uint16_t y,
                      uint16_t width,
                      uint16_t height,
                      uint16_t color);

void ILI9341_DrawRect(uint16_t x,
                      uint16_t y,
                      uint16_t width,
                      uint16_t height,
                      uint16_t thickness,
                      uint16_t color);

void ILI9341_DrawStringScale(uint16_t x,
                            uint16_t y,
                            const char *str,
                            uint8_t scale,
                            uint16_t color,
                            uint16_t bg);

void ILI9341_DrawUpArrow(uint16_t cx,
                         uint16_t top,
                         uint16_t color);

void ILI9341_DrawDownArrow(uint16_t cx,
                           uint16_t top,
                           uint16_t color);

void ILI9341_FillScreen(uint16_t color);
void ILI9341_Init(void);
void drawTrace(uint16_t x, uint16_t y, uint16_t colour);
void eraseTrace (uint16_t x, uint16_t y, uint16_t xf, uint16_t colour);
void ILI9341_DrawStringDot(uint16_t x, uint16_t y, const char* str, uint8_t dec_sep, uint16_t color, uint16_t bg);
void ILI9341_DrawStringNoDMA(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg);
void ILI9341_DrawStringD(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg);
//void ILI9341_DrawChar(uint16_t x, uint16_t y, uint16_t c, uint16_t color, uint16_t bg);
void uDMA1_Channel2_IRQHandler(void);

#endif /* INC_ILI9341_DRIVER_H_ */
