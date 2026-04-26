/*
 * hmi.c
 *
 *  Created on: Jun 19, 2025
 *      Author: Felipe
 */

// ------------------------- 		INCLUDES 		-------------------------

#include <stdint.h>
#include <stdbool.h>
#include "fonts.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_adc.h"
#include "stm32g4xx_ll_spi.h"
#include "stm32g4xx_ll_utils.h"
#include "stm32g4xx_ll_tim.h"
#include "ILI9341_driver.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_usart.h"
#include "shared.h"
//#include "stm32g4xx_ll_rcc.h"
//#include "stm32g4xx_ll_system.h"
//#include "stm32g4xx_ll_gpio.h"
//#include "stm32g4xx_ll_exti.h"
//#include "stm32g4xx_ll_bus.h"
//#include "stm32g4xx_ll_cortex.h"
//#include "stm32g4xx_ll_pwr.h"
//#include "stm32g4xx_ll_i2c.h"


// ------------------------- 		DEFINES 		-------------------------

#define	MAX_BIN				7
#define	SETTLE_CNT			10				// How many times a bin has to occur in sequence to be considered stable
#define	HIST				20				// Recorded bin history
#define	PAT_SIZE			5				// Number of states in a pattern
#define	SEL_TIME			60000			// Time to change the requested parameter, in ms
#define HOLD_TIME			500				// Time to ignore button press request after a previous press, in ms
#define INC_DEC_TIME		5				// Time to ignore increase / decrease request after a previous request, in ms
//#define PAR_REQ_RFSH_PER	50				// Time between subsequent screen updates (parameter request), in ms
#define MEAS_RFSH_PER		33				// Time between subsequent screen updates (measurement data), in ms
#define	DBG_INFO_RFSH_PER	511				// Time between subsequent screen updates (debug information), in ms
#define	UPTIME_RFSH_PER		200			// Time between subsequent screen updates (uptime information), in ms
#define LED_TOGGLE_PER		500				// Time to toggle LED status, in ms
#define UART_TX_PER			200				// Time between subsequent uart transmissions, in ms
#define PERINI				20				// Default period, in us
#define FREQINI				100				// Default frequency 1 = 0.01 Hz
#define SUPINI				600				// Default supply voltage, in V
#define PERMIN				10				// Minimum period, in us
#define FREQMIN				5				// Minimum frequency 1 = 0.01 Hz
#define SUPMIN				400				// Minimum supply voltage, in V
#define PERMAX				800000			// Maximum period, in us
#define FREQMAX				20000			// Maximum frequency 1 = 0.01 Hz
#define SUPMAX				1000			// Maximum supply voltage, in V
#define BUSINI				600
#define POWERINI			10
#define IPEAKINI			1
#define BUSMIN				400
#define POWERMIN			0
#define IPEAKMIN			0
#define BUSMAX				1000
#define POWERMAX			1000
#define IPEAKMAX			900
#define RX_SIZE				10				// Amount of bytes to be received from the uart
#define MEASCNT				3				// Amount of measured parameters
#define SEP					1


// ------------------------- 		MACROS			-------------------------

#define LEDON			LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_0)
#define LEDOFF			LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_0)

#define GET_BYTE(x, n)  ((uint8_t)((x) >> (8 * (n)) & 0xFF))	// Returns the nth byte


#define B0ON			LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_9)
#define B0OFF			LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_9)
#define B1ON			LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_8)
#define B1OFF			LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_8)
#define B2ON			LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_15)
#define B2OFF			LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_15)

#define ALLON			B0ON; B1ON; B2ON;
#define ALLOFF			B0OFF; B1OFF; B2OFF;


// ------------------------- 		ENUMS			-------------------------

enum ParamTypes { PERIOD, FREQUENCY, SUPPLY, PARAMS };
enum MeasTypes { BUS, POWER, IPEAK, MEASUREMENTS };
typedef enum { INCREASE, DECREASE, BUTTONP, ACTYPES } ReqTypes;


// ------------------------- 		VARIABLES 		-------------------------


uint8_t loopcnt = 0;

volatile uint32_t miliseconds = 0;
uint32_t highlight_expiry[PARAMS];
uint8_t bin_hist[PARAMS][HIST];			// Stores the latest bin results for each channel
uint16_t adc_bkp[PARAMS];					// Copy of latest ADC values - requested period, frequency and supply
uint8_t dotpos[PARAMS] = {3, 2, 0};
uint8_t changepos[PARAMS];
volatile bool new_rx = 0;

