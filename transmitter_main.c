    /* 
 * File:   main.c
 * Author: John Clarence Ronquillo
 *
 * Created on May 15, 2025, 4:53 AM
 */

#include <xc.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

//Imported libraries for computational purposes
#include <stdlib.h>
#include <math.h>

//Import function from "transmitter_init.c"
extern void Program_Initialize(void);

//GPS Related Initialization
#define MAX_GPS_FIELDS 13
#define MAX_FIELD_LENGTH 20

//Initialize variable for timer
int time_count = 0;

// Structure to hold parsed GPS data
typedef struct {
    char fields[MAX_GPS_FIELDS][MAX_FIELD_LENGTH];
    int field_count;
} GPS_Data;

//Initialization for ADC Readings
#define CO2_ADC_CHANNEL ADC_INPUTCTRL_MUXPOS_AIN4 // PB18 is AIN18
#define DUST_ADC_CHANNEL ADC_INPUTCTRL_MUXPOS_AIN0   // PA03 is AIN4
#define LM35_ADC_CHANNEL ADC_INPUTCTRL_MUXPOS_AIN1   // PA06 is xAIN1

#define ADC_ACTUAL_REF_VOLTAGE 5.0f 
#define ADC_MAX_VALUE 4095.0f   // 12-bit ADC

#define LM35_MV_PER_DEGREE_C 32.0f // LM35 outputs 10mV per degree Celsius

#define DUST_SENSITIVITY 0.5f  // V/(0.1mg/m^3) from datasheet typical value
#define DUST_OFFSET 0.1f       // Typical output voltage at no dust (0.9V / 0.5 default sensitivity implies 0.18mg/m3, often lower in practice)
                               // Datasheet typical V_oc (voltage output clean air) is ~0.9V. Use this to adjust offset if needed.
                               // If typical V_oc is 0.9V, offset might be 0.9V. Or this DUST_OFFSET is already adjusted.

#define DEW_POINT 23

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
        //Do not send data unless TXC is clear
        while (!(SERCOM3_REGS->USART_INT.SERCOM_INTFLAG & (1 << 0)));
        
        //Send the data
        SERCOM3_REGS->USART_INT.SERCOM_DATA = *message++;
    } 
}

 /* GPS MODULE FUNCTIONS
 * 
 * The following functions are used for reading, parsing and formatting the data from the module. It consists of the following:
 * [1] GPS Receive Function
 * [2] GPS Parsing Function
 * [3] GPS Process Function
 */

// This function is used to GPS read data
void gps_received_msg(char *buffer, uint32_t len) {
    int idx = 0;
    bool is_gpgga = false;
    
    // Read characters until we find the start of a GPRMC message or newline
    while (idx < len - 1) {
        //Do not return the character until RXC is clear
        while(!(SERCOM1_REGS->USART_INT.SERCOM_INTFLAG & (0x1 << 2)));
        
        //Read the data character
        char data = SERCOM1_REGS->USART_INT.SERCOM_DATA;
        
        if (data == '\n') {
            if (is_gpgga) {
                buffer[idx] = '\0'; // Null termination
                return;
            }
            idx = 0; // Reset index
            continue;
        }
        
        // Check for $GPGGA at the start of a new message
        if (idx == 0 && data == '$') {
            //Checking if it's a GPRMC Message
            char next[5];
            for (int i = 0; i < 5; i++) {
                while(!(SERCOM1_REGS->USART_INT.SERCOM_INTFLAG & (0x1 << 2)));
                next[i] = SERCOM1_REGS->USART_INT.SERCOM_DATA;
            }
            
            if (strncmp(next, "GPGGA", 5) == 0) {
                is_gpgga = true;
                buffer[idx++] = '$';
                for (int i = 0; i < 5; i++) {
                    if (idx < len - 1) {
                        buffer[idx++] = next[i];
                    }
                }
                continue;
            } else {
                idx = 0;
                continue;
            }
        }
        
        //Storing the GPRMC Data
        if (is_gpgga && idx < len - 1) {
            buffer[idx++] = data;
        }
        
    }
    
    //Full-buffer condition
    if (is_gpgga) {
        buffer[len - 1] = '\0'; // Null termination
    }
    
    //Exit the function
    return;
}

