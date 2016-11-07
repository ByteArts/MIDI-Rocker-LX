#ifndef _INC_JOYSTICK
#define _INC_JOYSTICK

/*
USB HID Report Constants
*/

// HID joystick button bit flags
/*
Older versions of the firmware had the button flags in two separate bytes, while newer versions put
them all into one word.
*/
// first (lower) byte
#define joyBUTTON_1		0x01
#define joyBUTTON_2		0x02
#define joyBUTTON_3		0x04
#define joyBUTTON_4		0x08
#define joyBUTTON_5		0x10
#define joyBUTTON_6		0x20
#define joyBUTTON_7		0x40
#define joyBUTTON_8		0x80

// second (upper) byte
#if defined(BYTE_BUTTON_FLAG) 
	// 2nd byte
	#define joyBUTTON_9		0x01
	#define joyBUTTON_10	0x02
	#define joyBUTTON_11	0x04
	#define joyBUTTON_12	0x08
	#define joyBUTTON_13	0x10
#else
	// upper byte of word
	#define joyBUTTON_9		0x0100
	#define joyBUTTON_10	0x0200
	#define joyBUTTON_11	0x0400
	#define joyBUTTON_12	0x0800
	#define joyBUTTON_13	0x1000
#endif

// axis, hat switch positions
#define joyHAT_UP		0x00
#define joyHAT_RIGHT	0x02 
#define joyHAT_DOWN		0x04
#define joyHAT_LEFT		0x06 

#if defined(GUITAR_HERO)
	#define joyHAT_CENTER	0x0F
#else
	#define joyHAT_CENTER	0x08
#endif


#if defined(GUITAR_HERO) 
	#define joyAXIS_CENTER	0x80 
#else
	#define joyAXIS_CENTER	0x7F 
#endif


#endif