volatile uint16_t adc_out[PARAMS];			// DMA dumps ADC readings here
volatile uint8_t uart_rx_buf[RX_SIZE];
//uint8_t meas_data[RX_SIZE];

int32_t param_req[PARAMS] = {PERINI, FREQINI, SUPINI};// Stores desired Period, Frequency and Supply voltage
const uint16_t param_min[PARAMS] = {PERMIN, FREQMIN, SUPMIN};
const uint32_t param_max[PARAMS] = {PERMAX, FREQMAX, SUPMAX};
uint8_t maxpos_par[PARAMS] = {6, 5, 4};	// Stores maximum number of digits for each parameter

int32_t meas[PARAMS] = {BUSINI, POWERINI, IPEAKINI};							// Buffer for decoded "meas_data"
const uint16_t meas_min[MEASUREMENTS] = {BUSMIN, POWERMIN, IPEAKMIN};
const uint16_t meas_max[MEASUREMENTS] = {BUSMAX, POWERMAX, IPEAKMAX};
uint8_t maxpos_meas[MEASCNT] = {4, 4, 3};	// Stores maximum number of digits for each measurement
bool param_update;
uint8_t param_disp;

const uint8_t pattern[ACTYPES][PAT_SIZE] = {// Data is stored in reverse order in the array, i.e., pattern[x][0] is the last in the sequence whereas pattern [x][PAT_SIZE-1] is the 1st
 	{7,5,4,6,7},							// Encoder rotates clockwise
	{7,6,4,5,7},							// Encoder rotates counter-clockwise
	{3,3,3,3,3},							// Button press
};

/*
 	{7,6,4,5,7},							// Encoder rotates clockwise
	{7,5,4,6,7},							// Encoder rotates counter-clockwise
	{3,3,3,3,3},							// Button press
};
 */

// ------------------------- 	INTERRUPT HANDLERS 	-------------------------

void uDMA1_Channel3_IRQHandler(void) {		// USART TX
	if (LL_DMA_IsActiveFlag_TC3(DMA1)) {
	    LL_DMA_ClearFlag_TC3(DMA1);
	    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
	}
//	LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_0);
//	LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_0);
//	LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_0);
}

void uDMA1_Channel4_IRQHandler(void) {		// USART RX
	LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);
	if (LL_DMA_IsActiveFlag_TC4(DMA1)){
  //      for (uint8_t i = 0; i < RX_SIZE; i++) {
    //    	meas_data[i] = uart_rx_buf[i];
      //  }
        new_rx = true;
    	LL_DMA_ClearFlag_TC4(DMA1);
    }
    if (LL_DMA_IsActiveFlag_TE4(DMA1)){
        LL_DMA_ClearFlag_TE4(DMA1);
        // Handle DMA error
    }
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_4, RX_SIZE);
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_4);
}

void uTIM2_IRQHandler(void) {				// Currently running @ 5 kHz. Triggers screen update and other timed events
	static uint16_t irq_cnt = 0;			// Counts from 0-4999 each time TIM2 IRQ is run (@ 5 kHz | 200 uS intervals)
	static uint8_t msec_cnt = 0;
	if (LL_TIM_IsActiveFlag_UPDATE(TIM2)) {
        LL_TIM_ClearFlag_UPDATE(TIM2);
		if (msec_cnt++ == 4) {
        	miliseconds++;
        	msec_cnt = 0;
		}
        if (irq_cnt++ == 4999) {
        	irq_cnt = 0;
        }
	}
}


// ------------------------- 	OTHER FUNCTIONS		-------------------------

void UART1_Transmit_DMA(uint8_t* buff, uint8_t len) {
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_3, (uint32_t)buff);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, len);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_3);
}


void hmi_config(void) {						// Carries out general configuration for the board
	LL_Init1msTick(SystemCoreClock);		// Required as auto generated code only initialises HAL system tick (due to USB relying on it)

	LL_DMA_ConfigAddresses(					// DMA configuration to automatically read ADC conversion results
	    DMA1,
	    LL_DMA_CHANNEL_1,
	    LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA),
	    (uint32_t)adc_out,
	    LL_DMA_DIRECTION_PERIPH_TO_MEMORY
	);
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, PARAMS);
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

	LL_ADC_StartCalibration(ADC1, LL_ADC_SINGLE_ENDED);	// Calibrates, enables and starts ADC
	while (LL_ADC_IsCalibrationOnGoing(ADC1));
	LL_mDelay(1);
	LL_ADC_Enable(ADC1);
	while (!LL_ADC_IsActiveFlag_ADRDY(ADC1));
	LL_ADC_REG_StartConversion(ADC1);



	LL_TIM_EnableIT_UPDATE(TIM2);    		// Enable update event interrupt for timer 2
	LL_TIM_GenerateEvent_UPDATE(TIM2); 		// Load prescaler/ARR
	LL_TIM_EnableCounter(TIM2);      		// Start counting

	LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_3, (uint32_t)&USART1->TDR);
	LL_USART_EnableDMAReq_TX(USART1);
	LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
	LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_3);