// Function that parses comma-separated GPS data into array
void parse_gps_data(const char *gps_str, GPS_Data *gps_data) {
    
    // Make a local copy of the string to avoid modifying the original
    char buffer[128];
    strncpy(buffer, gps_str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';  // Ensure null termination
    
    gps_data->field_count = 0;
    char *token = strtok(buffer, ",");
    
    while (token != NULL && gps_data->field_count < MAX_GPS_FIELDS) {
        // Trim leading whitespace
        while (*token == ' ') token++;
        
        // Remove checksum if present (everything after '*')
        char *checksum = strchr(token, '*');
        if (checksum != NULL) *checksum = '\0';
        
        // Trim trailing whitespace (if any)
        char *end = token + strlen(token) - 1;
        while (end > token && *end == ' ') end--;
        *(end + 1) = '\0';
        
        // Only copy if we have content
        if (*token != '\0') {
            // Copy the token to the struct field
            strncpy(gps_data->fields[gps_data->field_count], token, MAX_FIELD_LENGTH - 1);
            gps_data->fields[gps_data->field_count][MAX_FIELD_LENGTH - 1] = '\0';
            
            gps_data->field_count++;
        }
        
        token = strtok(NULL, ",");
    }
    
    //Exit the function
    return;
}

void process_gps_data(const GPS_Data *gps_data, char *output) {
    // Initialize data buffer
    char buffer[128];
    
    // Extract relevant fields
    char *time_read = gps_data->fields[1];
    char *lat_read = gps_data->fields[2];
    char *ns = gps_data->fields[3];
    char *lon_read = gps_data->fields[4];
    char *ew = gps_data->fields[5];
    char *alt_read = gps_data->fields[9];
    char *unt = gps_data->fields[12];
    
    // Default values
    int time = 0;
    double alt = 0.0;
    double lat = 0.0;
    double lon = 0.0;
    
    // Time Formatting
    time = atoi(time_read);
    time = (time > 160000) ? (time + 80000 - 240000) : (time + 80000);

    // Altitude Formatting
    alt = atof(alt_read)/10;

    // Latitude Formatting (convert NMEA to decimal degrees)
    double lat_deg = floor(atof(lat_read)/100);
    double lat_min = atof(lat_read) - (lat_deg*100);
    lat = lat_deg + (lat_min/60);
    if (*ns == 'S') lat = -lat;

    // Longitude Formatting (convert NMEA to decimal degrees)
    double lon_deg = floor(atof(lon_read)/100);
    double lon_min = atof(lon_read) - (lon_deg*100);
    lon = lon_deg + (lon_min/60);
    if (*ew == 'W') lon = -lon;

    // Time output
    char time_str[10];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", 
             time / 10000, (time % 10000) / 100, time % 100);
 
    strcat(output, "Local Time: "); 
    strcat(output, time_str);
    strcat(output, "\n");
    
    // Altitude output
    char alt_str[10];
    snprintf(alt_str, sizeof(alt_str), "%.2f", alt);
    strcat(output, "Altitude: "); 
    strcat(output, alt_str);
    strcat(output, " m\n");
    
    // Latitude output
    strcat(output, "Latitude: "); 
    strcat(output, lat_read);
    strcat(output, " ");
    strcat(output, ns);
    strcat(output, "\n");
    
    // Longitude output 
    strcat(output, "Longitude: "); 
    strcat(output, lon_read);
    strcat(output, " ");
    strcat(output, ew);
    strcat(output, "\n");
    
    // GMaps compatible coordinates
    char maps_str[30];
    snprintf(maps_str, sizeof(maps_str), "%.6f, %.6f", lat, lon);
    strcat(output, "GMaps: "); 
    strcat(output, maps_str);
    strcat(output, "\n");
    
    //Exit the function
    return;
}

