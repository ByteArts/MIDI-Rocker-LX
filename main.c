/********************************************************************
 FileName:     generic_hid.c
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
 
  1.0   5/5/2008     Converted Microchip's mouse.c for use in a
                     generic HID (Jan Axelson)	
  1.1   6/21/08      Revised ReportLoopback routine.
  1.2   9/16/08      Revised for use with Microchip Framework V2.2
  1.3   9/30/08      Revised Set_Report code
  1.4   10/4/08      Upgraded for Microchip Framework V2.3. 
					 Required adding LSB macro to generic_hid.h

********************************************************************/
/*
Handles application-specific tasks.

See the project's readme file for more information.
*/

/** INCLUDES *******************************************************/
#include "GenericTypeDefs.h"
#include "Compiler.h"
#include "usb_config.h"
#include "USB/usb_device.h"
#include "USB/usb.h"
#include "main.h" 
#include "HardwareProfile.h"
#include "Pinout.h"
#include "App.h"
#include "MIDI.h"

/** CONFIGURATION **************************************************/

#include "Config.h" // Processor configuration

/** VARIABLES ******************************************************/
#pragma udata

USB_HANDLE USBInHandle = 0;
USB_HANDLE USBOutHandle = 0;

BYTE g_PS3ControllerID = 0;


/** PRIVATE PROTOTYPES *********************************************/

void BlinkUSBStatus(void);
static void InitializeSystem(void);

void mySetReportHandler(void);
void HID_InputReport(void);
void HID_OutputReport(void);
BYTE ReportSupported(void);


/** VECTOR REMAPPING ***********************************************/