//	UART1_Transmit_DMA(uart_tx_buf,9);

//	uDMA1_Channel3_IRQHandler();



// Disable DMA channel before configuring
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);

    // Clear DMA transfer and error flags for channel 4
    LL_DMA_ClearFlag_TC4(DMA1);
    LL_DMA_ClearFlag_TE4(DMA1);

    // Set peripheral and memory addresses and data length
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_4,
        LL_USART_DMA_GetRegAddr(USART1, LL_USART_DMA_REG_DATA_RECEIVE),
        (uint32_t)uart_rx_buf,
        LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_4, RX_SIZE);

    // Enable DMA channel to start reception
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_4);
    LL_USART_EnableDMAReq_RX(USART1);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_4);  // Transfer complete interrupt
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_4);  // Transfer error interrupt (optional)

    LL_USART_Disable(USART1);				// RESET UART (otherwise power cycle is required for proper execution)
    LL_USART_Enable(USART1);





}


void u32_to_ascii(uint32_t number, char *str, uint8_t min_digs) {	// Creates an ascii string corresponding to "number"
    char temp[10]; 							// Enough for 10 digits (null written to *str
    uint8_t i = 0, j = 0;
    if (min_digs > 10) {					// Limits output to 10 digits
    	min_digs = 10;
    }
    while (i < min_digs || number > 0) {	// Carries out the conversion
    	temp[i++] = (number % 10) + '0';
        number /= 10;
    }
    while (i > 0) {							// Reverses the string into the output
        str[j++] = temp[--i];
    }
    str[j] = '\0';
}


uint8_t calc_bin (uint16_t adc) {			// Classifies the value given (adc) into one of the bins defined by thresholds from bin_th[]
	int8_t left = 0, mid = 0, right = MAX_BIN - 1;	// Must be signed as some values can be negative
	static const uint16_t bin_th[MAX_BIN] = {393, 944, 1501, 2046, 2603, 3195, 3979};	// Thresholds defining each bin
	while (left <= right) {
		mid = left + (right - left) / 2 ;
		if (adc < bin_th[mid]) {
			right = mid - 1;
		} else {
			left = mid + 1;
		}
	}
	return left;
}
//	Details about "const uint16_t bin_th[MAX_BIN]"
//	Range low		0		2373	2530	2707	2947	3223	3520	3873
// 	Range high		2373	2530	2707	2947	3223	3520	3873	4095
//	Bin				0		1		2		3		4		5		6		7


void sc_uptime(void) {
	char temp_string[10+1];					// Stores up to 10 digits + null
	u32_to_ascii (miliseconds, temp_string, 10);
	ILI9341_DrawStringD(sizeof("Uptime:") * CHARWIDTH, DISPHEIGHT - CHARHEIGHT - 1, temp_string, WHITE, BLACK);
}


void sc_param_req(uint8_t ch) {
	char temp_string[10+1];					// Stores up to 10 digits + null
	uint8_t past_dot;
	uint16_t x, y;

	u32_to_ascii (param_req[ch], temp_string, maxpos_par[ch]);
	past_dot = (dotpos[ch] == 0) ? 1 : 0;
	x = 120 + (6 - maxpos_par[ch] + past_dot) * CHARWIDTH;
	y = 106 - ch * CHARHEIGHT, temp_string;

	ILI9341_DrawStringDot(x, y, temp_string, dotpos[ch], WHITE, BLACK);

	eraseTrace(x,  y + CHARHEIGHT - 1, x + (maxpos_par[ch] + 1) * CHARWIDTH, BLACK);
	if (highlight_expiry[ch] > miliseconds) {
		past_dot = ((changepos[ch] >= dotpos[ch]) ? 1 : 0);
		drawTrace(x + (maxpos_par[ch] - changepos[ch] - past_dot ) * CHARWIDTH, y + CHARHEIGHT - 1, WHITE);
	}


	/*for (uint8_t ch = 0; ch < PARAMS; ch++) {
		u32_to_ascii (param_req[ch], temp_string, maxpos_par[ch]);

		past_dot = (dotpos[ch] == 0) ? 1 : 0;
		x = 120 + (6 - maxpos_par[ch] + past_dot) * CHARWIDTH;
		y = 106 - ch * CHARHEIGHT, temp_string;

		ILI9341_DrawStringDot(x, y, temp_string, dotpos[ch], WHITE, BLACK);

		eraseTrace(x,  y + CHARHEIGHT - 1, x + (maxpos_par[ch] + 1) * CHARWIDTH, BLACK);
		if (highlight_expiry[ch] > miliseconds) {
			past_dot = ((changepos[ch] >= dotpos[ch]) ? 1 : 0);
			drawTrace(x + (maxpos_par[ch] - changepos[ch] - past_dot ) * CHARWIDTH, y + CHARHEIGHT - 1, WHITE);
		}
	}*/
}


