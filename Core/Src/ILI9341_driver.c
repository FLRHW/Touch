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
    ILI9341_SPI_Send(0x88); // RGB, portrait
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








/*
// Draws "len" characters using contents from line buffer, up to an entire line, starting at x, y
void drawS(uint16_t x, uint16_t y, uint8_t len) {
	uint16_t i;
	uint16_t totcols = CHARWIDTH * len;
	ILI9341_SetAddress(x, y, x + totcols - 1, y + CHARHEIGHT - 1);
    DC_HIGH();
    CS_LOW();
    for(i = 0; i < CHARHEIGHT*totcols*2; i++) {
    	ILI9341_SPI_Send(linebuff[i]);
    }
	CS_HIGH();
}
 */



/*
void refresh() {
    uint32_t i = 0;
	ILI9341_SetAddress(0, 0, DISPWIDTH, DISPHEIGHT);
    DC_HIGH();
    CS_LOW();
    for(i = 0; i < DISPWIDTH*DISPHEIGHT*2; i++) {
    	ILI9341_SPI_Send(scrbuf[i]);
    }
	CS_HIGH();
}
*/

/*
// Draw a complete string, without draw char function
void draw(uint16_t x, uint16_t y, uint8_t len) {
    uint16_t totcols = CHARWIDTH * len;
	ILI9341_SetAddress(x, y, x + totcols - 1, y + CHARHEIGHT - 1);
    DC_HIGH();
    CS_LOW();
    for(uint8_t row = 0; row < CHARHEIGHT; row++) {
    	for(uint8_t col = 0; col < totcols; col++) {
    //       ILI9341_SPI_Send(strbuffer[row][col] >> 8);   // High byte
    //       ILI9341_SPI_Send(strbuffer[row][col] & 0xFF); // Low byte
            ILI9341_SPI_Send(strbuffer2[2 * (row * totcols + col)]);
            ILI9341_SPI_Send(strbuffer2[2 * (row * totcols + col) + 1]);
        }
    }
    for(uint16_t i = 0; i < CHARHEIGHT*totcols*2; i++) {
    	ILI9341_SPI_Send(linebuff[i]);
    }
	CS_HIGH();
}

    for(uint8_t row = 0; row < CHARHEIGHT; row++) {
        cntchar = 0;
    	for(uint8_t col = 0; col < CHARWIDTH*len; col++) {
    		bitmap = font11x23[*(str+cntchar)];
            bits = bitmap[row];
            if(bits & (1<<(15-ccol))) {
//                ILI9341_SPI_Send(color >> 8);   // High byte
  //              ILI9341_SPI_Send(color & 0xFF); // Low byte
                strbuffer[row][col] = color;
        	} else {
    //    		ILI9341_SPI_Send(bg >> 8);   // High byte
      //  		ILI9341_SPI_Send(bg & 0xFF); // Low byte
        		strbuffer[row][col] = bg;
        	}
        	if (ccol++ == CHARWIDTH-1) {
        		ccol = 0;
        		cntchar++;
        	}
        }
    }
    DC_HIGH();
    CS_LOW();
    for(uint8_t row = 0; row < CHARHEIGHT; row++) {
    	for(uint8_t col = 0; col < CHARWIDTH*len; col++) {
            ILI9341_SPI_Send(strbuffer[row][col] >> 8);   // High byte
            ILI9341_SPI_Send(strbuffer[row][col] & 0xFF); // Low byte
        }
    }
	CS_HIGH();
}
*/