#if defined(__18CXX)
	// On PIC18 devices, addresses 0x00, 0x08, and 0x18 are used for
	// the reset, high priority interrupt, and low priority interrupt
	// vectors.  However, the current Microchip USB bootloader 
	// examples are intended to occupy addresses 0x00-0x7FF or
	// 0x00-0xFFF depending on which bootloader is used.  Therefore,
	// the bootloader code remaps these vectors to new locations
	// as indicated below.  This remapping is only necessary if you
	// wish to program the hex file generated from this project with
	// the USB bootloader.  If no bootloader is used, edit the
	// usb_config.h file and comment out the following defines:
	// #define PROGRAMMABLE_WITH_USB_HID_BOOTLOADER
	// #define PROGRAMMABLE_WITH_USB_LEGACY_CUSTOM_CLASS_BOOTLOADER
	
	#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)
		#define REMAPPED_RESET_VECTOR_ADDRESS			0x1000
		#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x1008
		#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x1018
	#elif defined(PROGRAMMABLE_WITH_USB_LEGACY_CUSTOM_CLASS_BOOTLOADER)	
		#define REMAPPED_RESET_VECTOR_ADDRESS			0x800
		#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x808
		#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x818
	#else	
		#define REMAPPED_RESET_VECTOR_ADDRESS			0x00
		#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x08
		#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x18
	#endif
	
	#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER) || defined(PROGRAMMABLE_WITH_USB_LEGACY_CUSTOM_CLASS_BOOTLOADER)
		extern void _startup (void); // See c018i.c in your C18 compiler dir

		#pragma code REMAPPED_RESET_VECTOR = REMAPPED_RESET_VECTOR_ADDRESS
		void _reset (void)
		{
		    _asm goto _startup _endasm
		}
	#endif

	#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)
		/*
		"Stub" functions are here for when the code is being programmed using the old bootloader 
		which expected to find the user code vectors at these locations. The stub functions are 
		simply jumps to where the vectors are located when using code designed to be loaded with
		HID bootloader. This way, the old bootloader can still load the same hex file that is 
		designed to be used with the new HID bootloader.
		*/
		#pragma code _STUB0 = 0x0800 // old bootloaded code - reset address
		void _stub0(void)
		{
			_asm 
				goto 0x1000 // jump to HID bootloaded reset
			_endasm
		}
		
		#pragma code _STUB1 = 0x0808 // old bootloaded code - high priority vector
		void _stub1(void)
		{
			_asm 
				goto 0x1008 // jump to HID bootloaded high priority vector
			_endasm
		}

		#pragma code _STUB2 = 0x0818 // old bootloaded code - low priority vector
		void _stub2(void)
		{
			_asm 
				goto 0x1018 // jump to HID bootloaded low priority vector
			_endasm
		}

	#endif


	#pragma code REMAPPED_HIGH_INTERRUPT_VECTOR = REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS
	void Remapped_High_ISR (void)
	{
		//     _asm goto YourHighPriorityISRCode _endasm
	}
	#pragma code REMAPPED_LOW_INTERRUPT_VECTOR = REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS
	void Remapped_Low_ISR (void)
	{
		//     _asm goto YourLowPriorityISRCode _endasm
	}
	
	#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER) || defined(PROGRAMMABLE_WITH_USB_LEGACY_CUSTOM_CLASS_BOOTLOADER)
	//Note: If this project is built while one of the bootloaders has
	//been defined, but then the output hex file is not programmed with
	//the bootloader, addresses 0x08 and 0x18 would end up programmed with 0xFFFF.
	//As a result, if an actual interrupt was enabled and occured, the PC would jump
	//to 0x08 (or 0x18) and would begin executing "0xFFFF" (unprogrammed space).  This
	//executes as nop instructions, but the PC would eventually reach the REMAPPED_RESET_VECTOR_ADDRESS
	//(0x1000 or 0x800, depending upon bootloader), and would execute the "goto _startup".  This
	//would effective reset the application.
	
	//To fix this situation, we should always deliberately place a 
	//"goto REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS" at address 0x08, and a
	//"goto REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS" at address 0x18.  When the output
	//hex file of this project is programmed with the bootloader, these sections do not
	//get bootloaded (as they overlap the bootloader space).  If the output hex file is not
	//programmed using the bootloader, then the below goto instructions do get programmed,
	//and the hex file still works like normal.  The below section is only required to fix this
	//scenario.
	#pragma code HIGH_INTERRUPT_VECTOR = 0x08
	void High_ISR (void)
	{
	     _asm goto REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS _endasm
	}
	#pragma code LOW_INTERRUPT_VECTOR = 0x18
	void Low_ISR (void)
	{
	     _asm goto REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS _endasm
	}
	#endif	//end of "#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)||defined(PROGRAMMABLE_WITH_USB_LEGACY_CUSTOM_CLASS_BOOTLOADER)"

	#pragma code
	
	
	//These are your actual interrupt handling routines.
	#pragma interrupt YourHighPriorityISRCode
	void YourHighPriorityISRCode()
	{
		//Check which interrupt flag caused the interrupt.
		//Service the interrupt
		//Clear the interrupt flag
		//Etc.
	
	}	//This return will be a "retfie fast", since this is in a #pragma interrupt section 
	#pragma interruptlow YourLowPriorityISRCode
	void YourLowPriorityISRCode()
	{
		//Check which interrupt flag caused the interrupt.
		//Service the interrupt
		//Clear the interrupt flag
		//Etc.
	
	}	//This return will be a "retfie", since this is in a #pragma interruptlow section 

#endif //of "#if defined(__18CXX)"


/** DECLARATIONS ***************************************************/
#pragma code


/********************************************************************
 * Function:        void BlinkUSBStatus(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        BlinkUSBStatus turns on and off LEDs 
 *                  corresponding to the USB device state.
 *
 * Note:            mLED macros can be found in HardwareProfile.h
 *                  USBDeviceState is declared and updated in
 *                  usbd.c.
 *******************************************************************/
void BlinkUSBStatus(void)
{
    static WORD led_count = 0;
    
    if (led_count == 0) 
		led_count = 10000U;
    led_count--;

    if (USBSuspendControl == 1)
    {
        if (led_count == 0)
        {
			ledUSB = !ledUSB;
        }//end if
    }
    else
    {
        if (USBDeviceState == DETACHED_STATE)
        {
            ledUSB = LED_OUTPUT_OFF;
        }
        else if (USBDeviceState == ATTACHED_STATE)
        {
            ledUSB = !ledUSB; 
        }
        else if (USBDeviceState == POWERED_STATE)
        {
            ledUSB = !ledUSB; 
        }
        else if (USBDeviceState == DEFAULT_STATE)
        {
            ledUSB = LED_OUTPUT_OFF;
        }
        else if (USBDeviceState == ADDRESS_STATE)
        {
            if (led_count == 0)
            {
                ledUSB = !ledUSB;
            }//end if
        }
        else if (USBDeviceState == CONFIGURED_STATE)
        {
            if (led_count == 0)
            {
                ledUSB = LED_OUTPUT_ON;
            }//end if
        }//end if(...)
    }//end if(UCONbits.SUSPND...)

}//end BlinkUSBStatus

