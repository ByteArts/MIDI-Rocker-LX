#ifndef _INC_APP
#define _INC_APP

#include "MIDI.h"

// CONDITIONAL COMPILE FLAGS ----------------------------------------------

//#define MIDI_INTERRUPT // service the MIDI UART in an interrupt, otherwise it is polled.
//#define NOTE_OFF_CLEARS_FLAG // note OFF event clears the midi channel output

#define STICKY_SORT   // for mimicing holding down the kick pedal
#define MAP_SWAP_NOTE // for swapping note maps when a special note is recieved
//#define EXT_PEDAL	  // external foot pedal switch for changing maps

#define LOG_MIDI_DATA 		// include ability to log MIDI data
#define PROCESS_HOST_CMD  	// include host command processing
#define MULTIPLE_CHANNELS_PER_NOTE // one note can trigger multiple outputs

#define USE_HIHAT_THRESHOLD // use pedal position to determine hi hat note 


// CONSTANTS --------------------------------------------------------------

#if defined(TARGET_WII)
	#define RB2_DRUMS_PRODUCT_ID	0x3110 // RB2 drums (RB1 was 0x0005)
	#define GHWT_DRUMS_PRODUCT_ID	0x0120 // GHWT drums (doesn't really exist on USB)
#else 
	// PS3
	#define RB2_DRUMS_PRODUCT_ID	0x0210 // RB2 drums
	#define GHWT_DRUMS_PRODUCT_ID	0x0120 // Guitar Hero World Tour drums
#endif

// Vendor ID for USB
#if defined(TARGET_WII)
	// Wii
	#define VENDOR_ID 	0x1BAD // Harmonix 
	#define PRODUCT_ID 	RB2_DRUMS_PRODUCT_ID // drums
#else
	// PS3
	#define VENDOR_ID 	0x12BA // Harmonix 
 	#define PRODUCT_ID  RB2_DRUMS_PRODUCT_ID
#endif

// Version Numbers
//#define BETA

#if defined(BETA)
		#pragma BETA_VERSION  // to generate a warning message
		#define MAJOR_REV_NUMBER 9
		#define MINOR_REV_NUMBER 7
#else
	#if defined(MR_LX)
		// MR LX
		#define MAJOR_REV_NUMBER 2
		#define MINOR_REV_NUMBER 5
	#else
		// original MR
		#define MAJOR_REV_NUMBER 3
		#define MINOR_REV_NUMBER 9
	#endif	
#endif

#define MAJOR_REVISION ('0' + MAJOR_REV_NUMBER)
#define MINOR_REVISION ('0' + MINOR_REV_NUMBER) 


// EEPROM addresses of various settings
#define EE_VERSION 0x01

//                        Address     Contents
#define EEADDR_SYSTEM	  0x00  // System mode
#define EEADDR_GAME_MODE  0x01	// Game Mode (Rock Band or Guitar Hero)
#define EEADDR_PCB_VER    0x02  // PCB version (FF if before V1.3)

#define EEADDR_VERSION    0x10	// EEData Version
#define EEADDR_HOLD_COUNT 0x12  // MIDI Note Duration
#define EEADDR_VEL_THRESH 0x13  // MIDI Note Velocity Threshold
#define EEADDR_SWAP_NOTE  0x14  // MIDI Note number which switches to other map
#define EEADDR_HIHAT_THRESHOLD 0x15 // Hi Hat pedal position threshold

#define EEADDR_MIDI_MAP1  0x20 // starting address of MIDI map table in EEPROM
#define EEADDR_MIDI_MAP2  (EEADDR_MIDI_MAP1 + MIDI_TABLE_SIZE)

#define SYS_MODE_PS3 	0  // Playstation 3
#define SYS_MODE_XBOX 	1  // Xbox360
#define SYS_MODE_UPDATE 2  // bootloader mode
#define SYS_MODE_WII 	3  // Wii

// button mappings
#define	GREEN_DRUM		joyBUTTON_2
#define RED_DRUM		joyBUTTON_3
#define YELLOW_DRUM		joyBUTTON_4
#define BLUE_DRUM		joyBUTTON_1
#define KICK_PEDAL		joyBUTTON_5
#define HIHAT_PEDAL		joyBUTTON_6  // hi hat pedal in Rock Band 3
#define ORANGE_CYMBAL		joyBUTTON_6  // orange cymbal in Guitar Hero

#define DRUM_FLAG		joyBUTTON_11  // combined with one of the drum buttons to signal a drum hit 	
#define CYMBAL_FLAG		joyBUTTON_12 // combined with one of the drum buttons to signal a cymbal hit 	

#if defined(TARGET_WII)

	#define BACK_BUTTON		joyBUTTON_9  // - on the Wii
	#define START_BUTTON	joyBUTTON_10 // + on the Wii
	#define SYSTEM_BUTTON	0
	#define SELECT_BUTTON	joyBUTTON_2 // green drum

#else
	
	#define BACK_BUTTON		joyBUTTON_3  // same as red drum
	#define START_BUTTON	joyBUTTON_10
	#define SYSTEM_BUTTON	joyBUTTON_13
	#define SELECT_BUTTON	joyBUTTON_9

#endif

#define gmROCK_BAND 	0
#define gmGUITAR_HERO 	1

#define HOST_CMD_BUF_SIZE 8 // make sure this is not bigger than HID_INT_OUT_EP_SIZE

// error message constants
#define ERR_VERSION 0x01
#define ERR_UART 	0x06
#define ERR_EEPROM	0x08


// GLOBAL DATA -------------------------------------------------------


extern BYTE g_SystemMode;
extern BYTE g_MidiHoldCount;
extern BYTE g_GameMode;

#ifdef PROCESS_HOST_CMD
	extern BYTE g_HostCmdBuffer[HOST_CMD_BUF_SIZE];
	extern BOOL g_HostCmdMode;
	extern BYTE g_HostCmdResponseX, g_HostCmdResponseY;
#endif

#if !defined(MR_LX)
	extern BYTE g_AdjustKnobPos;
	extern BYTE g_Dummy;
#endif


// GLOBAL FUNCTIONS -------------------------------------------------------

extern void Delay10Microsecs(UINT16 Count);
extern void DelayMillisecs(UINT16 Count);
extern void DisplayValue(UINT8 Value);
extern void ErrorMessage(UINT8 ErrorCode, BOOL Halt);
extern void InitButtonStates(void);
extern void InitReportData(void);

#if defined(LOG_MIDI_DATA)
	extern void AddDataToLog(UINT8 DataID, UINT Data);
#endif

extern void Main_PS3(void);
extern void Main_Wii(void);
extern void Main_Xbox360(void);

#ifdef PROCESS_HOST_CMD
	extern void ProcessHostCommand(void);
#endif

extern void RecallStoredSettings(void);
extern void SetPID(BYTE GameMode);

#if defined(MR_LX)
	extern BYTE DoBootModeSelect_LX(void);
	extern void InitIO_LX(void);
	extern void UpdateInputReportData_LX(void);
#else
	extern BYTE DoBootModeSelect_MR(void);
	extern void InitIO_MR(void);
	extern void UpdateInputReportData_MR(void);
#endif

#endif
