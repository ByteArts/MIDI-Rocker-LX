#include "EEData.h"
#include <p18cxxx.h>

/*
Read a byte from EEPROM. This code is copied from read_B.c in the C18 library
*/
BYTE ReadEEData(BYTE Address)
{
	// wait for any previous write to finish
	while (EECON1bits.WR)
		; // do nothing

	EEADR = Address; // set address
	EECON1bits.EEPGD = 0; // point to data memory
  	EECON1bits.CFGS = 0; // access eeprom
	EECON1bits.RD = 1; // do read

	return (EEDATA); // read value
}


/*
Write to EEPROM. Waits for the write to complete before returning. This code comes 
from write_B.c in the C18 library. 
*/
void WriteEEData(BYTE Address, BYTE Data)
{
	// wait for any previous write to finish
	while (EECON1bits.WR)
		; // do nothing

	EEADR = Address; // address to write
  	EEDATA = Data; // value to write
  	EECON1bits.EEPGD = 0; // point to DATA memory
	EECON1bits.CFGS = 0; // access EEPROM
	EECON1bits.WREN = 1; // enable writes
	INTCONbits.GIE = 0; // disable interrupts
	EECON2 = 0x55;		// required sequence #1 
	EECON2 = 0xAA;		// #2
	EECON1bits.WR = 1;	// initiate write
	INTCONbits.GIE = 1;	// restore interrupts
	EECON1bits.WREN = 0;// disable write to EEPROM
}

