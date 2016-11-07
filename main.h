/*********************************************************************
 *
 *                Microchip USB C18 Firmware - Generic HID Demo
 *
 *********************************************************************
 * FileName:        generic_hid.h
 * Dependencies:    See INCLUDES section below
 * Processor:       PIC18
 * Compiler:        C18 2.30.01+
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * The software supplied herewith by Microchip Technology Incorporated
 * (the “Company”) for its PICmicro® Microcontroller is intended and
 * supplied to you, the Company’s customer, for use solely and
 * exclusively on Microchip PICmicro Microcontroller products. The
 * software is owned by the Company and/or its supplier, and is
 * protected under applicable copyright laws. All rights are reserved.
 * Any use in violation of the foregoing restrictions may subject the
 * user to criminal sanctions under applicable laws, as well as to
 * civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 * Author               Date        Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Rawin Rojvanit       11/19/04    Original.
 * Jan Axelson           9/16/08    Renamed and adapted for Generic HIDs.	
 ********************************************************************/

#ifndef USER_H
#define USER_H

#define SET_REPORT_PFUNC &USB_HID_SET_REPORT_HANDLER
#define OUTPUT_REPORT &hid_report_out[0]
#define FEATURE_REPORT &hid_report_feature[0]

#define LSB(a)          ((a).v[0])
#define MSB(a)          ((a).v[1])

/** P U B L I C  P R O T O T Y P E S *****************************************/

extern void ProcessIO(void);
extern void mySetReportHandler(void);
extern BYTE ReportSupported(void);
     
#endif //USER_H



