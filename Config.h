#ifndef _INC_CONFIG_H
#define _INC_CONFIG_H
/*
Processor configuration
*/

#if defined(PICDEM_FS_USB) || defined(MIDI_ROCKER)   // PIC18F4550 is used
    #pragma config PLLDIV = 5       // (20 MHz input)
    #pragma config CPUDIV = OSC1_PLL2
    #pragma config USBDIV = 2       // Clock source from 96MHz PLL/2
    #pragma config FOSC = HSPLL_HS
    //#pragma config FCMEN = OFF 	// fail-safe clock monitor
    #pragma config IESO = OFF 		// Internal/External Switch Over
    #if defined(__DEBUG)
	    #pragma config PWRT = OFF 		// power up timer
    #else
	    #pragma config PWRT = ON 		// power up timer
	#endif
    #pragma config BOR = ON 		// brown out reset
    #pragma config BORV = 1		    // brown out voltage (1 means 4.2V, 0 means 4.5V)
    #pragma config VREGEN = ON		// USB Voltage Regulator
    #pragma config WDT = OFF		// watchdog timer
    #pragma config WDTPS = 32768	// watchdog prescaler
    #pragma config MCLRE = ON		// MCLR enable
    #pragma config LPT1OSC = OFF	// Low Power Timer1 Oscillator Enable
    #pragma config PBADEN = OFF		// PORTB A/D Enable
    #pragma config STVREN = ON		// stack overflow reset
    #pragma config LVP = OFF		// Low Voltage ICSP
    #pragma config ICPRT = OFF      // Dedicated In-Circuit Debug/Programming
    #pragma config XINST = OFF      // Extended Instruction Set
    #pragma config WRT0 = OFF		// Write Protection Block 0
    #pragma config WRT1 = OFF		// Write Protection Block 1
    #if defined(__DEBUG)
    	#pragma config WRTB = OFF    	// Boot Block Write Protection
    #else
    	#pragma config WRTB = ON    	// Boot Block Write Protection
    #endif
    #pragma config WRTC = OFF		// Configuration Register Write Protection
    #pragma config CP0 = OFF		// Code Protection Block 0
    #pragma config CP1 = OFF		// Code Protection Block 1
    #pragma config CPB = OFF		// Boot Block Code Protection
#else
    #error No hardware board defined, see "HardwareProfile.h" and __FILE__
#endif

#endif // ifndef _INC_CONFIG_H