/********************************************************************
 * Function:        void ProcessIO(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This function is a place holder for other user
 *                  routines. It is a mixture of both USB and
 *                  non-USB tasks.
 *
 * Note:            None
 *******************************************************************/
void ProcessIO(void)
{   
    //Blink the LEDs according to the USB device status
    BlinkUSBStatus();

	// in Wii/GH mode, we want to continue to processs IO even if USB not active	
	if ((g_SystemMode == SYS_MODE_WII) && (g_GameMode == gmGUITAR_HERO))
		; // do nothing
	else if ((USBDeviceState < CONFIGURED_STATE) || (USBSuspendControl == 1)) 
		return;

    // User Application USB tasks

   	// Send and receive HID reports
	HID_InputReport();
	HID_OutputReport();

#ifndef MIDI_INTERRUPT
	// poll the UART for MIDI data
	if (PIR1bits.RCIF)
		MIDI_ServiceUARTRx();

	// check for UART error
	if (RCSTA & (0b0110)) // check FERR and OERR bits (framing, overun error)
	{
		#if defined(__DEBUG)
			ErrorMessage(RCSTA & ERR_UART, TRUE);
		#endif
		
		// clear and then set the CREN bit to clear the error
		RCSTA &= 0xEF; // clear CREN
		RCSTA |= 0x10; // set CREN, re-enable reception	
	}
#endif


}//end ProcessIO

/********************************************************************
 * Function:        static void InitializeSystem(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        InitializeSystem is a centralize initialization
 *                  routine. All required USB initialization routines
 *                  are called from here.
 *
 *                  User application initialization routine should
 *                  also be called from here.                  
 *
 * Note:            None
 *******************************************************************/
static void InitializeSystem(void)
{
    // ADCON1 |= 0x0F;                 // Default all pins to digital

    
//	The USB specifications require that USB peripheral devices must never source
//	current onto the Vbus pin.  Additionally, USB peripherals should not source
//	current on D+ or D- when the host/hub is not actively powering the Vbus line.
//	When designing a self powered (as opposed to bus powered) USB peripheral
//	device, the firmware should make sure not to turn on the USB module and D+
//	or D- pull up resistor unless Vbus is actively powered.  Therefore, the
//	firmware needs some means to detect when Vbus is being powered by the host.
//	A 5V tolerant I/O pin can be connected to Vbus (through a resistor), and
// 	can be used to detect when Vbus is high (host actively powering), or low
//	(host is shut down or otherwise not supplying power).  The USB firmware
// 	can then periodically poll this I/O pin to know when it is okay to turn on
//	the USB module/D+/D- pull up resistor.  When designing a purely bus powered
//	peripheral device, it is not possible to source current on D+ or D- when the
//	host is not actively providing power on Vbus. Therefore, implementing this
//	bus sense feature is optional.  This firmware can be made to use this bus
//	sense feature by making sure "USE_USB_BUS_SENSE_IO" has been defined in the
//	usbcfg.h file.    
    #if defined(USE_USB_BUS_SENSE_IO)
    tris_usb_bus_sense = INPUT_PIN; // See io_cfg.h
    #endif
    
//	If the host PC sends a GetStatus (device) request, the firmware must respond
//	and let the host know if the USB peripheral device is currently bus powered
//	or self powered.  See chapter 9 in the official USB specifications for details
//	regarding this request.  If the peripheral device is capable of being both
//	self and bus powered, it should not return a hard coded value for this request.
//	Instead, firmware should check if it is currently self or bus powered, and
//	respond accordingly.  If the hardware has been configured like demonstrated
//	on the PICDEM FS USB Demo Board, an I/O pin can be polled to determine the
//	currently selected power source.  On the PICDEM FS USB Demo Board, "RA2" 
//	is used for	this purpose.  If using this feature, make sure "USE_SELF_POWER_SENSE_IO"
//	has been defined in usbcfg.h, and that an appropriate I/O pin has been mapped
//	to it in io_cfg.h.
    #if defined(USE_SELF_POWER_SENSE_IO)
    tris_self_power = INPUT_PIN;
    #endif
    
    USBDeviceInit();
}//end InitializeSystem