/*
// Draw a complete string, without draw char function
void ILI9341_DrawStringD_BKP(uint16_t x, uint16_t y, uint8_t* str, uint16_t color, uint16_t bg) {
	uint8_t len = 0, ccol = 0, cntchar;
	const uint16_t* bitmap;
	uint16_t bits;
	uint16_t strbuffer[CHARHEIGHT][DISPWIDTH];
	if (str != NULL) {
		while (str[len] != '\0') {
			len++;
		}
	}
    ILI9341_SetAddress(x, y, x + CHARWIDTH*len - 1, y + CHARHEIGHT - 1);
    for(uint8_t row = 0; row < CHARHEIGHT; row++) {
        cntchar = 0;
    	for(uint8_t col = 0; col < CHARWIDTH*len; col++) {
    		bitmap = font11x23[*(str+cntchar)];
            bits = bitmap[row];
            if(bits & (1<<(15-ccol))) {
//                ILI9341_SPI_Send(color >> 8);   // High byte
  //              ILI9341_SPI_Send(color & 0xFF); // Low byte
                strbuffer[row][col] = color;
        	} else {
    //    		ILI9341_SPI_Send(bg >> 8);   // High byte
      //  		ILI9341_SPI_Send(bg & 0xFF); // Low byte
        		strbuffer[row][col] = bg;
        	}
        	if (ccol++ == CHARWIDTH-1) {
        		ccol = 0;
        		cntchar++;
        	}
        }
    }
    DC_HIGH();
    CS_LOW();
    for(uint8_t row = 0; row < CHARHEIGHT; row++) {
    	for(uint8_t col = 0; col < CHARWIDTH*len; col++) {
            ILI9341_SPI_Send(strbuffer[row][col] >> 8);   // High byte
            ILI9341_SPI_Send(strbuffer[row][col] & 0xFF); // Low byte
        }
    }
	CS_HIGH();
}

*/
/*
// Command/Data write
void ILI9341_WriteCommand(uint8_t cmd) {
    DC_LOW();
    CS_LOW();
    ILI9341_SPI_Send(cmd);
    CS_HIGH();
}
*/
/*
void ILI9341_WriteData(uint8_t data) {
    DC_HIGH();
    CS_LOW();
    ILI9341_SPI_Send(data);
    CS_HIGH();
}
*/

/*
void ILI9341_WriteData16(uint16_t data) {
    DC_HIGH();
    CS_LOW();
    ILI9341_SPI_Send(data >> 8);   // High byte
    ILI9341_SPI_Send(data & 0xFF); // Low byte
    CS_HIGH();
}
*/


/*
// Set address window
void ILI9341_SetAddressBKP(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    ILI9341_WriteCommand(ILI9341_CASET);
    ILI9341_WriteData(x0 >> 8);
    ILI9341_WriteData(x0 & 0xFF);
    ILI9341_WriteData(x1 >> 8);
    ILI9341_WriteData(x1 & 0xFF);
    ILI9341_WriteCommand(ILI9341_PASET);
    ILI9341_WriteData(y0 >> 8);
    ILI9341_WriteData(y0 & 0xFF);
    ILI9341_WriteData(y1 >> 8);
    ILI9341_WriteData(y1 & 0xFF);
    ILI9341_WriteCommand(ILI9341_RAMWR);
}
*/

/*
// Fill screen with color
void ILI9341_FillScreenBKP(uint16_t color) {
    ILI9341_SetAddress(0, 0, 239, 319);
    for(uint32_t i = 0; i < 240*320; i++) {
        ILI9341_WriteData16(color);
    }
}
*/

/*
// Draw a complete string, without draw char function
void ILI9341_DrawStringDbkp2(uint16_t x, uint16_t y, uint8_t* str, uint16_t color, uint16_t bg) {
	uint8_t len = 0, ccol = 0, cntchar;
	const uint16_t* bitmap;
	uint16_t bits;
	if (str != NULL) {
		while (str[len] != '\0') {
			len++;
		}
	}
    ILI9341_SetAddress(x, y, x + CHARWIDTH*len - 1, y + CHARHEIGHT - 1);
    DC_HIGH();
    CS_LOW();
    for(uint8_t row = 0; row < CHARHEIGHT; row++) {
        cntchar = 0;
    	for(uint8_t col = 0; col < CHARWIDTH*len; col++) {
    		bitmap = font11x23[*(str+cntchar)];
            bits = bitmap[row];
            if(bits & (1<<(15-ccol))) {
                ILI9341_SPI_Send(color >> 8);   // High byte
                ILI9341_SPI_Send(color & 0xFF); // Low byte
        	} else {
        		ILI9341_SPI_Send(bg >> 8);   // High byte
        		ILI9341_SPI_Send(bg & 0xFF); // Low byte
        	}
        	if (ccol++ == CHARWIDTH-1) {
        		ccol = 0;
        		cntchar++;
        	}
        }
    }
	CS_HIGH();
}
*/
/*

// Draw a complete string, without draw char function
void ILI9341_DrawStringDBKP(uint16_t x, uint16_t y, uint8_t* str, uint16_t color, uint16_t bg) {
	uint8_t len = 0, ccol = 0, cntchar;
	const uint16_t* bitmap;
	uint16_t bits;
	if (str != NULL) {
		while (str[len] != '\0') {
			len++;
		}
	}
    ILI9341_SetAddress(x, y, x + CHARWIDTH*len - 1, y + CHARHEIGHT - 1);
    for(uint8_t row = 0; row < CHARHEIGHT; row++) {
        cntchar = 0;
    	for(uint8_t col = 0; col < CHARWIDTH*len; col++) {
    		bitmap = font11x23[*(str+cntchar)];
            bits = bitmap[row];
        	if(bits & (1<<(15-ccol))) {
                ILI9341_WriteData16(color);
        	} else {
        		ILI9341_WriteData16(bg);
        	}
        	if (ccol++ == CHARWIDTH-1) {
        		ccol = 0;
        		cntchar++;
        	}
        }
    }
}
*/

