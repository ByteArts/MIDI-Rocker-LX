/* Worldwide defines for Universal PCB */
#ifndef _INC_PINOUT
#define _INC_PINOUT

/*
UPCB - Universal Programmed Controller Board 
Copyright (C) 2007  Marcus Post marcus@marcuspost.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#define INPUT 				1
#define OUTPUT				0

// A/D converter controls
#define AD_CH0	0x00	
#define AD_CH1	0x04	
#define AD_CH2	0x80
#define AD_ENABLE 0x01
#define AD_GO	0x02


/** I/O PINS *****************************************************/

#if defined(MR_LX)
	
	// game controller buttons
	#define swNAV_UP		PORTEbits.RE1  // IN5						
	#define swNAV_DOWN		PORTAbits.RA5  // IN3
	#define swNAV_LEFT		PORTEbits.RE0  // IN4
	#define swNAV_RIGHT		PORTCbits.RC0  // IN7
	#define swNAV_CENTER	PORTEbits.RE2  // IN6

	#define swBACK			PORTAbits.RA4  // IN2 (PB2)
	#define swSTART			PORTAbits.RA3  // IN1 (PB1)
	
	#define swEXT_PEDAL		PORTBbits.RB7  // external pedal jack
	
	// outputs
	#define ledCH1		PORTBbits.RB3
	#define ledCH2		PORTBbits.RB2
	#define ledCH3		PORTBbits.RB1
	#define ledCH4		PORTBbits.RB0
	#define ledCH5		PORTDbits.RD7
	#define ledCH6		PORTDbits.RD6
	#define ledALT		PORTDbits.RD5
	#define ledM1		PORTDbits.RD4
	#define ledM2		PORTDbits.RD3
	#define ledPROG		PORTDbits.RD2

	// spare I/O (on LXi)
	#define outEXT0		PORTAbits.RA0 
	#define outEXT1		PORTAbits.RA1 
	#define outEXT2		PORTAbits.RA2 
	#define outEXT3		PORTCbits.RC1 
	#define outEXT4		PORTCbits.RC2 
	#define outEXT5		PORTDbits.RD0 
	#define outEXT6		PORTDbits.RD1 
	#define outEXT7		PORTBbits.RB6 
	
	
	// LED Outputs
	#define ledUSB		PORTBbits.RB4  // OUT11
	#define	ledMIDI		PORTBbits.RB5  // OUT12

#else // regular MIDI Rocker
	
	// game controller buttons
	#define swNAV_UP		PORTDbits.RD1  
	#define swNAV_DOWN		PORTDbits.RD2  
	#define swNAV_LEFT		PORTDbits.RD3  
	#define swNAV_RIGHT		PORTDbits.RD4  
	#define swNAV_CENTER	PORTBbits.RB3
	
	#define swSTART		PORTCbits.RC2  // PB1
	#define swBACK		PORTDbits.RD0  // PB2
	#define swHOME		PORTCbits.RC1  // PB3

	#define swEXT_PEDAL		PORTCbits.RC0  // external pedal jack

	// drum buttons
	#define swDRUM1		PORTAbits.RA3
	#define swDRUM2     PORTAbits.RA4
	#define swDRUM3     PORTAbits.RA5
	#define swDRUM4     PORTEbits.RE0
	#define swDRUM5     PORTEbits.RE1
	#define swDRUM6		PORTEbits.RE2  // not a real switch -- it's a external input bit
	#define swDRUM7		PORTCbits.RC0  // not a real switch -- it's a external input bit

	// Function select switch
	#define swFUNCTION	PORTAbits.RA2  

	// outputs
	#define ledCH1		PORTDbits.RD5
	#define ledCH2		PORTDbits.RD6
	#define ledCH3		PORTDbits.RD7
	#define ledCH4		PORTBbits.RB0
	#define ledCH5		PORTBbits.RB1
	#define outEXT6		PORTBbits.RB2
	#define outEXT7		PORTBbits.RB6

	// LED Outputs
	#define ledUSB		PORTBbits.RB4  
	#define	ledMIDI		PORTBbits.RB5  

#endif

#define mMIDI_RX		PORTCbits.RC7
	
#define LED_OUTPUT_ON	0
#define LED_OUTPUT_OFF	1

#define SW_PRESSED 0U
#define SW_NOT_PRESSED 1U



#endif