/********************************************************************
 * Function:        void main(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Main program entry point.
 *
 * Note:            None
 *******************************************************************/
void main(void)
{
	// setup I/O pins
#if defined(MR_LX)
	// do a little delay to allow for power to settle
	DelayMillisecs(300);
	InitIO_LX();

	// figure out what mode go into to...
	DoBootModeSelect_LX(); // updates g_SystemMode, g_GameMode
#else
	// do a little delay to allow for power to settle
	DelayMillisecs(1000);
	InitIO_MR();

	// figure out what mode go into to...
	DoBootModeSelect_MR(); // updates g_SystemMode, g_GameMode
#endif

	SetPID(g_GameMode); // sets game mode - affects USB descriptor

	// initialize global settings
	RecallStoredSettings();

#if defined(MR_LX)

	// turn on LEDs to indicate game mode
	if (g_GameMode == gmGUITAR_HERO)
	{
		ledM2 = LED_OUTPUT_ON;
		g_MidiMapNumber = 1; // default to using 2nd map with guitar hero
	}
	else
	{
		ledM1 = LED_OUTPUT_ON;
		g_MidiMapNumber = 0; // default to using 1st map with Rock Band
	}

	// display current velocity
	DisplayValue(g_MinVelocity / 25);
	DelayMillisecs(1500);
	DisplayValue(0); // turns off the LEDs

#else

	if (g_GameMode == gmGUITAR_HERO)
		ledCH5 = LED_OUTPUT_ON;
	else
		ledCH1 = LED_OUTPUT_ON;

	DelayMillisecs(1500); // keep LED on for a little while

	// function switch determines which map to use
	if (swFUNCTION == SW_PRESSED)
		g_MidiMapNumber = 1;
	else
		g_MidiMapNumber = 0;

#endif

	SetMidiMapNumber(g_MidiMapNumber, TRUE); // gets saved map

	// initialize button state machine
	InitButtonStates();

	// setup UART for MIDI
	MIDI_Initialize();

	if (g_SystemMode == SYS_MODE_XBOX)
		Main_Xbox360();

#if defined(TARGET_WII)
	else if (g_SystemMode == SYS_MODE_WII) 
	{
		InitializeSystem(); // initialize all I/O, enable USB
		Main_Wii();
	}
#else
	else if (g_SystemMode == SYS_MODE_PS3) 
	{
		InitializeSystem(); // initialize all I/O, enable USB
		Main_PS3();
	}
#endif

	// shouldn't ever get here --- the bootloader should catch it
	// __init(); // ??? not sure about this

}//end main

void USBCBSuspend(void)
{
    #if defined(__C30__)
    #if 0
        U1EIR = 0xFFFF;
        U1IR = 0xFFFF;
        U1OTGIR = 0xFFFF;
        IFS5bits.USB1IF = 0;
        IEC5bits.USB1IE = 1;
        U1OTGIEbits.ACTVIE = 1;
        U1OTGIRbits.ACTVIF = 1;
        TRISA &= 0xFF3F;
        LATAbits.LATA6 = 1;
        Sleep();
        LATAbits.LATA6 = 0;
    #endif
    #endif
}

#if 0
void __attribute__ ((interrupt)) _USB1Interrupt(void)
{
    #if !defined(self_powered)
        if(U1OTGIRbits.ACTVIF)
        {
            LATAbits.LATA7 = 1;
        
            IEC5bits.USB1IE = 0;
            U1OTGIEbits.ACTVIE = 0;
            IFS5bits.USB1IF = 0;
        
            //USBClearInterruptFlag(USBActivityIFReg,USBActivityIFBitNum);
            USBClearInterruptFlag(USBIdleIFReg,USBIdleIFBitNum);
            //USBSuspendControl = 0;
            LATAbits.LATA7 = 0;
        }
    #endif
}
#endif

void USBCBWakeFromSuspend(void)
{
}