void sc_ain_dbg(void){
	char ptmp[3][HIST+1];
	uint8_t i;
	uint8_t j;
	for (i = 0; i < 3; i++) {
		for (j = 0; j < HIST; j++) {
			if (bin_hist[i][j] < 10) {
				ptmp[i][j] = '0' + bin_hist[i][j];
			}
		}
		ptmp[i][j] = '\0';
		ILI9341_DrawStringD(0, 265 - 24 * i, ptmp[i], WHITE, BLACK);
	}
}


void sc_meas_data (uint8_t id) {				// Prints the variable contents (target frequency, period, voltage, measurements)
	char temp_string[10+1];					// Stores up to 10 digits + null
	uint8_t past_dot;
	uint16_t x, y;
	if (id > 2) {
		return;
	}
	u32_to_ascii (meas[id], temp_string, maxpos_meas[id]);
	past_dot = 1;
	x = 120 + (6 - maxpos_meas[id] + past_dot) * CHARWIDTH;
	y = 106 - ((2-id) - PARAMS) * CHARHEIGHT, temp_string;
	ILI9341_DrawStringD(x, y, temp_string, WHITE, BLACK);
}


bool add_hist_data (uint8_t ch, uint8_t bin){// Adds the requested bin to the history
	if (bin_hist[ch][0] != bin || bin == 3) {
		for (uint8_t i = HIST - 1; i > 0; i--) {	// Moves all history one step back
			bin_hist[ch][i] = bin_hist[ch][i-1];
		}
		bin_hist[ch][0] = bin;				// Adds the requested bin to the latest position in the history
		return (true);						// Indicates successful inclusion
	}
	return (false);							// Indicates the bin was not included (same bin as the latest already in the history)
}


void param_inc(uint8_t ch){
	uint32_t adder = 1;
	uint8_t i = changepos[ch];
	// if i > number of digits in param[ch] -> error
	while (i > 0) {
		adder *= 10;
		i--;
	}
	param_req[ch] += adder;
	if (param_req[ch] > param_max[ch]) {
		param_req[ch] = param_max[ch];
	}
}


void param_dec(uint8_t ch){
	int32_t subber = 1;
	uint8_t i = changepos[ch];
	// if i > number of digits in param[ch] -> error
	while (i > 0) {
		subber *= 10;
		i--;
	}
	param_req[ch] -= subber;
	if (param_req[ch] < param_min[ch]) {
		param_req[ch] = param_min[ch];
	}
}


void pdbg_adc(uint8_t * data) {
	char tmp[4];
	uint8_t i;

	for (i = 0; i < 4; i++) {
		if (data[i] < 10) {
			tmp[i] = '0' + data[i];
		}
	}
	tmp[3] = '\0';
	ILI9341_DrawStringD(0, 260, tmp, WHITE, BLACK);
}


