/********************************************************************
 FileName:     USBConfig.h
 Dependencies: Always: GenericTypeDefs.h, usbd.h
               Situational: hid.h, cdc.h, msd.h, etc.
 Processor:    PIC18 USB Microcontrollers
 Hardware:
 Complier:     Microchip C18
 Company:		Microchip Technology, Inc.

 Software License Agreement:

 The software supplied herewith by Microchip Technology Incorporated
 (the �Company�) for its PICmicro� Microcontroller is intended and
 supplied to you, the Company�s customer, for use solely and
 exclusively on Microchip PICmicro Microcontroller products. The
 software is owned by the Company and/or its supplier, and is
 protected under applicable copyright laws. All rights are reserved.
 Any use in violation of the foregoing restrictions may subject the
 user to criminal sanctions under applicable laws, as well as to
 civil liability for the breach of the terms and conditions of this
 license.

 THIS SOFTWARE IS PROVIDED IN AN �AS IS� CONDITION. NO WARRANTIES,
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
        5/6/08       Modified for generic HID application (Jan Axelson)
 *******************************************************************/

/*********************************************************************
 * Descriptor specific type definitions are defined in: usbd.h
 ********************************************************************/

#ifndef USBCFG_H
#define USBCFG_H

/** INCLUDES *******************************************************/

// including hid.h here so that it can be auto-generated by a GUI tool.
#include "USB/usb_function_hid.h"
#include "main.h" 

/** DEFINITIONS ****************************************************/
#define USB_EP0_BUFF_SIZE      64   // 8, 16, 32, or 64
								// Using larger options take more SRAM, but
								// does not provide much advantage in most types
								// of applications.  Exceptions to this, are applications
								// that use EP0 IN or OUT for sending large amounts of
								// application related data.

#define USB_MAX_NUM_INT    1   	// For tracking Alternate Setting

#define HID_EP 1

//the maximum transfer size of EP1
#define USB_MAX_DATA_SIZE_EP1 64
#define USB_NUM_MESSAGES_EP1 1

//Make sure only one of the below "#define USB_PING_PONG_MODE" is uncommented
#define USB_PING_PONG_MODE USB_PING_PONG__NO_PING_PONG
//#define USB_PING_PONG_MODE USB_PING_PONG__FULL_PING_PONG
//#define USB_PING_PONG_MODE USB_PING_PONG__EP0_OUT_ONLY
//#define USB_PING_PONG_MODE USB_PING_PONG__ALL_BUT_EP0 //NOTE: this is not supported in 18F4550 rev Ax

#define USB_POLLING

#define USB_MAX_EP_NUMBER       2  // <<< not sure about this, should it be 1?

/* Parameter definitions are defined in usbd.h */
#define MODE_PP                 USB_PING_PONG_MODE
#define UCFG_VAL                _PUEN|_TRINT|_FS|MODE_PP

//#define USE_SELF_POWER_SENSE_IO
//#define USE_USB_BUS_SENSE_IO

// Uncomment one of the lines below to add support for a bootloader
// be sure to use the right linker script
#define PROGRAMMABLE_WITH_USB_HID_BOOTLOADER
//#define PROGRAMMABLE_WITH_USB_LEGACY_CUSTOM_CLASS_BOOTLOADER

#define SUPPORT_SET_DSC
#define SUPPORT_SYNC_FRAME

//#define USB_DYNAMIC_EP_CONFIG

/** DEVICE CLASS USAGE *********************************************/

#define USB_USE_HID
#define USB_SUPPORT_DEVICE

/** ENDPOINTS ALLOCATION *******************************************/

/* HID */
#define HID_INTF_ID             0x00
#define HID_UEP                 UEP1
#define HID_BD_OUT              USB_EP_1_OUT
#define HID_INT_OUT_EP_SIZE     10
#define HID_BD_IN               USB_EP_1_IN
#define HID_INT_IN_EP_SIZE      29
#define HID_NUM_OF_DSC          1
#define HID_RPT01_SIZE          137  // Size of the HID report descriptor in bytes (NOT the report size)

// The number of bytes in each report, 
// calculated from Report Size and Report Count in the report descriptor:
#define HID_INPUT_REPORT_BYTES   27
#define HID_OUTPUT_REPORT_BYTES  8
#define HID_FEATURE_REPORT_BYTES 8

#define USB_HID_SET_REPORT_HANDLER mySetReportHandler

#define USBGEN_EP_SIZE 64

#define HID_INTF_ID             		0x00
#define HID_NUM_OF_DSC          		1

/** DEFINITIONS ****************************************************/

#endif //USBCFG_H
