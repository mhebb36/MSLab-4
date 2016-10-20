//------------------------------------------------------------------------------------
// Hello.c
//------------------------------------------------------------------------------------
//8051 Test program to demonstrate serial port I/O.  This program writes a message on
//the console using the printf() function, and reads characters using the getchar()
//function.  An ANSI escape sequence is used to clear the screen if a '2' is typed. 
//A '1' repeats the message and the program responds to other input characters with
//an appropriate message.
//
//Any valid keystroke turns on the green LED on the board; invalid entries turn it off
//------------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------------
#include <c8051f120.h>
#include <stdio.h>
#include "putget.h"

//------------------------------------------------------------------------------------
// Global Constants
//------------------------------------------------------------------------------------
#define EXTCLK      22118400            // External oscillator frequency in Hz
#define SYSCLK      49766400            // Output of PLL derived from (EXTCLK * 9/4)
#define BAUDRATE    115200              // UART baud rate in bps

//------------------------------------------------------------------------------------
// Function Prototypes
//------------------------------------------------------------------------------------
void main(void);
void SYSCLK_INIT(void);
void PORT_INIT(void);
void UART0_INIT(void);
void ADC_Init(void);

void Poll_ADC(void);
void Display_ADC(void);

// Global variables
char SFRPAGE_SAVE;
unsigned short int ADC_result;
float ADC_voltage;
unsigned short int ADC_max;
unsigned short int ADC_min;
unsigned short int ADC_average;
unsigned char num_trials;

//------------------------------------------------------------------------------------
// MAIN Routine
//------------------------------------------------------------------------------------
void main(void)
{
    WDTCN = 0xDE;                       // Disable the watchdog timer
    WDTCN = 0xAD;

    PORT_INIT();                        // Initialize the Crossbar and GPIO
    SYSCLK_INIT();                      // Initialize the oscillator
    UART0_INIT();                       // Initialize UART0
	ADC_Init();

    SFRPAGE = UART0_PAGE;               // Direct output to UART0

    printf("\033[2J");                  // Erase screen & move cursor to home position
    
    while(1)
    {
		printf("Ground P1.0 to start ADC...\r\n");
		Poll_ADC();
		Display_ADC();
    }
}

//------------------------------------------------------------------------------------
// SYSCLK_Init
//------------------------------------------------------------------------------------
//
// Initialize the system clock to use a 22.1184MHz crystal as its clock source
//
void SYSCLK_INIT(void)
{
    int i;
    char SFRPAGE_SAVE;

    SFRPAGE_SAVE = SFRPAGE;             // Save Current SFR page

    SFRPAGE = CONFIG_PAGE;
    OSCXCN  = 0x67;                     // Start ext osc with 22.1184MHz crystal
    for(i=0; i < 256; i++);             // Wait for the oscillator to start up
    while(!(OSCXCN & 0x80));
    CLKSEL  = 0x01;
    OSCICN  = 0x00;

    SFRPAGE = CONFIG_PAGE;
    PLL0CN  = 0x04;
    SFRPAGE = LEGACY_PAGE;
    FLSCL   = 0x10;
    SFRPAGE = CONFIG_PAGE;
    PLL0CN |= 0x01;
    PLL0DIV = 0x04;
    PLL0FLT = 0x01;
    PLL0MUL = 0x09;
    for(i=0; i < 256; i++);
    PLL0CN |= 0x02;
    while(!(PLL0CN & 0x10));
    CLKSEL  = 0x02;

    SFRPAGE = SFRPAGE_SAVE;             // Restore SFR page
}

//------------------------------------------------------------------------------------
// PORT_Init
//------------------------------------------------------------------------------------
//
// Configure the Crossbar and GPIO ports
//
void PORT_INIT(void)
{    
    char SFRPAGE_SAVE;

    SFRPAGE_SAVE = SFRPAGE;             // Save Current SFR page

    SFRPAGE  = CONFIG_PAGE;
    XBR0     = 0x04;                    // Enable UART0
    XBR1     = 0x00;
    XBR2     = 0x40;                    // Enable Crossbar and weak pull-up
    P0MDOUT |= 0x01;                    // Set TX0 on P0.0 pin to push-pull
    P1MDOUT |= 0x40;                    // Set green LED output P1.6 to push-pull

    SFRPAGE  = SFRPAGE_SAVE;            // Restore SFR page
}

