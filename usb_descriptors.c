/********************************************************************
 FileName:     usb_descriptors.c
 Dependencies: See INCLUDES section
 Processor:    PIC18 USB Microcontrollers
 Hardware:
 Complier:     Microchip C18
 Company:		Microchip Technology, Inc.

 Software License Agreement:

 The software supplied herewith by Microchip Technology Incorporated
 (the “Company”) for its PICmicro® Microcontroller is intended and
 supplied to you, the Company’s customer, for use solely and
 exclusively on Microchip PICmicro Microcontroller products. The
 software is owned by the Company and/or its supplier, and is
 protected under applicable copyright laws. All rights are reserved.
 Any use in violation of the foregoing restrictions may subject the
 user to criminal sanctions under applicable laws, as well as to
 civil liability for the breach of the terms and conditions of this
 license.

 THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.

********************************************************************
 File Description:

 Change History:
  Rev   Date         Description
  1.0   11/19/2004   Initial release
  2.1   02/26/2007   Updated for simplicity and to use common
                     coding style
        5/6/08       Modified for generic HID. (Jan Axelson)
*******************************************************************/

/*********************************************************************
 * Descriptor specific type definitions are defined in: usbd.h
 * Configuration information is defined in: usbcfg.h
 ********************************************************************/

#ifndef __USB_DESCRIPTORS_C
#define __USB_DESCRIPTORS_C

/** INCLUDES *******************************************************/
#include "GenericTypeDefs.h"
#include "Compiler.h"
#include "usb_config.h"
#include "USB/usb_device.h"
#include "App.h" // version info

/** CONSTANTS ******************************************************/
#if defined(__18CXX)
#pragma romdata
#endif

#define WORD_BYTES(w)	(BYTE)((w) & 0xFF), (BYTE)((w) >> 8)

/* Device Descriptor */
#if defined(USB_DESCRIPTOR_IN_RAM)
	USB_DEVICE_DESCRIPTOR device_dsc =
#else
	ROM USB_DEVICE_DESCRIPTOR device_dsc =
#endif
{
    18,    // Size of this descriptor in bytes
    USB_DESCRIPTOR_DEVICE,  // DEVICE descriptor type
    0x0110,                 // USB Spec Release Number in BCD format
    0x00,                   // Class Code
    0x00,                   // Subclass code
    0x00,                   // Protocol code
    USB_EP0_BUFF_SIZE,      // Max packet size for EP0, see usb_config.h
    VENDOR_ID,              // Vendor ID
    PRODUCT_ID,             // Product ID
    0x0200,                 // Device release number in BCD format
    0x01,                   // Manufacturer string index
    0x02,                   // Product string index
    0x00,                   // Device serial number string index
    0x01                    // Number of possible configurations
};

/* Configuration 1 Descriptor */
ROM BYTE configDescriptor1[] =
{
    /* Configuration Descriptor */
    9,    					// Size of this descriptor in bytes
    USB_DESCRIPTOR_CONFIGURATION,                // CONFIGURATION descriptor type
    WORD_BYTES(0x29),       // Total length of data for this cfg (41 bytes)
    1,                      // Number of interfaces in this cfg
    1,                      // Index value of this configuration
    0,                      // Configuration string index
    _DEFAULT | _SELF,       // Attributes, see usb_device.h
    50,                     // Max power consumption (2X mA)
	
						
    /* Interface Descriptor */
    9,  					// Size of this descriptor in bytes
    USB_DESCRIPTOR_INTERFACE,               // INTERFACE descriptor type
    0,                      // Interface Number
    0,                      // Alternate Setting Number
    2,                      // Number of endpoints in this intf
    HID_INTF,               // Class code
    0,     					// Subclass code
    0,     					// Protocol code
    0,                      // Interface string index

    /* HID Class-Specific Descriptor */
    9,    				 	// Size of this descriptor in bytes
    DSC_HID,                // HID descriptor type
    WORD_BYTES(0x0111),     // HID Spec Release Number in BCD format (1.11)
    0x00,                   // Country Code (0x00 for Not supported)
    HID_NUM_OF_DSC,         // Number of class descriptors, see usbcfg.h
    DSC_RPT,                // Report descriptor type
	WORD_BYTES(HID_RPT01_SIZE), // Size of the report descriptor
    
    /* Endpoint Descriptor */
    7,						// Size of this descriptor in bytes
    USB_DESCRIPTOR_ENDPOINT,// Endpoint Descriptor
    HID_EP | _EP_IN,        // EndpointAddress
    _INTERRUPT,             // Attributes
    WORD_BYTES(64),         // Max Packet Size
    10,                     // Interval

    /* Endpoint Descriptor */
    7,						// Size of this descriptor in bytes
    USB_DESCRIPTOR_ENDPOINT,// Endpoint Descriptor
    HID_EP | _EP_OUT,       // EndpointAddress
    _INTERRUPT,             // Attributes
    WORD_BYTES(64),         // Max Packet Size
    10                      // Interval
};