/* ADC FUNCTIONS
 * 
 * The following functions are used to implement ADC Readings. It composed of the following
 * [1] ADC Initialize Function
 * [2] ADC Read Channel
 */

//This function is for ADC Readings
void ADC_Initialize(void){
    // GCK0 : Div factor 1 | Source Select 7 | Generic Clock Generator Enable
    GCLK_REGS->GCLK_GENCTRL[0] = (1 << 16) | (7 << 0) | (1 << 8);
    while ((GCLK_REGS->GCLK_SYNCBUSY & (1 << 2)) != 0);

    // ADC Bus Clock : Generic Clock Generator Value | Channel Enable
    GCLK_REGS->GCLK_PCHCTRL[28] = (0 << 0) | (1 << 6); // page 229
    while ((GCLK_REGS->GCLK_PCHCTRL[28] & (1 << 6)) != (1 << 6));
    
    /* Reset ADC */
    ADC_REGS->ADC_CTRLA = (1 << 0); // Reset
    while ((ADC_REGS->ADC_SYNCBUSY & (1 << 0)) == (1 << 0))
        ;

    /* Prescaler */
    ADC_REGS->ADC_CTRLB = (2 << 0); // Peripheral clock divided by 8 (page 1215)

    /* Sampling length */
    ADC_REGS->ADC_SAMPCTRL = (3 << 0); // ADC Sample time Tsamp = SAMPLEN+1 ? TAD (page 1227)

    /* Reference */
    ADC_REGS->ADC_REFCTRL = (2 << 0); // REFSEL = AVDD (page 1216)

    /* Resolution & Operation Mode */
    ADC_REGS->ADC_CTRLC = (uint16_t)((0 << 4) | (0 << 8)); // Conversion Result Resolution: 12, Window Monitor Mode: Disabled (page 1224)

    /* Clear all interrupt flags */
    ADC_REGS->ADC_INTFLAG = (uint8_t)0x07; // clears the Window Monitor, Overrun, Result Ready interrupt flag
    while (0U != ADC_REGS->ADC_SYNCBUSY);
    
    /*ADC Enable*/
    ADC_REGS->ADC_CTRLA |= (1 << 1);
    while (0U != ADC_REGS->ADC_SYNCBUSY);
    
    //Exit
    return;
}

static uint16_t ADC_Read_Channel(uint32_t AIN_Channel){
    // Select ADC channel by setting MUXPOS in INPUTCTRL
    ADC_REGS->ADC_INPUTCTRL = AIN_Channel;
    while (ADC_REGS->ADC_SYNCBUSY & ADC_SYNCBUSY_INPUTCTRL_Msk);
    
    // Start ADC conversion by software trigger
    ADC_REGS->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    // No SYNCBUSY for SWTRIG itself, but conversion takes time.
    
    // Wait for the conversion to be complete (Result Ready flag)
    while (!(ADC_REGS->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk));
    
    // Read the ADC result
    uint16_t result = ADC_REGS->ADC_RESULT;

    // Clear the Result Ready flag by writing a '1' to it (optional if only polling)
    ADC_REGS->ADC_INTFLAG = ADC_INTFLAG_RESRDY_Msk; 
    
    return result;
}
 
 /* HC12 MODULE FUNCTIONS
 * 
 * The following functions are used for reading and sending data to the module. It consists of the following:
 * [1] HC12 Send Message
 * [2] HC12 Receive Message
 */
 
 //This function is used to send messages to the HC12 Module