//------------------------------------------------------------------------------------
// UART0_Init
//------------------------------------------------------------------------------------
//
// Configure the UART0 using Timer1, for <baudrate> and 8-N-1
//
void UART0_INIT(void)
{
    char SFRPAGE_SAVE;

    SFRPAGE_SAVE = SFRPAGE;             // Save Current SFR page

    SFRPAGE = TIMER01_PAGE;
    TMOD   &= ~0xF0;
    TMOD   |=  0x20;                    // Timer1, Mode 2, 8-bit reload
    TH1     = -(SYSCLK/BAUDRATE/16);    // Set Timer1 reload baudrate value T1 Hi Byte
    CKCON  |= 0x10;                     // Timer1 uses SYSCLK as time base
    TL1     = TH1;
    TR1     = 1;                        // Start Timer1

    SFRPAGE = UART0_PAGE;
    SCON0   = 0x50;                     // Mode 1, 8-bit UART, enable RX
    SSTA0   = 0x10;                     // SMOD0 = 1
    TI0     = 1;                        // Indicate TX0 ready

    SFRPAGE = SFRPAGE_SAVE;             // Restore SFR page
}

//------------------------------------------------------------------------------------
// ADC_Init
//------------------------------------------------------------------------------------
//
// Configure ADC0 to operate in single-ended mode

void ADC_Init(void)
{
    SFRPAGE   = ADC0_PAGE;
    ADC0CN    = 0x80;
	REF0CN    = 0x03; // Enable internal voltage reference and buffer
}

void Poll_ADC(void)
{
	unsigned char *low_byte;
	unsigned char *high_byte;
	low_byte = (char *) &ADC_result;
	high_byte = low_byte + 1;
	SFRPAGE_SAVE = SFRPAGE;
	// Poll pin 1.0 until it is low
	while (P1 == 0xFF);
	SFRPAGE = ADC0_PAGE;
	// Clear AD0INT to start ADC
	AD0INT = 0;
	AD0BUSY = 1;
	while (!AD0INT) // Wait for conversion to complete
	SFRPAGE = SFRPAGE_SAVE;
	*low_byte = ADC0L;
	*high_byte = ADC0H;
}

void Display_ADC(void)
{
	int i;
	unsigned short int total = 0;
	unsigned short int readings[16];

	SFRPAGE_SAVE = SFRPAGE;
	// Convert ADC_result to a voltage
	ADC_voltage = ADC_result / 4096.0 * 2.68;
	SFRPAGE = UART0_PAGE;
	num_trials++;
	// Find max, min, and average
	if (ADC_result > ADC_max || num_trials == 1)
		ADC_max = ADC_result;
	if (ADC_result < ADC_min || num_trials == 1)
		ADC_min = ADC_result;


	readings[(num_trials-1) % 16] = ADC_result;
	// Sum of last 16 trials (or total trials if less than 16)
	if (num_trials < 16)
	{
		for (i=0; i<num_trials; i++)
		{
			total += readings[i];
		}
		printf("%d, %d\r\n", readings[0], num_trials);
		ADC_average = total / num_trials;
	}
	else
	{
		for (i=0; i<16; i++)
		{
			total += readings[i];
		}
		ADC_average = total / 16;
	}


	printf_fast_f("Current voltage reading: %f\r\n", ADC_voltage);
	printf("High ADC reading: 0x%x\r\n", ADC_max);
	printf("Low ADC reading: 0x%x\r\n", ADC_min);
	printf("Average of last 16 trials: 0x%x\r\n", ADC_average);


	// Make sure pin 1.0 is not low
	while (P1 == 0xFE);
	SFRPAGE = SFRPAGE_SAVE;
}