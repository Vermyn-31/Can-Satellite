/* 
 * File:   transmitter_init.c
 * Author: MSI
 *
 * Created on May 15, 2025, 4:53 AM
 */

#include <xc.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/////////////////////////////////////////////////////////////////////////////

// Enable higher frequencies for higher performance
static void raise_perf_level(void){
	uint32_t tmp_reg = 0;
	
	PM_REGS->PM_INTFLAG = 0x01;
	PM_REGS->PM_PLCFG = 0x02;
	while ((PM_REGS->PM_INTFLAG & 0x01) == 0)
		asm("nop");
	PM_REGS->PM_INTFLAG = 0x01;
	
	NVMCTRL_SEC_REGS->NVMCTRL_CTRLB = (2 << 1) ;
	SUPC_REGS->SUPC_VREGPLL = 0x00000302;
	while ((SUPC_REGS->SUPC_STATUS & (1 << 18)) == 0)
		asm("nop");
	
	OSCCTRL_REGS->OSCCTRL_DFLLCTRL = 0x0000;
	while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0)
		asm("nop");
	
	tmp_reg  = ((uint32_t)0x00806020);
	tmp_reg &= ((uint32_t)(0b111111) << 25);
	tmp_reg >>= 15;
	tmp_reg |= ((512 << 0) & 0x000003ff);
	OSCCTRL_REGS->OSCCTRL_DFLLVAL = tmp_reg;
	while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0)
		asm("nop");

	OSCCTRL_REGS->OSCCTRL_DFLLCTRL |= 0x0002;
	while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0)
		asm("nop");
	
	GCLK_REGS->GCLK_GENCTRL[2] = 0x00000105;
	while ((GCLK_REGS->GCLK_SYNCBUSY & (1 << 4)) != 0)
		asm("nop");
	
	// Switch over GCLK_GEN0 to DFLL48M, with DIV=2 to get 24 MHz.
	GCLK_REGS->GCLK_GENCTRL[0] = 0x00020107;
	while ((GCLK_REGS->GCLK_SYNCBUSY & (1 << 2)) != 0)
		asm("nop");
    
	
	// Done. We're now at 24 MHz.
	return;
}

static void EIC_init_early(void){
	GCLK_REGS->GCLK_PCHCTRL[4] = 0x00000042;
	while ((GCLK_REGS->GCLK_PCHCTRL[4] & 0x00000042) == 0)
		asm("nop");
	
	// Reset, and wait for said operation to complete.
	EIC_SEC_REGS->EIC_CTRLA = 0x01;
	while ((EIC_SEC_REGS->EIC_SYNCBUSY & 0x01) != 0)
		asm("nop");
	
	EIC_SEC_REGS->EIC_DPRESCALER = (0b0 << 16) | (0b0000 << 4) |
		                       (0b1111 << 0);
	
    //Exit
    return;
}
static void EIC_init_late(void){

	EIC_SEC_REGS->EIC_CTRLA |= 0x02;
	while ((EIC_SEC_REGS->EIC_SYNCBUSY & 0x02) != 0)
		asm("nop");
	return;
}

// Configure the EVSYS peripheral
static void EVSYS_init(void){
    
	EVSYS_SEC_REGS->EVSYS_CTRLA = 0x01;
	asm("nop");
	asm("nop");
	asm("nop");
	return;
}

//////////////////////////////////////////////////////////////////////////////

//Read Count
int read_count() {
    // Allow read access of COUNT register 
    TC0_REGS->COUNT16.TC_CTRLBSET = ((0x4) << 5);
    return TC0_REGS->COUNT16.TC_COUNT;// Return back the counter value
}

//Initialize TC0
void TC0_Initialize(void){
    /* TC0 Bus Clock */
    GCLK_REGS->GCLK_PCHCTRL[23] = (0x00000042); // Enable TC0 Bus Clock
    while((GCLK_REGS->GCLK_PCHCTRL[23] & (0x00000040)) == 0); // Wait
    
    /* Setting up the TC0 -> CTRLA Register */
    TC0_REGS->COUNT16.TC_CTRLA = (0x01); // Software reset at the start
    while ((TC0_REGS->COUNT16.TC_SYNCBUSY & (0x01)));
    
    TC0_REGS->COUNT16.TC_CTRLA |= ((0x0) << 2); // Set to 16-bit mode
    TC0_REGS->COUNT16.TC_CTRLA |= ((0x1) << 4); // Set the Prescaler and Counter Sync
    
    TC0_REGS->COUNT16.TC_CTRLA |= ((0x07) << 8); // Set the Prescaler Factor
    
    /* Setting up the WAVE Register */
    TC0_REGS->COUNT16.TC_WAVE = (0x01); // Match Frequency Operation mode
    
    /* Setting the Top Value */
    TC0_REGS->COUNT16.TC_CC[0] = 391; // 391 = Set CC0 (Top) value = 100ms
    
    TC0_REGS->COUNT16.TC_CTRLA |= ((0x01) << 1); // Enable the TC0 Peripheral
    while ((TC0_REGS->COUNT16.TC_SYNCBUSY & ((0x01) << 1)));
}