static void hc12_send_msg(const char *message){
    
    print_terminal(message);
    
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
static void hc12_rcvd_msg(char *message, uint32_t len){
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

void main_program() {
    // Buffers for storing readings
    char gps_read_str[128] = {0};
    char c02_read_str[32] = {0};
    char pm_read_str[32] = {0};
    char temp_read_str[32] = {0};
    char humid_read_str[32] = {0};
    char output_msg[256] = {0};
    
    // Flags for synchronization
    bool gps_on = false;
    bool co2_on = false;
    bool pm_on = false;
    bool temp_on = false;
    bool alt_on = false;
    
    //GPS Data Structure Initialization
    GPS_Data gps_data;
    
    //Protocol Header;
    strcat(output_msg, "[D.L~N~R]\n");
   
    //MQ-135 Sensor Data Readings    
    do{
        int c02_adc_read = ADC_Read_Channel(CO2_ADC_CHANNEL);
        double c02_voltage = c02_adc_read*(ADC_ACTUAL_REF_VOLTAGE / ADC_MAX_VALUE);
        double c02_read = (c02_voltage*100) + 400;
        
        snprintf(c02_read_str, sizeof(c02_read_str), "C02 Readings: %.3f PPM\n", c02_read);
        strcat(output_msg, c02_read_str);
        
        //For Synchronization of Data
        co2_on = true;

        //Exit
        break;
        
    }while(0); 
    
    //LM35 Sensor Data Readings
    do{
        int temp_adc_read = ADC_Read_Channel(LM35_ADC_CHANNEL);
        float temp_voltage = temp_adc_read * (ADC_ACTUAL_REF_VOLTAGE / ADC_MAX_VALUE) ;
        float temp_read = (temp_voltage * 1000.0f) / LM35_MV_PER_DEGREE_C;
        
        snprintf(temp_read_str, sizeof(temp_read_str), "Temperature Readings: %.3f  C", temp_read);     
        strcat(output_msg, temp_read_str);
        strcat(output_msg, "\n");
        
        //Humidity Sensor Readings
        float humid_read = 100*(exp((17.675*DEW_POINT)/(234.04+DEW_POINT))/exp((17.675*temp_read)/(234.04+temp_read)));
        
        snprintf(humid_read_str, sizeof(humid_read_str), "Humidity Reading: %.2f % \n", humid_read);        ;
        strcat(output_msg, humid_read_str);  
        
        //For synchronization of data
        temp_on = true;
        
        //Exit
        break;
        
    }while(0);
    
    //PM2.5 Sensor Data Readings    
    do{
        int pm_adc_read = ADC_Read_Channel(DUST_ADC_CHANNEL);
        double pm_voltage = pm_adc_read * (ADC_ACTUAL_REF_VOLTAGE / ADC_MAX_VALUE);
        double pm_read = ((pm_voltage - DUST_OFFSET) / DUST_SENSITIVITY * 0.1f) *0.046888f;
        
        if (pm_read < 0.0f){
            pm_read = 0.0f;
        }
        
        
        snprintf(pm_read_str, sizeof(pm_read_str), "PM Readings: %.8f mg/m^3\n", pm_read);        
        strcat(output_msg, pm_read_str);
        
        //For synchronization of data
        pm_on = true;
        
        //Exit
        break;
        
    }while(0);   

    //GY-NE06MV2 Readings
    do{
        
        gps_received_msg(gps_read_str, sizeof(gps_read_str));
        
        // Parse the GPS data into structured format
        parse_gps_data(gps_read_str, &gps_data);

        // Process and format the GPS data
        process_gps_data(&gps_data, output_msg);
        
        //For synchronization of data
        gps_on = true;
        
        //Exit
        break;
        
    }while(0);

        
    //Send the data 
    hc12_send_msg(output_msg);
    
    
    //Send the data readings
    if (gps_on && co2_on && pm_on && temp_on) { 
        
        //Add another newline
        strcat(output_msg, "\n");
        
        //Send the data 
        hc12_send_msg(output_msg);
    }
     
      
    //Exit the function
    return;
}

/////////////////////////////////////////////////////////////////////////////

// main() -- the heart of the program
int main(void) {
    
    //Initialize Function
    Program_Initialize();
    
    //ADC Initialization
    ADC_Initialize();
    
    print_terminal("Program Initialize for the Transmitter...\r\n");
    
    for (;;){
        main_program();       
    }
    
    // This line must never be reached    
    return 1;
}