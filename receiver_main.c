/* 
 * File:   receiver_main.c
 * Author: John Clarence Ronquillo
 *
 * Created on May 16, 2025, 5:35 AM
 */

#include <xc.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

//Import function from "transmitter_init.c"
extern void Program_Initialize(void);

//Variables for timer purposes
int time_count = 0;
int time_delay = 8000;
static int idx = 0;

//Variable for receiving message
static char hc12_rcvd_msg[128];
static uint16_t status = 0x0000;
//static uint8_t  data = 0x00;

/////////////////////////////////////////////////////////////////////////////

//This function will be used for time delay purposes
static void nsec_delay(int n_delay){
    time_count = 0;
    
    while (1){
        if(read_count() > TC0_REGS->COUNT16.TC_CC[0]/2){
            time_count++;
        }

        if (time_count > n_delay){
            return;
        }        
    }
}

//This function will be used for debugging purposes
static void print_terminal(const char *message) {
    if (message == NULL) return;
    
    // TX Handling
    while (*message) {
        while (!(SERCOM3_REGS->USART_INT.SERCOM_INTFLAG & (1 << 0)))
            asm("nop");
        
        SERCOM3_REGS->USART_INT.SERCOM_DATA = *message++;
    } 
}

 //This function is used to send messages to the HC12 Module
static void hc12_send_msg(const char *message){
    if (message == NULL) return;
    
    // TX Handling
    while (*message) {
        while (!(SERCOM0_REGS->USART_INT.SERCOM_INTFLAG & (1 << 0)));
        SERCOM0_REGS->USART_INT.SERCOM_DATA = *message++;
    }
    
    //Exit
    return;
}

//This function is used to receive messages from the HC12 Module
static void hc12_received_msg(char *message, uint32_t len){
    int rcvd_idx = 0;
    
    while (rcvd_idx < len - 1){
        //Do not return the character until RXC is clear
        while(!(SERCOM0_REGS->USART_INT.SERCOM_INTFLAG & (0x1 << 2)));
        
        //Append to the message buffer
        message[rcvd_idx++] = SERCOM1_REGS->USART_INT.SERCOM_DATA;  
    }
    
    //Print Terminal
    print_terminal(message);
    
    //Exit
    return;
}

/////////////////////////////////////////////////////////////////////////////

// main() -- the heart of the program
int main(void) {
    
    //Initialize Function
    Program_Initialize();
    print_terminal("Program Initialize for the Receiver...\r\n");    
    
    while (1){
        while(1){
            // Clear the flags first before reading the data
            if ((SERCOM0_REGS->USART_INT.SERCOM_INTFLAG & (0x1 << 2)) != 0) { 
                status = SERCOM0_REGS->USART_INT.SERCOM_STATUS | 0x8000; // Clearing the flags
            }

            if (status & 0x8005){
                while(!(SERCOM0_REGS->USART_INT.SERCOM_INTFLAG & (0x1 << 2)));
                
                char data = SERCOM0_REGS->USART_INT.SERCOM_DATA;   
                
                if (data == "\n") { // New Line or Maximum Length 
                    //Add New Line at the end of the string
                    SERCOM3_REGS->USART_INT.SERCOM_DATA = 10;
                    
                    //Print the message
                    print_terminal(hc12_rcvd_msg);
                    
                    //Reset the state variables
                    memset(hc12_rcvd_msg, 0, sizeof(hc12_rcvd_msg));
                    idx = 0;
       
                    //Exit
                    break;
                    }
                
                SERCOM3_REGS->USART_INT.SERCOM_DATA = data;
                idx++;
                
            }            
        }
    }
    //It should not reach this line    
    return 1;
 }