static void NVIC_init(void)
{
	__DMB();
	__enable_irq();
	NVIC_SetPriority(EIC_EXTINT_2_IRQn, 3);
	NVIC_SetPriority(SysTick_IRQn, 3);
	NVIC_EnableIRQ(EIC_EXTINT_2_IRQn);
	NVIC_EnableIRQ(SysTick_IRQn);
	return;
}

//ADC Initialize Function
void ADC_PORT_Initialize(void){
    //LM35 Sensor Pinout: PA06,Pin 10
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[6] |= 0x1;
    PORT_SEC_REGS->GROUP[0].PORT_PMUX[3] = 0x1;

    //MQ-135 Sensor Pinout: PA03, Pin 11
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[2] = 0x1;
    PORT_SEC_REGS->GROUP[0].PORT_PMUX[1] = 0x1;
    
    //PM Sensor Pinout: PA02, Pin 12
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[3] |= 0x1;
    PORT_SEC_REGS->GROUP[0].PORT_PMUX[1] = 0x1;
    
    //Exit the initialization
    return;
}

//////////////////////////////////////////////////////////////////////////////
/* SERCOM(s) Initialization
 * 
 * The following functions are used for:
 * [1] USART Related Initialization
 * [2] I2C Related Initialization
 */

//SERCOM0 UART Initialize Function
void SERCOM0_Initialize(void){
    //Enable the Clock Peripheral
    GCLK_REGS->GCLK_PCHCTRL[17] = 0x00000042;
	while ((GCLK_REGS->GCLK_PCHCTRL[17] & 0x00000040) == 0)
		asm("nop");
    
    //Software Reset Function
	SERCOM0_REGS->USART_INT.SERCOM_CTRLA |= (0x1 << 0);
	while ((SERCOM0_REGS->USART_INT.SERCOM_SYNCBUSY & (0x1 << 0)) != 0)
		asm("nop");
	SERCOM0_REGS->USART_INT.SERCOM_CTRLA = (uint32_t)(0x1 << 2);
    
    //Setting up the USART Settings
    SERCOM0_REGS->USART_INT.SERCOM_CTRLA |= (0x0 << 13)|(0x1 << 30)|(0x0 << 24)|(0x0 << 16)|(0x1 << 20); //Fomerly (0x1 << 24)
	SERCOM0_REGS->USART_INT.SERCOM_CTRLB |= (0x0 << 6) | (0x0 << 0); //Fomerly (0x1 << 0)
    //sercom baud = 65536(1-(16bits*9600bps/4M) = 63020 = 0xF62C 
    SERCOM0_REGS->USART_INT.SERCOM_BAUD = 0xF62C;
    
    //Configure the Physical Pins
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[4] = 0x01; 
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[5] = 0x01; 
    PORT_SEC_REGS->GROUP[0].PORT_PMUX[2] = 0x33;
    
    //Enable the transmitter and receiver
    SERCOM0_REGS->USART_INT.SERCOM_CTRLB |= (0x1 << 17) | (0x1 << 16) | (0x3 << 23);
	while ((SERCOM0_REGS->USART_INT.SERCOM_SYNCBUSY & (0x1 << 2)) != 0)
		asm("nop");
    
    //Enable the peripheral
	SERCOM0_REGS->USART_INT.SERCOM_CTRLA |= (0x1 << 1);
	while ((SERCOM0_REGS->USART_INT.SERCOM_SYNCBUSY & (0x1 << 1)) != 0)
		asm("nop");
    
}