void process_adc (uint8_t ch) {
	bool inserted;
	static bool action[PARAMS][ACTYPES];
	static uint8_t bin[PARAMS], prev_bin[PARAMS], bin_cnt[PARAMS];

	bin[ch] = calc_bin(adc_bkp[ch]);

	// DEBUG AID
	if (ch == 1) {
		ALLON;
		switch (bin[ch]) {
			case 0:
				B0ON;
				B1ON;
				B2ON;
				break;
			case 1:
				B0OFF;
				B1ON;
				B2ON;
				break;
			case 2:
				B0ON;
				B1OFF;
				B2ON;
				break;
			case 3:
				B0OFF;
				B1OFF;
				B2ON;
				break;
			case 4:
				B0ON;
				B1ON;
				B2OFF;
				break;
			case 5:
				B0OFF;
				B1ON;
				B2OFF;
				break;
			case 6:
				B0ON;
				B1OFF;
				B2OFF;
				break;
			case 7:
				B0OFF;
				B1OFF;
				B2OFF;
				break;
			default:
				B0OFF;
				B1OFF;
				B2OFF;
				break;
		}
	}


	if (bin[ch] <= 3) {
		bin[ch] = 3;
	}

//	pdbg_adc(bin);


	if (bin[ch] != prev_bin[ch]) {			// De-bouncing - adds new value if it repeats SETTLE_CNT times
		prev_bin[ch] = bin[ch];
		bin_cnt[ch] = 1;
	} else if (bin_cnt[ch]++ >= SETTLE_CNT) {
		inserted = add_hist_data (ch, bin[ch]);		// Flag confirming insertion of new data point
		bin_cnt[ch] = SETTLE_CNT;
	}

	if (inserted) {							// Checks if latest data matches any of the expected patterns
	//	inserted = false;
		for (ReqTypes usr_req = 0; usr_req < ACTYPES; usr_req++){
			action[ch][usr_req] = true;
			for (uint8_t i = 0; i < PAT_SIZE; i++) {
				if (bin_hist[ch][i] != pattern[usr_req][i]) {
					action[ch][usr_req] = false;
					break;
				}
			}
			if (action[ch][usr_req]) {

				inserted = add_hist_data (ch, 0); // Adds a single 0 to break the sequence so that the action does not repeat
				param_update = true;
				param_disp = ch;
				// **********************************************************************************
				switch (usr_req) {
					case INCREASE:
						if (miliseconds + SEL_TIME - INC_DEC_TIME < highlight_expiry[ch] ) { // Limits how fast to increase
							break;
						}
						if (miliseconds < highlight_expiry[ch]) {
							param_inc(ch);
							highlight_expiry[ch] = miliseconds + SEL_TIME;
						}
						break;

					case DECREASE:
						if (miliseconds + SEL_TIME - INC_DEC_TIME < highlight_expiry[ch] ) { // Limits how fast to decrease
							break;
						}
						if (miliseconds < highlight_expiry[ch]) {
							param_dec(ch);
							highlight_expiry[ch] = miliseconds + SEL_TIME;
						}
						break;

					case BUTTONP:
						if (miliseconds + SEL_TIME - HOLD_TIME < highlight_expiry[ch] ) { // Limits how fast to change position
							break;
						}
						if (miliseconds > highlight_expiry[ch]) { // Does not change pos. on first button press after idle
							highlight_expiry[ch] = miliseconds + SEL_TIME;  // Activates highlight
							break;
						}
						highlight_expiry[ch] = miliseconds + SEL_TIME;  // Activates highlight
						if (changepos[ch]++ == maxpos_par[ch] - 1) {
							changepos[ch] = 0;
						}
						break;

					default:
						break;
				}
			}
		}
	}/* else {	// inserted = false;
		for (usr_req = 0; usr_req < ACTYPES; usr_req++){
			action[ch][usr_req] = false;
		}
	}*/
}


void load_def_screen(void) {
	ILI9341_FillScreen(BLACK); 				// Black background
	ILI9341_DrawStringD(50, 20, "1 kV STROBE", WHITE, BLACK);
	ILI9341_DrawStringD(10, 60,  "SUPPLY:           V ", YELLOW, BLACK);
	ILI9341_DrawStringD(10, 60 + 1 * CHARHEIGHT,  "FREQUENCY:        Hz", MAGENTA, BLACK);
	ILI9341_DrawStringD(10, 60 + 2 * CHARHEIGHT, "PERIOD:           ms", TURQUOISE, BLACK);
	ILI9341_DrawStringD(10, 60 + 3 * CHARHEIGHT, "BUS:              V", RED, BLACK);
	ILI9341_DrawStringD(10, 60 + 4 * CHARHEIGHT, "Power:            W", GREEN, BLACK);
	ILI9341_DrawStringD(10, 60 + 5 * CHARHEIGHT, "Ipeak:            A", BLUE, BLACK);
	ILI9341_DrawStringD(0, DISPHEIGHT - CHARHEIGHT - 1, "Uptime:            ms", WHITE, BLACK);
}