//Language code string descriptor
ROM struct { BYTE bLength;BYTE bDscType;WORD string[1]; } sd000 =
{
	sizeof(sd000), USB_DESCRIPTOR_STRING, {0x0409}
};


//Manufacturer string descriptor

#if defined(MR_LX) && defined(TARGET_WII)
	ROM struct { BYTE bLength; BYTE bDscType; WORD string[16]; } sd001 = 
	{
		sizeof(sd001), USB_DESCRIPTOR_STRING,
		{ // '-LX' is here so software can detect LX in Wii version
			'B','y','t','e',' ','A','r','t','s',' ','L','L','C','-','L','X'
		}
	};
#else
	ROM struct { BYTE bLength; BYTE bDscType; WORD string[13]; } sd001 =
	{ 
		sizeof(sd001), USB_DESCRIPTOR_STRING,
		{
			'B', 'y', 't', 'e', ' ', 'A', 'r', 't', 's', ' ', 'L', 'L', 'C'
		}
	};
#endif

// Product string descriptor

#if defined(TARGET_WII)

	// Wii requires this special string for controller to be recognized in GH5
	ROM struct { BYTE bLength; BYTE bDscType; WORD string[41];} sd002 =
	{
		sizeof(sd002),USB_DESCRIPTOR_STRING,
		{
			'H','a','r','m','o','n','i','x',' ','D','r','u','m',' ','C','o',
			'n','t','r','o','l','l','e','r',' ','f','o','r',' ','N','i','n',
			't','e','n','d','o',' ','W','i','i' // 41
		}
	};
	
#else

	/*
	ROM struct { BYTE bLength; BYTE bDscType; WORD string[34];} sd002 =
	{
		sizeof(sd002),USB_DESCRIPTOR_STRING,
		{//  1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17
			'G','u','i','t','a','r',' ','H','e','r','o','4',' ',
			'f','o','r',' ',
			'P','l','a','y','s','t','a','t','i','o','n',' ','(','R',')',' ','3' // 34 bytes
		}
	};
	*/

	#if defined(MR_LX)
	
		ROM struct { BYTE bLength; BYTE bDscType; WORD string[20];} sd002 =
		{
			sizeof(sd002),USB_DESCRIPTOR_STRING,
			{
		#if defined(TARGET_XBOX)
				'M','I','D','I',' ','R','o','c','k','e','r',' ','L','X','i',
				' ','V', MAJOR_REVISION, '.', MINOR_REVISION
		#else
				'M','I','D','I',' ','R','o','c','k','e','r',' ','L','X',' ',
				' ','V', MAJOR_REVISION, '.', MINOR_REVISION
		#endif
			}
		};
		
	#else
	
		ROM struct { BYTE bLength; BYTE bDscType; WORD string[16];} sd002 =
		{
			sizeof(sd002),USB_DESCRIPTOR_STRING,
			{
				'M','I','D','I',' ','R','o','c','k','e','r',
				' ','V', MAJOR_REVISION, '.', MINOR_REVISION
			}
		};
		
	#endif
#endif


//Class specific descriptor - Generic HID

// To change the number of bytes in a report, under Input report, Output report,
// or Feature report below. 
// In usb_config.h, edit these values to match the new report size(s): 
// #define HID_INPUT_REPORT_BYTES   
// #define HID_OUTPUT_REPORT_BYTES  
// #define HID_FEATURE_REPORT_BYTES 