/********************************************************************
 * Function:        void USBCB_SOF_Handler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The USB host sends out a SOF packet to full-speed
 *                  devices every 1 ms. This interrupt may be useful
 *                  for isochronous pipes. End designers should
 *                  implement callback routine as necessary.
 *
 * Note:            None
 *******************************************************************/
void USBCB_SOF_Handler(void)
{
    // No need to clear UIRbits.SOFIF to 0 here.
    // Callback caller is already doing that.
}

/*******************************************************************
 * Function:        void USBCBErrorHandler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The purpose of this callback is mainly for
 *                  debugging during development. Check UEIR to see
 *                  which error causes the interrupt.
 *
 * Note:            None
 *******************************************************************/
void USBCBErrorHandler(void)
{
    // No need to clear UEIR to 0 here.
    // Callback caller is already doing that.
}

/*******************************************************************
 * Function:        void USBCBCheckOtherReq
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Check for vendor- or class-specific control requests.
 *
 * Note:            None
 *******************************************************************/
void USBCBCheckOtherReq(void)
{
     USBCheckHIDRequest();
}//end

void USBCBStdSetDscHandler(void)
{
    // Must claim session ownership if supporting this request
}//end

/*******************************************************************
 * Function:        void USBCBInitEP(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This function is called when the device becomes
 *                  initialized.  This should initialize the endpoints
 *                  for the device's usage according to the current
 *                  configuration.
 *
 * Note:            None
 *******************************************************************/
void USBCBInitEP(void)
{
    //enable the HID endpoint
    USBEnableEndpoint(HID_EP,USB_IN_ENABLED|USB_OUT_ENABLED | USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);
}

/********************************************************************
 * Function:        void USBCBSendResume(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This callback should send a RESUME signal that
 *                  has the period of 1-15ms.
 *
 * Note:            Interrupt vs. Polling
 *                  -Primary clock
 *                  -Secondary clock ***** MAKE NOTES ABOUT THIS *******
 *                   > Can switch to primary first by calling USBCBWakeFromSuspend()
 
 *                  The modifiable section in this routine should be changed
 *                  to meet the application needs. Current implementation
 *                  temporary blocks other functions from executing for a
 *                  period of 1-13 ms depending on the core frequency.
 *
 *                  According to USB 2.0 specification section 7.1.7.7,
 *                  "The remote wakeup device must hold the resume signaling
 *                  for at lest 1 ms but for no more than 15 ms."
 *                  The idea here is to use a delay counter loop, using a
 *                  common value that would work over a wide range of core
 *                  frequencies.
 *                  That value selected is 1800. See table below:
 *                  ==========================================================
 *                  Core Freq(MHz)      MIP         RESUME Signal Period (ms)
 *                  ==========================================================
 *                      48              12          1.05
 *                       4              1           12.6
 *                  ==========================================================
 *                  * These timing could be incorrect when using code
 *                    optimization or extended instruction mode,
 *                    or when having other interrupts enabled.
 *                    Make sure to verify using the MPLAB SIM's Stopwatch
 *                    and verify the actual signal on an oscilloscope.
 *******************************************************************/
void USBCBSendResume(void)
{
    static WORD delay_count;
    
    USBResumeControl = 1;                // Start RESUME signaling
    
    delay_count = 1800U;                // Set RESUME line for 1-13 ms
    do
    {
        delay_count--;
    }while(delay_count);
    USBResumeControl = 0;
}

/******************************************************************************
 * Function:        void mySetReportHandler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Checks to see if an Output or Feature report has arrived
 *         			on the control pipe. If yes, extracts and uses the data.
 *
 * Note:            None
 *****************************************************************************/