/*

// Display initialization
void ILI9341_Init(void) {
	// Set CS, DC, RST high initially
	CS_HIGH();
	DC_HIGH();
	RST_HIGH();
	LL_mDelay(100);

	LL_SPI_Enable(SPI1);

    // Reset sequence
    RST_LOW();
    LL_mDelay(20);
    RST_HIGH();
    LL_mDelay(120);

    ILI9341_WriteCommand(ILI9341_SWRESET);
    LL_mDelay(120);

    ILI9341_WriteCommand(ILI9341_SLPOUT);
    LL_mDelay(120);

    ILI9341_WriteCommand(ILI9341_COLMOD);
    ILI9341_WriteData(0x55); // 16-bit color

    ILI9341_WriteCommand(ILI9341_MADCTL);
    ILI9341_WriteData(0x88); // RGB, portrait

    ILI9341_WriteCommand(ILI9341_DISPON);
    LL_mDelay(120);

}
*/

// char TMP_str[6];
// ILI9341_DrawString(60, 20, TMP_str, white, black);

/*
// Draw a single character
void ILI9341_DrawChar(uint16_t x, uint16_t y, uint16_t c, uint16_t color, uint16_t bg) {
    const uint16_t* bitmap = font11x23[c];
    ILI9341_SetAddress(x, y, x + charwidth - 1, y + charheight - 1);
    for(uint8_t row = 0; row < charheight; row++) {
        uint16_t bits = bitmap[row];
        for(uint8_t col = 0; col < charwidth; col++) {
            if(bits & (1<<(15-col)))
                ILI9341_WriteData16(color);
            else
                ILI9341_WriteData16(bg);
        }
    }
}
*/
/*
// Draw a string using draw char function
void ILI9341_DrawString(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg) {
	while(*str) {
        ILI9341_DrawChar(x, y, *str, color, bg);
        x += charwidth + 1;
        str++;
        if (x > dispwidth-charwidth) {
        			x = 0;
        			y += charheight + 1;
        }
    }
}
*/


/*

void ILI9341_DrawStringD_BKP(uint16_t x, uint16_t y, const uint8_t* str, uint16_t color, uint16_t bg) {
	uint8_t len = 0, ccol = 0;
	const uint16_t* bitmap;
	uint16_t bits;
	const uint8_t* currchar;
	if (str != NULL) {
		while (str[len] != '\0') {
			len++;
		}
	}
    ILI9341_SetAddress(x, y, x + charwidth*len - 1, y + charheight - 1);
    for(uint8_t row = 0; row < charheight; row++) {
        currchar = str;
    	for(uint8_t col = 0; col < charwidth*len; col++) {
    		bitmap = font11x23[*currchar];
            bits = bitmap[row];
        	if(bits & (1<<(15-ccol))) {
                ILI9341_WriteData16(color);
        	} else {
        		ILI9341_WriteData16(bg);
        	}
        	if (ccol++ == charwidth-1) {
        		currchar++;
        		ccol = 0;
        	}
        }
    }
}
*/