//SERCOM1 UART Initialize Function 
void SERCOM1_Initialize(void){
    //Enable the Clock Peripheral
    GCLK_REGS->GCLK_PCHCTRL[18] = 0x00000042;
	while ((GCLK_REGS->GCLK_PCHCTRL[18] & 0x00000040) == 0)
		asm("nop");
    
    //Software Reset Function
	SERCOM1_REGS->USART_INT.SERCOM_CTRLA |= (0x1 << 0);
	while ((SERCOM1_REGS->USART_INT.SERCOM_SYNCBUSY & (0x1 << 0)) != 0)
		asm("nop");
	SERCOM1_REGS->USART_INT.SERCOM_CTRLA = (uint32_t)(0x1 << 2);
    
    //Setting up the USART Settings
    SERCOM1_REGS->USART_INT.SERCOM_CTRLA |= (0x0 << 13)|(0x1 << 30)|(0x0 << 24)|(0x0 << 16)|(0x1 << 20); //Formerly (0x1 << 24)
	SERCOM1_REGS->USART_INT.SERCOM_CTRLB |= (0x0 << 6); //Formerly (0x1 << 0)
    
    //SERCOM Baud = 65536(1-(16bits*9600bps/4M) = 63020 = 0xF62C 
    SERCOM1_REGS->USART_INT.SERCOM_BAUD = 0xF62C;
    
   //Configure the Physical Pins
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[16] = 0x01;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[17] = 0x01;
    PORT_SEC_REGS->GROUP[0].PORT_PMUX[8] = 0x22; 
    
    //Enable the transmitter and receiver
    SERCOM1_REGS->USART_INT.SERCOM_CTRLB |= (0x1 << 17) | (0x1 << 16) | (0x3 << 23); //Formerly: (0x1 << 17) | (0x1 << 16) | (0x3 << 23)
	while ((SERCOM1_REGS->USART_INT.SERCOM_SYNCBUSY & (0x1 << 2)) != 0)
		asm("nop");
    
    //Enable the peripheral
	SERCOM1_REGS->USART_INT.SERCOM_CTRLA |= (0x1 << 1);
	while ((SERCOM1_REGS->USART_INT.SERCOM_SYNCBUSY & (0x1 << 1)) != 0)
		asm("nop");
    
    //Exit the initialization
    return;
}

//SERCOM3 UART Initialize Function
void SERCOM3_Initialize(void){
    //Enable the Clock Peripheral
    GCLK_REGS->GCLK_PCHCTRL[20] = 0x00000042;
	while ((GCLK_REGS->GCLK_PCHCTRL[20] & 0x00000040) == 0)
		asm("nop");
    
    //Software Reset Function
	SERCOM3_REGS->USART_INT.SERCOM_CTRLA |= (0x1 << 0);
	while ((SERCOM3_REGS->USART_INT.SERCOM_SYNCBUSY & (0x1 << 0)) != 0)
		asm("nop");
	SERCOM3_REGS->USART_INT.SERCOM_CTRLA = (uint32_t)(0x1 << 2);
    
    //Setting up the USART Settings
    SERCOM3_REGS->USART_INT.SERCOM_CTRLA |= (0x0 << 13)|(0x1 << 30)|(0x0 << 24)|(0x0 << 16)|(0x1 << 20); //Formerly (0x1 << 24)
	SERCOM3_REGS->USART_INT.SERCOM_CTRLB |= (0x0 << 6) | (0x0 << 0); //Formerly (0x1 << 0)
    //sercom baud = 65536(1-(16bits*9600bps/4M) = 63020 = 0xF62C 
    SERCOM3_REGS->USART_INT.SERCOM_BAUD = 0xF62C;
    
    //Configure the Physical Pins
    PORT_SEC_REGS->GROUP[1].PORT_PINCFG[8] = 0x03; 
    PORT_SEC_REGS->GROUP[1].PORT_PINCFG[9] = 0x03; 
	PORT_SEC_REGS->GROUP[1].PORT_PMUX[4] = 0x33; 
    
    //Enable the transmitter and receiver
    SERCOM3_REGS->USART_INT.SERCOM_CTRLB |= (0x1 << 17) | (0x1 << 16) | (0x3 << 23);
	while ((SERCOM3_REGS->USART_INT.SERCOM_SYNCBUSY & (0x1 << 2)) != 0)
		asm("nop");
    
    //Enable the peripheral
	SERCOM3_REGS->USART_INT.SERCOM_CTRLA |= (0x1 << 1);
	while ((SERCOM3_REGS->USART_INT.SERCOM_SYNCBUSY & (0x1 << 1)) != 0)
		asm("nop");
    
    //Exit the initialization
    return;
}

/////////////////////////////////////////////////////////////////////////////

void Program_Initialize(void){
	// Raise the power level
	raise_perf_level();
	
	// Early initialization
	EVSYS_init();
	EIC_init_early();
    
    //Regular Initialization
    TC0_Initialize();
    ADC_PORT_Initialize();
    SERCOM0_Initialize();
    SERCOM1_Initialize();
    SERCOM3_Initialize();
    
	// Late initialization
	EIC_init_late();
	NVIC_init();
    
    //Exit the program initialization
	return;
}