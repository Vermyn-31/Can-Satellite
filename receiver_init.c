/* 
 * File:   receiverx_init.c
 * Author: John Clarence Ronquillo
 *
 * Created on May 26, 2025, 1:46 AM
 */

#include <xc.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/////////////////////////////////////////////////////////////////////////////

// Enable higher frequencies for higher performance
static void raise_perf_level(void)
{
	uint32_t tmp_reg = 0;
	
	/*
	 * The chip starts in PL0, which emphasizes energy efficiency over
	 * performance. However, we need the latter for the clock frequency
	 * we will be using (~24 MHz); hence, switch to PL2 before continuing.
	 */
	PM_REGS->PM_INTFLAG = 0x01;
	PM_REGS->PM_PLCFG = 0x02;
	while ((PM_REGS->PM_INTFLAG & 0x01) == 0)
		asm("nop");
	PM_REGS->PM_INTFLAG = 0x01;
	
	/*
	 * Power up the 48MHz DFPLL.
	 * 
	 * On the Curiosity Nano Board, VDDPLL has a 1.1uF capacitance
	 * connected in parallel. Assuming a ~20% error, we have
	 * STARTUP >= (1.32uF)/(1uF) = 1.32; as this is not an integer, choose
	 * the next HIGHER value.
	 */
	NVMCTRL_SEC_REGS->NVMCTRL_CTRLB = (2 << 1) ;
	SUPC_REGS->SUPC_VREGPLL = 0x00000302;
	while ((SUPC_REGS->SUPC_STATUS & (1 << 18)) == 0)
		asm("nop");
	
	/*
	 * Configure the 48MHz DFPLL.
	 * 
	 * Start with disabling ONDEMAND...
	 */
	OSCCTRL_REGS->OSCCTRL_DFLLCTRL = 0x0000;
	while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0)
		asm("nop");
	
	/*
	 * ... then writing the calibration values (which MUST be done as a
	 * single write, hence the use of a temporary variable)...
	 */
	tmp_reg  = ((uint32_t)0x00806020);
	tmp_reg &= ((uint32_t)(0b111111) << 25);
	tmp_reg >>= 15;
	tmp_reg |= ((512 << 0) & 0x000003ff);
	OSCCTRL_REGS->OSCCTRL_DFLLVAL = tmp_reg;
	while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0)
		asm("nop");
	
	// ... then enabling ...
	OSCCTRL_REGS->OSCCTRL_DFLLCTRL |= 0x0002;
	while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0)
		asm("nop");
	
	// ... then restoring ONDEMAND.
//	OSCCTRL_REGS->OSCCTRL_DFLLCTRL |= 0x0080;
//	while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0)
//		asm("nop");
	
	/*
	 * Configure GCLK_GEN2 as described; this one will become the main
	 * clock for slow/medium-speed peripherals, as GCLK_GEN0 will be
	 * stepped up for 24 MHz operation.
	 */
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

/*
 * Configure the EIC peripheral
 * 
 * NOTE: EIC initialization is split into "early" and "late" halves. This is
 *       because most settings within the peripheral cannot be modified while
 *       EIC is enabled.
 */
static void EIC_init_early(void)
{
	/*
	 * Enable the APB clock for this peripheral
	 * 
	 * NOTE: The chip resets with it enabled; hence, commented-out.
	 * 
	 * WARNING: Incorrect MCLK settings can cause system lockup that can
	 *          only be rectified via a hardware reset/power-cycle.
	 */
	// MCLK_REGS->MCLK_APBAMASK |= (1 << 10);
	
	/*
	 * In order for debouncing to work, GCLK_EIC needs to be configured.
	 * We can pluck this off GCLK_GEN2, configured for 4 MHz; then, for
	 * mechanical inputs we slow it down to around 15.625 kHz. This
	 * prescaling is OK for such inputs since debouncing is only employed
	 * on inputs connected to mechanical switches, not on those coming from
	 * other (electronic) circuits.
	 * 
	 * GCLK_EIC is at index 4; and Generator 2 is used.
	 */
	GCLK_REGS->GCLK_PCHCTRL[4] = 0x00000042;
	while ((GCLK_REGS->GCLK_PCHCTRL[4] & 0x00000042) == 0)
		asm("nop");
	
	// Reset, and wait for said operation to complete.
	EIC_SEC_REGS->EIC_CTRLA = 0x01;
	while ((EIC_SEC_REGS->EIC_SYNCBUSY & 0x01) != 0)
		asm("nop");
	
	/*
	 * Just set the debounce prescaler for now, and leave the EIC disabled.
	 * This is because most settings are not editable while the peripheral
	 * is enabled.
	 */
	EIC_SEC_REGS->EIC_DPRESCALER = (0b0 << 16) | (0b0000 << 4) |
		                       (0b1111 << 0);
	return;
}
static void EIC_init_late(void)
{
	/*
	 * Enable the peripheral.
	 * 
	 * Once the peripheral is enabled, further configuration is almost
	 * impossible.
	 */
	EIC_SEC_REGS->EIC_CTRLA |= 0x02;
	while ((EIC_SEC_REGS->EIC_SYNCBUSY & 0x02) != 0)
		asm("nop");
	return;
}

// Configure the EVSYS peripheral
static void EVSYS_init(void)
{
	/*
	 * Enable the APB clock for this peripheral
	 * 
	 * NOTE: The chip resets with it enabled; hence, commented-out.
	 * 
	 * WARNING: Incorrect MCLK settings can cause system lockup that can
	 *          only be rectified via a hardware reset/power-cycle.
	 */
	// MCLK_REGS->MCLK_APBAMASK |= (1 << 0);
	
	/*
	 * EVSYS is always enabled, but may be in an inconsistent state. As
	 * such, trigger a reset.
	 */
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

/*
 * Configure the NVIC
 * 
 * This must be called last, because interrupts are enabled as soon as
 * execution returns from this function.
 */
static void NVIC_init(void)
{
	/*
	 * Unlike AHB/APB peripherals, the NVIC is part of the Arm v8-M
	 * architecture core proper. Hence, it is always enabled.
	 */
	__DMB();
	__enable_irq();
	NVIC_SetPriority(EIC_EXTINT_2_IRQn, 3);
	NVIC_SetPriority(SysTick_IRQn, 3);
	NVIC_EnableIRQ(EIC_EXTINT_2_IRQn);
	NVIC_EnableIRQ(SysTick_IRQn);
	return;
}

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
	PORT_SEC_REGS->GROUP[0].PORT_PMUX[(4 >> 1)] = (0x33); 
    
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
    //sercom baud = 65536(1-(16bits*9600bps/4M) = 63020 = 0xF62C 
    SERCOM1_REGS->USART_INT.SERCOM_BAUD = 0xF62C;
    
    //Configure the Physical Pins
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[16] = 0x01; 
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[17] = 0x01; 
	PORT_SEC_REGS->GROUP[0].PORT_PMUX[(16 >> 1)] = (0x22); 
    
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
    PORT_SEC_REGS->GROUP[1].PORT_PINCFG[8] = 0x01; 
    PORT_SEC_REGS->GROUP[1].PORT_PINCFG[9] = 0x01; 
	PORT_SEC_REGS->GROUP[1].PORT_PMUX[(9 >> 1)] = (0x33); 
    
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
    SERCOM0_Initialize();
    SERCOM1_Initialize();
    SERCOM3_Initialize();
    
	// Late initialization
	EIC_init_late();
	NVIC_init();
    
    //Exit the program initialization
	return;
}