/*
uint16_t x;
uint16_t y;
const char* str;
uint16_t colour;
uint16_t bg;

// char TMP_str[6];
// ILI9341_DrawString(60, 20, TMP_str, white, black);
// (uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg)
//	step		does
//	0			set address
//	1


SPI_ISR (void)









void ILI9341_DrawString(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg) {
    while(*str) {
    	const uint16_t* bitmap = font11x23[*str];
		x0 = x;
		y0 = y;
		x1 = x + charwidth - 1;
		y1 = y + charheight - 1;
		DC_LOW();
		CS_LOW();
		while(!LL_SPI_IsActiveFlag_TXE(SPI1));
		LL_SPI_TransmitData8(SPI1, ILI9341_CASET);
		while(LL_SPI_IsActiveFlag_BSY(SPI1));
		CS_HIGH();

		DC_HIGH();
		CS_LOW();
		while(!LL_SPI_IsActiveFlag_TXE(SPI1));
		LL_SPI_TransmitData8(SPI1, x0 >> 8);
		while(LL_SPI_IsActiveFlag_BSY(SPI1));
		CS_HIGH();

		DC_HIGH();
		CS_LOW();
		while(!LL_SPI_IsActiveFlag_TXE(SPI1));
		LL_SPI_TransmitData8(SPI1, x0 & 0xFF);
		while(LL_SPI_IsActiveFlag_BSY(SPI1));
		CS_HIGH();

		DC_HIGH();
		CS_LOW();
		while(!LL_SPI_IsActiveFlag_TXE(SPI1));
		LL_SPI_TransmitData8(SPI1, x1 >> 8);
		while(LL_SPI_IsActiveFlag_BSY(SPI1));
		CS_HIGH();

		DC_HIGH();
		CS_LOW();
		while(!LL_SPI_IsActiveFlag_TXE(SPI1));
		LL_SPI_TransmitData8(SPI1, x1 & 0xFF);
		while(LL_SPI_IsActiveFlag_BSY(SPI1));
		CS_HIGH();

		DC_LOW();
		CS_LOW();
		while(!LL_SPI_IsActiveFlag_TXE(SPI1));
		LL_SPI_TransmitData8(SPI1, ILI9341_PASET);
		while(LL_SPI_IsActiveFlag_BSY(SPI1));
		CS_HIGH();

		DC_HIGH();
		CS_LOW();
		while(!LL_SPI_IsActiveFlag_TXE(SPI1));
		LL_SPI_TransmitData8(SPI1, y0 >> 8);
		while(LL_SPI_IsActiveFlag_BSY(SPI1));
		CS_HIGH();

		DC_HIGH();
		CS_LOW();
		while(!LL_SPI_IsActiveFlag_TXE(SPI1));
		LL_SPI_TransmitData8(SPI1, y0 & 0xFF);
		while(LL_SPI_IsActiveFlag_BSY(SPI1));
		CS_HIGH();

		DC_HIGH();
		CS_LOW();
		while(!LL_SPI_IsActiveFlag_TXE(SPI1));
		LL_SPI_TransmitData8(SPI1, y1 >> 8);
		while(LL_SPI_IsActiveFlag_BSY(SPI1));
		CS_HIGH();

		DC_HIGH();
		CS_LOW();
		while(!LL_SPI_IsActiveFlag_TXE(SPI1));
		LL_SPI_TransmitData8(SPI1, y1 & 0xFF);
		while(LL_SPI_IsActiveFlag_BSY(SPI1));
		CS_HIGH();

		DC_LOW();
		CS_LOW();
		while(!LL_SPI_IsActiveFlag_TXE(SPI1));
		LL_SPI_TransmitData8(SPI1, ILI9341_RAMWR);
		while(LL_SPI_IsActiveFlag_BSY(SPI1));
		CS_HIGH();


			}
            }
            for(uint8_t row = 0; row < charheight; row++) {
                uint16_t bits = bitmap[row];
                for(uint8_t col = 0; col < charwidth; col++) {
                    if(bits & (1<<(15-col)))
                        ILI9341_WriteData16(color);
                    else
                        ILI9341_WriteData16(bg);
                }
            }
        }
        // ****************************
        x += charwidth + 1;
        str++;
        if (x > dispwidth-charwidth) {
        			x = 0;
        			y += charheight + 1;
        }
    }
}









void ILI9341_DrawString(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg) {
    while(*str) {
        ILI9341_DrawChar(x, y, *str, color, bg);
        //------------------------
        void ILI9341_DrawChar(uint16_t x, uint16_t y, uint16_t c, uint16_t color, uint16_t bg) {
            const uint16_t* bitmap = font11x23[c];
            ILI9341_SetAddress(x, y, x + charwidth - 1, y + charheight - 1);
            //------------------------
            void ILI9341_SetAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
                ILI9341_WriteCommand(ILI9341_CASET);
                //------------------------
                void ILI9341_WriteCommand(uint8_t cmd) {
                    DC_LOW();
                    CS_LOW();
                    ILI9341_SPI_Send(cmd);
                    //------------------------
                    void ILI9341_SPI_Send(uint8_t data) {
                        while(!LL_SPI_IsActiveFlag_TXE(SPI1));
                        LL_SPI_TransmitData8(SPI1, data);
                        while(LL_SPI_IsActiveFlag_BSY(SPI1));
                    }
                    // ****************************
                    CS_HIGH();
                }
                // ****************************
                ILI9341_WriteData(x0 >> 8);
                //------------------------
                void ILI9341_WriteData(uint8_t data) {
                    DC_HIGH();
                    CS_LOW();
                    ILI9341_SPI_Send(data);
                    //------------------------
                    void ILI9341_SPI_Send(uint8_t data) {
                    	while(!LL_SPI_IsActiveFlag_TXE(SPI1));
                        LL_SPI_TransmitData8(SPI1, data);
                        while(LL_SPI_IsActiveFlag_BSY(SPI1));
                    }
                    // ****************************
                    CS_HIGH();
                }
                // ****************************
                ILI9341_WriteData(x0 & 0xFF);
                //------------------------
                void ILI9341_WriteData(uint8_t data) {
                    DC_HIGH();
                    CS_LOW();
                    ILI9341_SPI_Send(data);
                    //------------------------
                    void ILI9341_SPI_Send(uint8_t data) {
                    	while(!LL_SPI_IsActiveFlag_TXE(SPI1));
                        LL_SPI_TransmitData8(SPI1, data);
                        while(LL_SPI_IsActiveFlag_BSY(SPI1));
                    }
                    // ****************************
                    CS_HIGH();
                }
                // ****************************
                ILI9341_WriteData(x1 >> 8);
                //------------------------
                void ILI9341_WriteData(uint8_t data) {
                    DC_HIGH();
                    CS_LOW();
                    ILI9341_SPI_Send(data);
                    //------------------------
                    void ILI9341_SPI_Send(uint8_t data) {
                    	while(!LL_SPI_IsActiveFlag_TXE(SPI1));
                        LL_SPI_TransmitData8(SPI1, data);
                        while(LL_SPI_IsActiveFlag_BSY(SPI1));
                    }
                    // ****************************
                    CS_HIGH();
                }
                // ****************************
                ILI9341_WriteData(x1 & 0xFF);
                //------------------------
                void ILI9341_WriteData(uint8_t data) {
                    DC_HIGH();
                    CS_LOW();
                    ILI9341_SPI_Send(data);
                    //------------------------
                    void ILI9341_SPI_Send(uint8_t data) {
                    	while(!LL_SPI_IsActiveFlag_TXE(SPI1));
                        LL_SPI_TransmitData8(SPI1, data);
                        while(LL_SPI_IsActiveFlag_BSY(SPI1));
                    }
                    // ****************************
                    CS_HIGH();
                }
                // ****************************
                ILI9341_WriteCommand(ILI9341_PASET);
                //------------------------
                void ILI9341_WriteCommand(uint8_t cmd) {
                    DC_LOW();
                    CS_LOW();
                    ILI9341_SPI_Send(cmd);
                    //------------------------
                    void ILI9341_SPI_Send(uint8_t data) {
                    	while(!LL_SPI_IsActiveFlag_TXE(SPI1));
                        LL_SPI_TransmitData8(SPI1, data);
                        while(LL_SPI_IsActiveFlag_BSY(SPI1));
                    }
                    // ****************************
                    CS_HIGH();
                }
                // ****************************
                ILI9341_WriteData(y0 >> 8);
                //------------------------
                void ILI9341_WriteData(uint8_t data) {
                    DC_HIGH();
                    CS_LOW();
                    ILI9341_SPI_Send(data);
                    //------------------------
                    void ILI9341_SPI_Send(uint8_t data) {
                    	while(!LL_SPI_IsActiveFlag_TXE(SPI1));
                        LL_SPI_TransmitData8(SPI1, data);
                        while(LL_SPI_IsActiveFlag_BSY(SPI1));
                    }
                    // ****************************
                    CS_HIGH();
                }
                // ****************************
                ILI9341_WriteData(y0 & 0xFF);
                //------------------------
                void ILI9341_WriteData(uint8_t data) {
                    DC_HIGH();
                    CS_LOW();
                    ILI9341_SPI_Send(data);
                    //------------------------
                    void ILI9341_SPI_Send(uint8_t data) {
                    	while(!LL_SPI_IsActiveFlag_TXE(SPI1));
                        LL_SPI_TransmitData8(SPI1, data);
                        while(LL_SPI_IsActiveFlag_BSY(SPI1));
                    }
                    // ****************************
                    CS_HIGH();
                }
                // ****************************
                ILI9341_WriteData(y1 >> 8);
                //------------------------
                void ILI9341_WriteData(uint8_t data) {
                    DC_HIGH();
                    CS_LOW();
                    ILI9341_SPI_Send(data);
                    //------------------------
                    void ILI9341_SPI_Send(uint8_t data) {
                    	while(!LL_SPI_IsActiveFlag_TXE(SPI1));
                        LL_SPI_TransmitData8(SPI1, data);
                        while(LL_SPI_IsActiveFlag_BSY(SPI1));
                    }
                    // ****************************
                    CS_HIGH();
                }
                // ****************************
                ILI9341_WriteData(y1 & 0xFF);
                //------------------------
                void ILI9341_WriteData(uint8_t data) {
                    DC_HIGH();
                    CS_LOW();
                    ILI9341_SPI_Send(data);
                    //------------------------
                    void ILI9341_SPI_Send(uint8_t data) {
                    	while(!LL_SPI_IsActiveFlag_TXE(SPI1));
                    	LL_SPI_TransmitData8(SPI1, data);
                    	while(LL_SPI_IsActiveFlag_BSY(SPI1));
                    }
                    // ****************************
                    CS_HIGH();
                }
                // ****************************
                ILI9341_WriteCommand(ILI9341_RAMWR);
                //------------------------
                void ILI9341_WriteCommand(uint8_t cmd) {
                    DC_LOW();
                    CS_LOW();
                    ILI9341_SPI_Send(cmd);
                    //------------------------
                    void ILI9341_SPI_Send(uint8_t data) {
                    	while(!LL_SPI_IsActiveFlag_TXE(SPI1));
                        LL_SPI_TransmitData8(SPI1, data);
                        while(LL_SPI_IsActiveFlag_BSY(SPI1));
                    }
                    // ****************************
                    CS_HIGH();
                }
                // ****************************
            }
            // ****************************
            for(uint8_t row = 0; row < charheight; row++) {
                uint16_t bits = bitmap[row];
                for(uint8_t col = 0; col < charwidth; col++) {
                    if(bits & (1<<(15-col)))
                        ILI9341_WriteData16(color);
                    else
                        ILI9341_WriteData16(bg);
                }
            }
        }
        // ****************************
        x += charwidth + 1;
        str++;
        if (x > dispwidth-charwidth) {
        			x = 0;
        			y += charheight + 1;
        }
    }
}




























	while(*str) {
        const uint16_t* bitmap = font11x23[*str];

        x1 = x + charwidth - 1;
        y1 = y + charheight - 1;

        DC_LOW();
        CS_LOW();
        ILI9341_SPI_Send(ILI9341_CASET);
        CS_HIGH();

        DC_HIGH();
        CS_LOW();
        ILI9341_SPI_Send(x >> 8);
        CS_HIGH();

        DC_HIGH();
        CS_LOW();
        ILI9341_SPI_Send(x & 0xFF);
        CS_HIGH();

        DC_HIGH();
        CS_LOW();
        ILI9341_SPI_Send(x1 >> 8);
        CS_HIGH();

        DC_HIGH();
        CS_LOW();
        ILI9341_SPI_Send(x1 & 0xFF);
        CS_HIGH();

        DC_LOW();
        CS_LOW();
        ILI9341_SPI_Send(ILI9341_PASET);
        ILI9341_WriteData(y >> 8);

        ILI9341_WriteData(y & 0xFF);

        ILI9341_WriteData(y1 >> 8);

        ILI9341_WriteData(y1 & 0xFF);

        ILI9341_SPI_Send(ILI9341_RAMWR);

        for(uint8_t row = 0; row < charheight; row++) {
            uint16_t bits = bitmap[row];
            for(uint8_t col = 0; col < charwidth; col++) {
                if(bits & (1<<(15-col)))
                    ILI9341_WriteData16(color);
                else
                    ILI9341_WriteData16(bg);
                }
            }
        }

        x += charwidth + 1;
        str++;
        if (x > dispwidth-charwidth) {
        			x = 0;
        			y += charheight + 1;
        }
    }
}


*/