void mySetReportHandler(void)
{
	BYTE count = 0;

	// Find out if an Output or Feature report has arrived on the control pipe.
	// Get the report type from the Setup packet.

	switch (MSB(SetupPkt.W_Value))
    {
		case 0x02: // Output report 
		{	
    		switch (LSB(SetupPkt.W_Value))
		    {
				case 0: // Report ID 0
				{
					/*
					The PS3 sends a report to set options on the controller. The only one
					I've figured out is byte 2, which sets the controller number.
					*/
					g_PS3ControllerID = hid_report_out[2];
				}
				break;
			} // end switch(LSB(SetupPkt.W_Value))
			break;
		}

		case 0x03: // Feature report 
		{
			// Get the report ID from the Setup packet.

    		switch (LSB(SetupPkt.W_Value))
		    {
				// The Feature report data is in hid_report_feature.
				// This example application just sends the data back in the next
				// Get_Report request for a Feature report.			
			
			    // wCount holds the number of bytes read in the Data stage.
				// This example assumes the report fits in one transaction.	
		
				// The Feature report uses a single buffer so to send the same data back
				// in the next IN Feature report, there is nothing to copy.
				// The data is in hid_report_feature[HID_FEATURE_REPORT_BYTES]
				case 0: // Report ID 0
				{
#ifdef PROCESS_HOST_CMD
					/*
					This is where we can get some data when host sends a SetFeature request. My
					GUI program uses it to send commands to the MIDI Rocker, and the PS3 uses
					it to set the controller id number etc... 

					NOTE: Make sure that HOST_CMD_BUF_SIZE is not bigger than HID_FEATURE_REPORT_BYTES
					*/
					UINT8 nIndex;

					// clear the buffer
					for (nIndex = 0; (nIndex < HOST_CMD_BUF_SIZE); ++nIndex)
						g_HostCmdBuffer[nIndex] = 0;

					// copy data into buffer
					for (nIndex = 0; nIndex < HOST_CMD_BUF_SIZE; ++nIndex)
						g_HostCmdBuffer[nIndex] = hid_report_feature[nIndex];

					ProcessHostCommand();
#endif
				}
				break;
				
			} // end switch(LSB(SetupPkt.W_Value))	
			break;	
		}
		
	} // end switch(MSB(SetupPkt.W_Value))

} // end mySetReportHandler

/******************************************************************************
 * Function:        BOOL ReportSupported(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          2 if it's a supported Output report
 *                  3 if it's a supported Feature report
 *                  0 for all other cases
 *
 * Side Effects:    None
 *
 * Overview:        Checks to see if the HID supports a specific Output or Feature
 *				    report. 
 *
 * Note:            None
 *****************************************************************************/

BYTE ReportSupported(void)
{
	// Find out if an Output or Feature report has arrived on the control pipe.

	USBDeviceTasks();

	switch (MSB(SetupPkt.W_Value))
    {
		case 0x02: // Output report 
		{	
    		switch(LSB(SetupPkt.W_Value))
		    {
				case 0x00: // Report ID 0
				{
					return 2;		
				}
				default:
				{
					// Other report IDs not supported.

					return 0;				
				}			
			} // end switch(LSB(SetupPkt.W_Value))					
		}

		case 0x03: // Feature report 
		{
    		switch(LSB(SetupPkt.W_Value))
		    {
				case 0x00: // Report ID 0
				{
					return 3;					
				}
				default:
				{
					// Other report IDs not supported.

					return 0;					
				}
			} // end switch(LSB(SetupPkt.W_Value))
		}			
		default:
		{
			return 0;	
		}			
	} // end switch(MSB(SetupPkt.W_Value))

} // end ReportSupported

/******************************************************************************
 * Function:        void ReportLoopback(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    The ownership of the USB buffers will change according
 *                  to the required operation
 *
 * Overview:        This routine will send a received Input report back 
 *                  to the host in an Output report.
 *                  Both directions use interrupt transfers.
 *
 * Note:            
 *
 *****************************************************************************/
void HID_InputReport(void)
{ 
	if (!HIDTxHandleBusy(USBInHandle))		 
	{
	#if defined(MR_LX)
		UpdateInputReportData_LX(); // populates hid_report_in array
	#else
		UpdateInputReportData_MR(); // populates hid_report_in array
	#endif			
		USBInHandle = HIDTxPacket(HID_EP, (BYTE*)&hid_report_in, HID_INPUT_REPORT_BYTES);
	}					


} // end ReportLoopback


void HID_OutputReport(void)
{
    if (!HIDRxHandleBusy(USBOutHandle))	// Check if data was received from the host.
	{
		// The CPU owns the endpoint. Check for received data...

        //Re-arm the OUT endpoint for the next packet
        USBOutHandle = HIDRxPacket(HID_EP,(BYTE*)&hid_report_out, HID_OUTPUT_REPORT_BYTES);
	}
}


/** EOF generic_hid.c *************************************************/