ROM struct { BYTE report[HID_RPT01_SIZE]; } hid_rpt01 = {
{
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x35, 0x00,                    //   PHYSICAL_MINIMUM (0)
    0x45, 0x01,                    //   PHYSICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x0d,                    //   REPORT_COUNT (13)
    0x05, 0x09,                    //   USAGE_PAGE (Button)
    0x19, 0x01,                    //   USAGE_MINIMUM (Button 1)
    0x29, 0x0d,                    //   USAGE_MAXIMUM (Button 13)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x03,                    //   REPORT_COUNT (3)
    0x81, 0x01,                    //   INPUT (Cnst,Ary,Abs)
    0x05, 0x01,                    //   USAGE_PAGE (Generic Desktop)
    0x25, 0x07,                    //   LOGICAL_MAXIMUM (7)
    0x46, 0x3b, 0x01,              //   PHYSICAL_MAXIMUM (315)
    0x75, 0x04,                    //   REPORT_SIZE (4)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x65, 0x14,                    //   UNIT (Eng Rot:Angular Pos)
    0x09, 0x39,                    //   USAGE (Hat switch)
    0x81, 0x42,                    //   INPUT (Data,Var,Abs,Null)
    0x65, 0x00,                    //   UNIT (None)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x81, 0x01,                    //   INPUT (Cnst,Ary,Abs)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x46, 0xff, 0x00,              //   PHYSICAL_MAXIMUM (255)
    0x09, 0x30,                    //   USAGE (X)
    0x09, 0x31,                    //   USAGE (Y)
    0x09, 0x32,                    //   USAGE (Z)
    0x09, 0x35,                    //   USAGE (Rz)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x04,                    //   REPORT_COUNT (4)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x06, 0x00, 0xff,              //   USAGE_PAGE (Generic Desktop)
    0x09, 0x20,                    //   USAGE (Undefined)
    0x09, 0x21,                    //   USAGE (Undefined)
    0x09, 0x22,                    //   USAGE (Undefined)
    0x09, 0x23,                    //   USAGE (Undefined)
    0x09, 0x24,                    //   USAGE (Undefined)
    0x09, 0x25,                    //   USAGE (Undefined)
    0x09, 0x26,                    //   USAGE (Undefined)
    0x09, 0x27,                    //   USAGE (Undefined)
    0x09, 0x28,                    //   USAGE (Undefined)
    0x09, 0x29,                    //   USAGE (Undefined)
    0x09, 0x2a,                    //   USAGE (Undefined)
    0x09, 0x2b,                    //   USAGE (Undefined)
    0x95, 0x0c,                    //   REPORT_COUNT (12)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x0a, 0x21, 0x26,              //   0xa(0x2621)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0xb1, 0x02,                    //   FEATURE (Data,Var,Abs)
    0x0a, 0x21, 0x26,              //   0xa(0x2621)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
    0x26, 0xff, 0x03,              //   LOGICAL_MAXIMUM (1023)
    0x46, 0xff, 0x03,              //   PHYSICAL_MAXIMUM (1023)
    0x09, 0x2c,                    //   0x9(0x2c)
    0x09, 0x2d,                    //   0x9(0x2d)
    0x09, 0x2e,                    //   0x9(0x2e)
    0x09, 0x2f,                    //   0x9(0x2f)
    0x75, 0x10,                    //   REPORT_SIZE (16)
    0x95, 0x04,                    //   REPORT_COUNT (4)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0xc0                           // END_COLLECTION
	}
};                  

//Array of configuration descriptors
ROM BYTE *ROM USB_CD_Ptr[]=
{
    (ROM BYTE *ROM)&configDescriptor1
};

//Array of string descriptors
ROM BYTE *ROM USB_SD_Ptr[]=
{
    (ROM BYTE *ROM)&sd000,
    (ROM BYTE *ROM)&sd001,
    (ROM BYTE *ROM)&sd002
};

/** EOF usbdescriptors.c ***************************************************/

#endif