bool uart_rx (void) {
	static uint8_t sync_del;
	uint8_t crc = 0;
	bool rx_status = false;
	for(uint8_t i = 0; i < RX_SIZE - 1; i++) {	// Calculates CRC on the new data
		crc ^= uart_rx_buf[i];
		for(uint8_t j = 0; j < 8; j++) {
			crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
		}
	}
	if (crc == uart_rx_buf[RX_SIZE - 1]) {		// CRC is valid
		rx_status = true;
		uint32_t rxtemp;
		for (uint8_t i = 0; i < MEASUREMENTS; i++) {// Converts each sequence of 3 bytes into a 24 bit value
			rxtemp = (uart_rx_buf[3 * i]) | ((uart_rx_buf[3 * i + 1]) << 8) | ((uart_rx_buf[3 * i + 2]) << 16);
			if (rxtemp > meas_max[i]) {			// Only stores data if within the limits specified
				meas[i] = meas_max[i];
			} else if (rxtemp < meas_min[i]) {
				meas[i] = meas_min[i];
			} else {
				meas[i] = rxtemp;
			}
		}
	} else {
		rx_status = false;
		LL_USART_DisableDirectionRx(USART1);
		LL_mDelay(sync_del);
		sync_del++;
		crc = LL_USART_ReceiveData8(USART1);
		LL_USART_ClearFlag_ORE(USART1);
		LL_USART_ClearFlag_NE(USART1);
		LL_USART_ClearFlag_FE(USART1);
		LL_USART_ClearFlag_PE(USART1);
		LL_USART_EnableDirectionRx(USART1);
/*		LL_USART_Disable(USART1);				// RESET UART (might be required to resume normal operation)
	    LL_USART_Enable(USART1);
*/
	}
	return (rx_status);
}


void uart_tx (void) {
	static uint8_t uart_tx_buf[10];
	uint8_t crc = 0;
	for(uint8_t i = 0; i < 9; i++) {
		uart_tx_buf[i] = GET_BYTE(param_req[i / 3], i % 3);
	    crc ^= uart_tx_buf[i];
	    for(uint8_t j = 0; j < 8; j++) {
	        crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
	    }
	}
	uart_tx_buf[9] = crc;

	if (!LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_3)) {
		if (LL_USART_IsActiveFlag_TC(USART1)) {         // DMA channel not active// UART transmission complete
			UART1_Transmit_DMA(uart_tx_buf,10);
		}
	}
}


void hmi_main() {
	uint32_t meas_upd_t = 0, LED_tog_t = 0, uart_tr_t = 0, ain_dbg_t = 0, upt_t = 0, param_upd_t = 0;
	LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_0);	// Disables LEDS
	uint8_t meas_sc = 0;
	hmi_config();

	ILI9341_Init();
	load_def_screen();
	for (param_disp = 0; param_disp < 3; param_disp++) {
		sc_param_req(param_disp);
	}
	param_disp = 0;

	while (1) {
		if (miliseconds > LED_tog_t) {
			LED_tog_t = miliseconds + LED_TOGGLE_PER;
			LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_0);
		}
		if (miliseconds > meas_upd_t) {
			if (++meas_sc == 3) {
				meas_sc = 0;
			}
			sc_meas_data(meas_sc);
			meas_upd_t = miliseconds + MEAS_RFSH_PER;
		}
		if (miliseconds > ain_dbg_t) {
			//sc_ain_dbg();
			ain_dbg_t = miliseconds + DBG_INFO_RFSH_PER;
		}
		if (miliseconds > upt_t) {
			sc_uptime();
			upt_t = miliseconds + UPTIME_RFSH_PER;
		}
		if (param_update || (miliseconds > param_upd_t) ) {	// updates when a new parameter is set or when the selection is not active anymore
			param_update = false;
			param_upd_t = miliseconds + SEL_TIME;
			sc_param_req(param_disp);
		}
		if (miliseconds > uart_tr_t) {			// Time for new transmission
			uart_tr_t = miliseconds + UART_TX_PER;
			uart_tx();
		}
		if (LL_ADC_IsActiveFlag_EOS(ADC1)) {
			LL_ADC_ClearFlag_EOS(ADC1);
			for (uint8_t ch = 0; ch < PARAMS; ch++) {
				adc_bkp[ch] = adc_out[ch]; // saves a copy of ADC values before allowing new ADC conversion
			}
			LL_ADC_REG_StartConversion(ADC1);

			process_adc(0);
			process_adc(1);
			process_adc(2);
		}
		if (new_rx) {							// RX flag active
			uart_rx();
			new_rx = false;
		}
	}
}









