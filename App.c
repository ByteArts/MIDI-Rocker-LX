/*
MIDI ROCKER Application code module
This is the core part of the code which emulates the drum controller.
*/

#include <GenericTypeDefs.h>
#include <Compiler.h>
#include <Delays.h>
#include <Timers.h>
#include "App.h"
#include "EEData.h"
#include "MIDI.h"
#include "Pinout.h"
#include "Joystick.h"
#include "USB\usb_device.h"


// COMPILER FLAGS =====================================================

#if defined(XBOX_RB2_INTERFACE) && (!defined(TARGET_XBOX) || !defined(MR_LX))
	!!!"ERROR: XBOX_RB2_INTERFACE only valid when TARGET_XBOX and MR_LX defined"
#endif

#if defined(MIDI_OUT_ADAPTER) && defined(LX_EXT_OUTS)
	!!!"ERROR: MIDI_OUT_ADAPTER not valid when LX_EXT_OUTS defined"
#endif

//#define ARCADE_INTERFACE // special output mode for Christian Cooper

// CONSTANTS =======================================================

// default note hold count (how long a notes output is held active after a MIDI note is recieved)
#if defined(TARGET_XBOX)
	#define MIDI_HOLD_COUNT	5  // controls pulse width of external interface signal (for RB1 interface)
#else
	#define MIDI_HOLD_COUNT 5 
#endif


#define VELOCITY_INCREMENT	20

// Mode switch positions - don't change values, as these correspond to switch positions on the MR
#define MODE_PROG_VELOCITY 	0  	// velocity program mode
#define MODE_PROG_MAP 		1	// map program mode
#define MODE_PLAY  			2	// normal mode
#define MODE_PROG_DURATION 	3 	// special LX mode for changing note duration

// mapping of outputs to "drum" functions - these are bitmasks that get used with m_ChannelOutputFlags
#define ofRED_PAD 		0x0001 // OUT1 
#define ofYELLOW_PAD 	0x0002 // OUT2 - yellow cymbal on Guitar hero
#define ofBLUE_PAD 		0x0004 // OUT3
#define ofGREEN_PAD 	0x0008 // OUT4
#define ofPEDAL1 		0x0010 // OUT5
#define ofYELLOW_CYMBAL 0x0020 // OUT6
#define ofORANGE_CYMBAL	0x0020 // OUT6 - Guitar Hero orange cymbal
#define ofBLUE_CYMBAL 	0x0040 // OUT7
#define ofGREEN_CYMBAL 	0x0080 // OUT8
#define ofPEDAL2		0x0100 // OUT9 - hi hat pedal


// GLOBAL FUNCTION PROTOTYPES ====================================

#if defined(LOG_MIDI_DATA)
	void AddDataToLog(UINT8 EventType, UINT EventData);
#endif


// GLOBAL DATA =======================================================
#pragma udata

#ifdef PROCESS_HOST_CMD
	BYTE g_HostCmdBuffer[HOST_CMD_BUF_SIZE];
	BOOL g_HostCmdMode = FALSE;
	BYTE g_HostCmdResponseX, g_HostCmdResponseY, g_HostCmdResponseZ;
#endif

BYTE g_MidiHoldCount;

#ifdef MAP_SWAP_NOTE
	UINT8 g_MidiSwapNote = INVALID_NOTE_NUMBER; // note for swapping maps
#endif

BYTE g_SystemMode; // Xbox, PS, or Wii

BYTE g_GameMode = gmROCK_BAND; // Game Mode: Rock Band or Guitar Hero 

#if !defined(MR_LX)
	BYTE g_AdjustKnobPos;
	BYTE g_Dummy;
#endif


// LOCAL DATA =======================================================

static BYTE m_bSystemMode = MODE_PLAY; // Mode switch position

#if !defined(MR_LX)
	static BYTE m_bModePos = MODE_PLAY; // Mode switch position
#endif

static UINT16 m_wButtonFlags;
static BYTE m_bNavButtonFlags;

#if defined(STICKY_SORT)
	static BYTE m_bStickyButtonFlag = 0;
#endif

static INT8 m_MidiChannelToProgram = -1;
static BYTE m_MidiHoldCounts[MIDI_CHANNEL_COUNT] = 
{ 
	0, 0, 0, 0, 
	0, 0, 0, 0
};

static UINT16 m_ChannelOutputFlags = 0;

#if defined(LOG_MIDI_DATA)
	#define DATA_LOG_SIZE HID_INT_IN_EP_SIZE
	static BOOL m_DataLoggingIsEnabled = FALSE;
	static UINT8 m_DataLogCount = 0;
	static UINT8 m_DataLogSequenceID = 0;
	static UINT8 m_DataLog[DATA_LOG_SIZE];
#endif

// button state bit flags
#define	bsUP					0x00  // button is not pressed
#define bsPRESSED				0x01  // button is pressed
#define bsHELD_DOWN				0x02  // button is held down
#define	bsPRESS_RELEASED		0x04  // button released after being pressed
#define bsHOLD_RELEASED 		0x08  // button released after being held down
#define bsHELD_DOWN_LONG    	0x10  // held down for a long time
#define bsHELD_DOWN_SUPER_LONG 	0x20  // held down a really long time

typedef struct
{
	BOOL	StateChanged, Pressed;
	BYTE 	State;
	UINT16	Count;
} TButtonStatus;

// number of entries in the button state machine (1 for each user interface button)
#if defined(MR_LX)
	#if defined(EXT_PEDAL)
		#define BUTTON_COUNT 8
	#else
		#define BUTTON_COUNT 7
	#endif
#else
	#if defined(EXT_PEDAL)
		#define BUTTON_COUNT 10
	#else
		#define BUTTON_COUNT 9
	#endif
#endif

static TButtonStatus m_ButtonStatus[BUTTON_COUNT];

// indices to the button state array
#define NAV_CENTER_INDEX	0
#define NAV_LEFT_INDEX		1
#define NAV_RIGHT_INDEX		2
#define NAV_UP_INDEX		3
#define NAV_DOWN_INDEX		4
#define START_BTN_INDEX 	5  // PB1
#define BACK_BTN_INDEX 		6  // PB2
#define HOME_BTN_INDEX		7  // PB3 (MR-1 ONLY)
#define FSWITCH_INDEX		8  // Function select switch (MR-1 ONLY)
#define EXT_PEDAL_INDEX		(BUTTON_COUNT - 1) // external pedal port

// LOCAL FUNCTION PROTOTYPES ====================================

static void DoButtonStateMachine(BOOL Pressed, TButtonStatus * PState);
static void DoMidiMapping(void);
static void	DoMidiMapProgramming(void);
static BYTE ScaleVelocity(BYTE Value); 

#if defined(XBOX_RB2_INTERFACE)
	static BYTE ScaleHoldCount(BYTE Channel, BYTE Velocity);
#endif

static void SelectProgramMode(INT8 ChannelNumber);
static void UpdateButtonStates(void);

#if defined(MR_LX)
	#define SetOutput(O, A)	SetOutput_LX(O, A)

	#if defined(LX_EXT_OUTS)
	static void SetExtOutput_LX(BYTE Output, BOOL Active);
	#endif

	static void SetOutput_LX(BYTE Output, BOOL Active);
#else
	#define SetOutput(O, A)	SetOutput_MR(O, A)
	static void SetOutput_MR(BYTE Output, BOOL Active);
	static void UpdateKnobPositions(void);
#endif

// CODE =======================================================

#pragma code


/*
Add an item to the data log.
*/
#if defined(LOG_MIDI_DATA)
void AddDataToLog(UINT8 DataID, UINT Data)
{
	if (!m_DataLoggingIsEnabled)	
		return;

	/*
	The first 3 bytes of the data log are a prefix, sequence ID, and count. SendDataLog()
	will fill those values in.
	*/
	if (m_DataLogCount < DATA_LOG_SIZE - 3)
	{
		m_DataLog[m_DataLogCount++] = DataID;
		m_DataLog[m_DataLogCount++] = Data;
	}
}

/*
Copy the log data to the input report buffer.
*/
void SendDataLog(void)
{
	UINT8 nIndex;

	// if no data to send, just zero out the prefix byte
	if (m_DataLogCount == 0)
	{
		hid_report_in[0] = 0; 
		return;
	}

	// fill in log header: prefix, sequence number, and count
	hid_report_in[0] = 0xBA; // prefix
	++m_DataLogSequenceID; 
	hid_report_in[1] = m_DataLogSequenceID; // sequence number
	hid_report_in[2] = m_DataLogCount; // byte count, not including prefix

	// copy data to report buffer
	for (nIndex = 0; nIndex < m_DataLogCount; ++nIndex)
	{
		hid_report_in[3 + nIndex] = m_DataLog[nIndex];
	}
	m_DataLogCount = 0;
}

#endif  // #if defined(LOG_MIDI_DATA)


void Delay10Microsecs(UINT16 Count)
{
	/*
	Delay for the specified number of 10's of microsecs. 
	*/
	while (Count--)
	{
		// CPU is running at 48Mhz, so each instruction takes 83.33ns, so 10us = 120 instructions
		Delay10TCYx(12); 
	}
}


void DelayMillisecs(UINT16 Count)
{
	/*
	Delay for the specified number of millisecs.
	*/
	while (Count--)
	{
		Delay1KTCYx(12); // (At 48Mhz, each instruction takes 83.33ns, so 1ms = 12,000 instructions)
	}
}


/*
Turn on LEDs to indicate a value, where the value goes from 0-5 (to 6 on the LX)
*/
void DisplayValue(UINT8 Value)
{
	ledCH1 = LED_OUTPUT_OFF;
	ledCH2 = LED_OUTPUT_OFF;
	ledCH3 = LED_OUTPUT_OFF;
	ledCH4 = LED_OUTPUT_OFF;
	ledCH5 = LED_OUTPUT_OFF;
#if defined(MR_LX)
	ledCH6 = LED_OUTPUT_OFF;
#endif

	if (Value)
		ledCH1 = LED_OUTPUT_ON;
	if (Value > 1)
		ledCH2 = LED_OUTPUT_ON;
	if (Value > 2)
		ledCH3 = LED_OUTPUT_ON;
	if (Value > 3)
		ledCH4 = LED_OUTPUT_ON;
	if (Value > 4)
		ledCH5 = LED_OUTPUT_ON;
#if defined(MR_LX)
	if (Value > 5)
		ledCH6 = LED_OUTPUT_ON;
#endif
}


#if defined(MR_LX)
/*
Figure out what operating mode to use
*/
BYTE DoBootModeSelect_LX(void)
{
#if defined(MIDI_OUT_ADAPTER) && defined(TARGET_WII)
	/*
	This configuration is used with Wii and MIDI OUT option.
	*/

	// use Wii mode, means that USB is enabled
	g_SystemMode = SYS_MODE_WII;	
	
	// get default game mode
	g_GameMode = ReadEEData(EEADDR_GAME_MODE);

	// make sure g_GameMode is valid 
	if (g_GameMode > gmGUITAR_HERO)
		g_GameMode = gmGUITAR_HERO; 

	// change game mode if different...
	if ((swNAV_RIGHT == SW_PRESSED) && (g_GameMode != gmGUITAR_HERO))
	{
		g_GameMode = gmGUITAR_HERO;
		WriteEEData(EEADDR_GAME_MODE, g_GameMode);
	}
	else if ((swNAV_LEFT == SW_PRESSED) && (g_GameMode != gmROCK_BAND))
	{
		g_SystemMode = SYS_MODE_WII; 
		g_GameMode = gmROCK_BAND;
		WriteEEData(EEADDR_GAME_MODE, g_GameMode);
	}
	
	// hold down BACK button to put LX into Xbox mode to disable USB 
	if (swBACK == SW_PRESSED)
	{
		g_SystemMode = SYS_MODE_XBOX;
	}
	
#elif defined(MIDI_OUT_ADAPTER) && defined(TARGET_XBOX)

	/*
	This configuration can be used with MIDI OUT and a GHWT controller when in GH mode, or
	a MIDI PRO adapter when in RB mode. 
	*/	

	// default to Xbox mode so that USB isn't enabled
	g_SystemMode = SYS_MODE_XBOX; 

	// get default game mode
	g_GameMode = ReadEEData(EEADDR_GAME_MODE);
	if (g_GameMode > gmGUITAR_HERO)
		g_GameMode = gmROCK_BAND; 

	// change game mode if different...
	if ((swNAV_RIGHT == SW_PRESSED) && (g_GameMode != gmGUITAR_HERO))
	{
		g_GameMode = gmGUITAR_HERO;
		WriteEEData(EEADDR_GAME_MODE, g_GameMode);
	}
	else if ((swNAV_LEFT == SW_PRESSED) && (g_GameMode != gmROCK_BAND))
	{
		g_GameMode = gmROCK_BAND;
		WriteEEData(EEADDR_GAME_MODE, g_GameMode);
	}
	
	// hold down BACK button to put LX into PS3 mode for PC operation
	if (swBACK == SW_PRESSED)
	{
		g_SystemMode = SYS_MODE_PS3;
		g_GameMode = gmROCK_BAND;
	}

#else	

	// set default mode
	#if defined(TARGET_WII)
		g_SystemMode = SYS_MODE_WII;
	#elif defined(TARGET_XBOX)
		g_SystemMode = SYS_MODE_XBOX;
	#else
		g_SystemMode = SYS_MODE_PS3; 
	#endif

	if (g_SystemMode == SYS_MODE_XBOX)
	{
		// Xbox mode only supports Rock Band mode 
		g_GameMode = gmROCK_BAND;

		// hold down BACK button to put LX into PS3 mode for PC operation
		if (swBACK == SW_PRESSED)
		{
			g_SystemMode = SYS_MODE_PS3;
		}
	}
	else if (g_SystemMode == SYS_MODE_PS3)
	{	
		// get default game mode
		g_GameMode = ReadEEData(EEADDR_GAME_MODE);
	
		// set game mode depending on what buttons are pressed
		if (g_GameMode > gmGUITAR_HERO)
			g_GameMode = gmROCK_BAND; 
	
		// change game mode if different...
		if ((swNAV_RIGHT == SW_PRESSED) && (g_GameMode != gmGUITAR_HERO))
		{
			g_GameMode = gmGUITAR_HERO;
			WriteEEData(EEADDR_GAME_MODE, g_GameMode);
		}
		else if ((swNAV_LEFT == SW_PRESSED) && (g_GameMode != gmROCK_BAND))
		{
			g_GameMode = gmROCK_BAND;
			WriteEEData(EEADDR_GAME_MODE, g_GameMode);
		}
	}
	else
	{
		// must be Wii mode -- only supports Rock Band for now
		g_GameMode = gmROCK_BAND; 
	}
	
#endif
	
	return g_SystemMode;
} // DoBootModeSelect()

#else

BYTE DoBootModeSelect_MR(void)
{
	BYTE bSavedTRISB;

	/*
	Standard MIDI ROCKER -- need to examine RB6,7 to determine whether to boot in Update, PS, or
	Xbox mode.
	*/

	// configure RB6,7 as inputs, enable the weak internal pull-up
	//         76543210
	bSavedTRISB = TRISB;
	TRISB |= 0b11000000; // configure bits 7,6 as inputs
	INTCON2 &= 0b01111111; // clear bit 7 (/RBPU) to enable pull-up

	// now check the state of RB6,7 to determine the switch position

	// set default mode
	#if defined(TARGET_WII)
		g_SystemMode = SYS_MODE_WII;
	#else
		g_SystemMode = SYS_MODE_PS3; 
	#endif

	if (PORTBbits.RB7 == SW_PRESSED)
		g_SystemMode = SYS_MODE_XBOX;
	else if (PORTBbits.RB6 == SW_PRESSED)
		g_SystemMode = SYS_MODE_UPDATE; // we shouldn't get here -- bootloader should have kicked in!


	INTCON2 |= 0b10000000; // set bit 7 (/RBPU) to disable pull-up
	TRISB = bSavedTRISB; // restore value

#if defined(__DEBUG)
	#pragma COMPILED FOR DEBUGGING // to generate a warning as a reminder

	// force the mode (for testing)
	#if defined(TARGET_WII)
		g_SystemMode = SYS_MODE_WII;
	#elif defined(TARGET_XBOX)
		g_SystemMode = SYS_MODE_XBOX;
	#else
		g_SystemMode = SYS_MODE_PS3; 
	#endif

	///g_SystemMode = SYS_MODE_360;
	///g_SystemMode = SYS_MODE_UPDATE; 

#endif

	// Xbox mode only supports Rock Band mode 
	if (g_SystemMode == SYS_MODE_XBOX)
	{
		g_GameMode = gmROCK_BAND;
	}
	else if (g_SystemMode == SYS_MODE_PS3)
	{
		// get default game mode
		g_GameMode = ReadEEData(EEADDR_GAME_MODE);
	
		// set game mode depending on what buttons are pressed
		if (g_GameMode > gmGUITAR_HERO)
			g_GameMode = gmROCK_BAND; 
	
		if ((swNAV_RIGHT == SW_PRESSED) && (g_GameMode != gmGUITAR_HERO))
		{
			g_GameMode = gmGUITAR_HERO;
			WriteEEData(EEADDR_GAME_MODE, g_GameMode);
		}
		else if ((swNAV_LEFT == SW_PRESSED) && (g_GameMode != gmROCK_BAND))
		{
			g_GameMode = gmROCK_BAND;
			WriteEEData(EEADDR_GAME_MODE, g_GameMode);
		}
	}
	else 
	{
		// must be Wii mode -- only supports Rock Band for now
		g_GameMode = gmROCK_BAND; 
	}

	return g_SystemMode;
} // DoBootModeSelect()

#endif

static void DoMidiMapping(void)
{
	int nMidiInput;

	/*
	The MIDI input service routine will set a channel output when a note is recieved, so here
	we check if any MIDI notes have been received and reset the hold count. 
	*/
	for (nMidiInput = 0; nMidiInput < MIDI_CHANNEL_COUNT; ++nMidiInput)
	{
		if (g_MidiChannelOutputs[nMidiInput])
		{
			#if defined(XBOX_RB2_INTERFACE)
				/*
				For the Xbox RB2 interface, the note hold count depends on the MIDI note velocity --
				this way, the output pulse width varies as a function of the note velocity.
				*/
				if (g_SystemMode == SYS_MODE_XBOX)
				{
					m_MidiHoldCounts[nMidiInput] = ScaleHoldCount(nMidiInput, g_MidiChannelVelocity[nMidiInput]);
				}
				else
			#endif
					m_MidiHoldCounts[nMidiInput] = g_MidiHoldCount;

		}
	}

	/*
	In drum mode, a midi note ON is used to activate a "midi channel output" and set 
	the hold count for that channel (see above). So next we check the hold count of all
	the channels and if it is > 0, we activate the corresponding drum output. This way,
	each drum activation lasts for just a certain amount of time (determined by the 
	g_MidiHoldCount value), rather than waiting for the note OFF.

	The MIDI note velocity is stored in g_MidiChannelVelocity by the MIDI input service routine.
	*/

	ClearMidiOutputs(); // reset all the g_MidiChannelOutputs[] flags

	if (m_MidiHoldCounts[0] > 0)
	{
		--m_MidiHoldCounts[0];
		m_ChannelOutputFlags |= ofRED_PAD;

		// set the velocity for this channel
		hid_report_in[12] = ScaleVelocity(g_MidiChannelVelocity[0]);
	}

	if (m_MidiHoldCounts[1] > 0)
	{
		--m_MidiHoldCounts[1];
		m_ChannelOutputFlags |= ofYELLOW_PAD;

		// set the velocity for this channel
		hid_report_in[11] = ScaleVelocity(g_MidiChannelVelocity[1]);
	}	

	if (m_MidiHoldCounts[2] > 0)
	{
		--m_MidiHoldCounts[2];
		m_ChannelOutputFlags |= ofBLUE_PAD;

		// set the velocity for this channel
		hid_report_in[14] = ScaleVelocity(g_MidiChannelVelocity[2]);
	}

	if (m_MidiHoldCounts[3] > 0)
	{
		--m_MidiHoldCounts[3];
		m_ChannelOutputFlags |= ofGREEN_PAD;

		// set the velocity for this channel
		hid_report_in[13] = ScaleVelocity(g_MidiChannelVelocity[3]);
	}

	if (m_MidiHoldCounts[4] > 0)
	{
		--m_MidiHoldCounts[4];
		m_ChannelOutputFlags |= ofPEDAL1;

		// set the velocity for this channel (guitar hero only)
		if (g_GameMode == gmGUITAR_HERO)
			hid_report_in[15] = ScaleVelocity(g_MidiChannelVelocity[3]);
	}
	
#if defined(USE_HIHAT_THRESHOLD)
	// hi hat pedal
	if (g_HiHatPedalPosition >= g_HiHatThreshold)
	{
		m_ChannelOutputFlags |= ofPEDAL2;
	}
#endif

	/*
	cymbal channels
	*/
	
	if (g_GameMode == gmGUITAR_HERO)
	{
		if (m_MidiHoldCounts[5] > 0)
		{
			--m_MidiHoldCounts[5];
			m_ChannelOutputFlags |= ofORANGE_CYMBAL;
	
			// set the velocity for this channel
			hid_report_in[16] = ScaleVelocity(g_MidiChannelVelocity[5]);
		}
	}
	else // ROCK BAND cymbals
	{
		if (m_MidiHoldCounts[5] > 0)
		{
			--m_MidiHoldCounts[5];
			m_ChannelOutputFlags |= ofYELLOW_CYMBAL;
			
			// set the velocity for this channel
			hid_report_in[11] = ScaleVelocity(g_MidiChannelVelocity[5]);
		}
		
		if (m_MidiHoldCounts[6] > 0)
		{
			--m_MidiHoldCounts[6];
			m_ChannelOutputFlags |= ofBLUE_CYMBAL;
	
			// set the velocity for this channel
			hid_report_in[14] = ScaleVelocity(g_MidiChannelVelocity[6]);
		}	
		
		if (m_MidiHoldCounts[7] > 0)
		{
			--m_MidiHoldCounts[7];
			m_ChannelOutputFlags |= ofGREEN_CYMBAL;
	
			// set the velocity for this channel
			hid_report_in[13] = ScaleVelocity(g_MidiChannelVelocity[7]);
		}	
	}
} // DoMidiMapping()



static void	DoMidiMapProgramming(void)
{
	INT8 nNoteCount;

	if (m_MidiChannelToProgram < 0)
		return;

	// if a MIDI note has been received, then program that note to the selected output
	if (g_MidiOnNote != INVALID_NOTE_NUMBER)
	{
		// Append new notes to existing map
		nNoteCount = AddMidiNoteMapping(m_MidiChannelToProgram, g_MidiOnNote, g_NoteVelocity, TRUE);

		if (nNoteCount < 0) // error occured
			; // TO DO: add some sort of error indication
		else if (nNoteCount == 0) // duplicate note
			; // do nothing
		else 
		{
			// toggle the LED off/on/off
			SetOutput(m_MidiChannelToProgram, FALSE);
			DelayMillisecs(300);
			SetOutput(m_MidiChannelToProgram, TRUE);

			if (nNoteCount >= NOTES_PER_CHANNEL)
			{
#if defined(MR_LX)
				ledPROG = LED_OUTPUT_OFF;
				DelayMillisecs(300);
				ledPROG = LED_OUTPUT_ON;
#else
				// toggle the LED off/on/off
				DelayMillisecs(300);
				SetOutput(m_MidiChannelToProgram, FALSE);
				DelayMillisecs(300);
				SetOutput(m_MidiChannelToProgram, TRUE);
#endif
			}
		}

		g_MidiOnNote = INVALID_NOTE_NUMBER; // reset note so it doesn't get programmed twice!
	} // if (g_MidiOffNote != INVALID_NOTE_NUMBER)
} // DoMidiMapProgramming()



void ErrorMessage(UINT8 ErrorCode, BOOL Halt)
{
	do
	{
		ledCH1 = !(ErrorCode & 0x01);
		ledCH2 = !(ErrorCode & 0x02);
		ledCH3 = !(ErrorCode & 0x04);
		ledCH4 = !(ErrorCode & 0x08);
		ledCH5 = !(ErrorCode & 0x10);
		DelayMillisecs(1000);
		ledCH1 = LED_OUTPUT_OFF;
		ledCH2 = LED_OUTPUT_OFF;
		ledCH3 = LED_OUTPUT_OFF;
		ledCH4 = LED_OUTPUT_OFF;
		ledCH5 = LED_OUTPUT_OFF;
		DelayMillisecs(500);
	} while (Halt);	
}



/*
Initialize the "state machines" for the buttons.
*/
void InitButtonStates(void)
{
	int nIndex;

	for (nIndex = 0; nIndex < BUTTON_COUNT; ++nIndex)
	{
		m_ButtonStatus[nIndex].Pressed = FALSE;
		m_ButtonStatus[nIndex].State = bsUP;
		m_ButtonStatus[nIndex].Count = 0;
	}
}


/*
Initialize all the I/O pins and registers for the MIDI Rocker
*/

#define LED_DELAY 150

#if defined(MR_LX)
void InitIO_LX(void)
{
	/*
	Configure I/O pins
	*/

	ADCON1 = 0x0F; // configure A/D for digital inputs
	CMCON = 0x07; // configure comparator for digital input

	// PORT A
	// Bit  Usage
	// 0	(EXT0)
	// 1	(EXT1)
	// 2	(EXT2)
	// 3	IN1 (SW_HOME) 
	// 4	IN2 (SW_START)
	// 5	IN3 (SW_NAV_DOWN)
	// 6	-
	// 7	-
#if defined(LX_EXT_OUTS)
	#if defined(XBOX_RB2_INTERFACE)
	//             76543210
		LATA  &= 0b11111000; // set ext outputs (EXT2-EXT0) low for RB2 interface
	#else
	//             76543210
		LATA  |= 0b00000111; // set ext outputs (EXT2-EXT0) high for RB1 interface
	#endif
	//         76543210
	TRISA |= 0b00111000; // configure bits 543 as inputs
	TRISA &= 0b11111000; // configure bits 210 as outputs
#else
	//         76543210
	TRISA |= 0b00111111; // configure bits 543210 as inputs
#endif

	// PORT B
	// Bit  Usage
	// 0	OUT7  (LED7)
	// 1	OUT8  (LED8)
	// 2	OUT9  (LED9)
	// 3	OUT10 (LED10)
	// 4	OUT11 (LED11)
	// 5	OUT12 (LED12)
	// 6	debugger (EXT7)
	// 7	debugger (external pedal switch)
#if defined(LX_EXT_OUTS)
	//         76543210
	LATB  |= 0b00111111; // set all LED outputs high (turns off LEDs)
	
	#if defined(XBOX_RB2_INTERFACE)
		//         76543210
		LATB  &= 0b10111111; // set RB6 (EXT7) low for RB2 interface
	#else
		//         76543210
		LATB  |= 0b01000000; // set RB6 (EXT7) high for RB1 interface
	#endif
	//         76543210
	TRISB &= 0b10000000; // configure bits 654 3210 as outputs
#else
	//         76543210
	LATB  |= 0b00111111; // set outputs high
	TRISB &= 0b11000000; // configure bits 54 3210 as outputs
#endif
#if defined(EXT_PEDAL)
	// enable internal pull up on port B to pull up the pedal input
	INTCON2 &= 0b01111111; // clear bit 7 (/RBPU) to enable pull-up
#endif

	// PORT C 
	// Bit  Usage
	// 0	IN7	(SW_NAV_RIGHT)	
	// 1	(EXT3)
	// 2	(EXT4)
	// 3	
	// 4	USB D-
	// 5	USB D+
	// 6	TX (MIDI OUT)
	// 7	RX (MIDI IN)
#if defined(LX_EXT_OUTS)
	#if defined(XBOX_RB2_INTERFACE)
		//         76543210
		LATC  &= 0b11111001; // set RC1,2 (EXT3,4) outputs low
	#else
		//         76543210
		LATC  |= 0b00000110; // set RC1,2 (EXT3,4) outputs high
	#endif
	//         76543210
	TRISC |= 0b00000001; // configure bits 0 as input
	TRISC &= 0b11111001; // configure bits 21 as outputs
#else
	//         76543210
	TRISC |= 0b11000111; // configure bits 76,210 as inputs - UART handles direction of TX,RX pins 
#endif

	// PORT D 
	// Bit  Usage
	// 0	(EXT5)
	// 1	(EXT6)
	// 2	OUT1
	// 3	OUT2
	// 4	OUT3
	// 5	OUT4
	// 6	OUT5
	// 7	OUT6
#if defined(LX_EXT_OUTS)
	//        76543210
	LATD |= 0b11111100; // set all LED outputs high
	#if defined(XBOX_RB2_INTERFACE)
		//        76543210
		LATD &= 0b11111100; // set EXT1-6 outputs low for RB2 interface
	#else
		LATD |= 0b00000011; // set EXT1-6 outputs high for RB1 interface
	#endif
	//        76543210
	TRISD = 0b00000000; // bits 7654 3210 are outputs
#else
	//        76543210
	LATD |= 0b11111100; // set outputs high
	TRISD = 0b00000011; // bits 7654 32 are outputs, rest are inputs
#endif

	// PORT E 
	// Bit  Usage
	// 0	IN4 (SW_NAV_LEFT)
	// 1	IN5 (SW_NAV_UP)
	// 2	IN6 (SW_NAV_CTR)
	// 3	-
	// 4	-
	// 5	-
	// 6	-
	// 7	-
	TRISE |= 0b00000111; // bits 210 are inputs

	/*
	Put on a little light show...
	*/
	ledUSB = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledUSB = LED_OUTPUT_OFF;

	ledCH1 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH1 = LED_OUTPUT_OFF;

	ledCH2 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH2 = LED_OUTPUT_OFF;

	ledCH3 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH3 = LED_OUTPUT_OFF;

	ledCH4 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH4 = LED_OUTPUT_OFF;

	ledCH5 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH5 = LED_OUTPUT_OFF;

	ledCH6 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH6 = LED_OUTPUT_OFF;

	ledALT = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledALT = LED_OUTPUT_OFF;

	ledM1 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledM1 = LED_OUTPUT_OFF;

	ledM2 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledM2 = LED_OUTPUT_OFF;

	ledPROG = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledPROG = LED_OUTPUT_OFF;

	ledMIDI = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledMIDI = LED_OUTPUT_OFF;

	#if 0 // defined(LX_EXT_OUTS) 
	// test aux outputs
	// while (1)
	{
		SetExtOutput_LX(0, TRUE);
		DelayMillisecs(500);
		SetExtOutput_LX(0, FALSE);

		SetExtOutput_LX(1, TRUE);
		DelayMillisecs(500);
		SetExtOutput_LX(1, FALSE);

		SetExtOutput_LX(2, TRUE);
		DelayMillisecs(500);
		SetExtOutput_LX(2, FALSE);

		SetExtOutput_LX(3, TRUE);
		DelayMillisecs(500);
		SetExtOutput_LX(3, FALSE);

		SetExtOutput_LX(4, TRUE);
		DelayMillisecs(500);
		SetExtOutput_LX(4, FALSE);

		SetExtOutput_LX(5, TRUE);
		DelayMillisecs(500);
		SetExtOutput_LX(5, FALSE);

		SetExtOutput_LX(6, TRUE);
		DelayMillisecs(500);
		SetExtOutput_LX(6, FALSE);

		SetExtOutput_LX(7, TRUE);
		DelayMillisecs(500);
		SetExtOutput_LX(7, FALSE);

	}
	#endif

} // InitIO_LX()

#else

// standard MIDI ROCKER
void InitIO_MR(void) 
{
	/*
	Configure I/O pins
	*/


	// PORT A
	// Bit  Usage
	// 0	Mode	(AN0)
	// 1	Velocity (AN1)
	// 2	Instrument
	// 3	IN1
	// 4	IN2
	// 5	IN3
	// 6
	// 7
	//         76543210
	TRISA |= 0b00111111; // configure bits 543210 as inputs
	
	ADCON1 = 0b00001101; // Voltage Reference = VSS,VDD Analog Inputs = AN1,AN0
	ADCON0 = AD_CH1; // select channel
	ADCON2 = 0b00111110;  // 0x3E; // set A/D acq time, clock source
	ADCON0 |= AD_ENABLE; // enable A/D
	ADCON0 |= AD_GO; // start A/D

	// PORT B
	// Bit  Usage
	// 0	ledCH4
	// 1	ledCH5
	// 2	outKEY6
	// 3	USB_ATTACH
	// 4	LED0
	// 5	LED1
	// 6	outKEY7
	// 7
	//         76543210
	LATB  |= 0b01110111; // set bits 654 210
	TRISB &= 0b10001000; // configure bits 654 210 as outputs

	// PORT C 
	// Bit  Usage
	// 0	IN7		
	// 1	GPAD1
	// 2	GPAD2
	// 3	
	// 4	USB D-
	// 5	USB D+
	// 6	MIDI out
	// 7	MIDI in
	//         76543210
	TRISC |= 0b00000111; // configure bits 210 as inputs

	// PORT D 
	// Bit  Usage
	// 0	GPAD3
	// 1	NAV UP
	// 2	NAV DN
	// 3	NAV LEFT
	// 4	NAV RIGHT
	// 5	ledCH1
	// 6	ledCH2
	// 7	ledCH3
	//        76543210
	LATD |= 0b11100000; // set bits 765
	TRISD = 0b00011111; // bits 765 are outputs, rest are inputs

	// PORT E 
	// Bit  Usage
	// 0	IN4
	// 1	IN5
	// 2	IN6
	// 3	
	// 4	
	// 5	
	// 6
	// 7
	TRISE |= 0b00000111; // bits 210 are inputs

	outEXT6 = 0;
	outEXT7 = 0;

	/*
	Put on a little light show...
	*/

	ledUSB = LED_OUTPUT_ON;
	ledMIDI = LED_OUTPUT_ON;

	ledCH1 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH1 = LED_OUTPUT_OFF;

	ledCH2 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH2 = LED_OUTPUT_OFF;

	ledCH3 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH3 = LED_OUTPUT_OFF;

	ledCH4 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH4 = LED_OUTPUT_OFF;

	ledCH5 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH5 = LED_OUTPUT_OFF;

	ledCH4 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH4 = LED_OUTPUT_OFF;

	ledCH3 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH3 = LED_OUTPUT_OFF;

	ledCH2 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH2 = LED_OUTPUT_OFF;

	ledCH1 = LED_OUTPUT_ON;
	DelayMillisecs(LED_DELAY);
	ledCH1 = LED_OUTPUT_OFF;

	ledUSB = LED_OUTPUT_OFF;
	ledMIDI = LED_OUTPUT_OFF;

	#if 0 // test aux outputs
	while (1)
	{
		outEXT6 = 1;
		DelayMillisecs(1000);
		outEXT6 = 0;
		outEXT7 = 1;
		DelayMillisecs(1000);
		outEXT7 = 0;
	}
	#endif

} // InitIO_MR()
#endif


/*
Set the HID report data to default values
*/
void InitReportData(void)
{
	// Fill in HID report data with default values
	hid_report_in[0] = 0; 	// Button States (button 1 to 8) 
	hid_report_in[1] = 0;	// Button States (buttons 9 to 13)
	hid_report_in[2] = joyHAT_CENTER;	// hat switch position
	hid_report_in[3] = joyAXIS_CENTER;  // X axis
	hid_report_in[4] = joyAXIS_CENTER;	// Y axis
	hid_report_in[5] = joyAXIS_CENTER;	// Z axis
	hid_report_in[6] = joyAXIS_CENTER;	// Rz axis
	hid_report_in[7] = 0;
	hid_report_in[8] = 0;
	hid_report_in[9] = 0;
	hid_report_in[10] = 0;
	hid_report_in[11] = 0;	
	hid_report_in[12] = 0;	
	hid_report_in[13] = 0;	
	hid_report_in[14] = 0;	
	hid_report_in[15] = 0;
	hid_report_in[16] = 0;

	if (g_GameMode == gmGUITAR_HERO)
	{
		hid_report_in[17] = 0;
		hid_report_in[18] = 0;
		hid_report_in[19] = 0;
		hid_report_in[20] = 0;
		hid_report_in[21] = 0;
		hid_report_in[22] = 0;
		hid_report_in[23] = 0;
		hid_report_in[24] = 0;
		hid_report_in[25] = 0;
		hid_report_in[26] = 2;
	}
	else // ROCK BAND
	{
		hid_report_in[17] = 0;
		hid_report_in[18] = 0;
		hid_report_in[19] = 2;
		hid_report_in[20] = 0;
		hid_report_in[21] = 2;
		hid_report_in[22] = 0;
		hid_report_in[23] = 2;
		hid_report_in[24] = 0;
		hid_report_in[25] = 2;
		hid_report_in[26] = 0;
	}
} // InitReportData()



#if defined(TARGET_WII)
/*
Do infinite loop for Wii mode.
*/
void Main_Wii(void)
{
    while (1)
    {
		// Check bus status and service USB interrupts.
        USBDeviceTasks();     // Interrupt or polling method
        
		// Application-specific tasks.
        ProcessIO();        
    }//end while
}

#else

/*
Do infinite loop for PS3 mode.
*/
void Main_PS3(void)
{
    while (1)
    {
		// Check bus status and service USB interrupts.
        USBDeviceTasks();     // Interrupt or polling method
        
		// Application-specific tasks.
        ProcessIO();        
    }//end while
}

#endif


/*
Do infinite loop for Xbox mode. We don't need to service the USB, because we are only
using the USB to get power.
*/

/*
Time count is set to simulate the USB poll rate of 100Hz.
One timer count is 51.2µsecs.

The RB2 interface loop must run at a much higher rate than the standard 100Hz in order
generate the proper pulse widths required. (Someday this should all be done in an interrupt).
*/

#if defined(XBOX_RB2_INTERFACE)
	#define TIMER_POLL_COUNT 5   // Poll Rate = 256usecs
#else
	#define TIMER_POLL_COUNT 192 // Poll Rate = 9.8msecs
#endif

void Main_Xbox360(void)
{
	// turn on the USB LED to indicate 360 mode
	ledUSB = LED_OUTPUT_ON;

	/*
	Setup timer 0, used to simulate the poll rate of the USB
	*/
	OpenTimer0(TIMER_INT_OFF & T0_16BIT & T0_SOURCE_INT & T0_PS_1_256); 

	while (TRUE)
	{
#if defined(MR_LX) // used for debugging
		UpdateInputReportData_LX(); // poll switches, MIDI data etc... generate HID report data
#else
		UpdateInputReportData_MR(); // poll switches, MIDI data etc... generate HID report data
#endif
		WriteTimer0(0); // reset timer
		
		// wait for timer to expire, check MIDI UART in the meantime
		while (ReadTimer0() < TIMER_POLL_COUNT)
		{
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
		}
	} // while (TRUE)
}


#ifdef PROCESS_HOST_CMD

// host command bytes           	  Description        Parameters              Output
#define dcCLEAR           	 	0  //  exit cmd mode         none                X,Y = default values
#define dcGET_VERSION      		1  //  get version           none                X,Y = major,minor
#define dcSET_XY           		2  //  sets XY               X,Y            	 X,Y = specified values
#define dcGET_NOTE_MAPPING 		3  //  returns midi note     channel,index       X = midi note
#define dcSET_NOTE_MAPPING 		4  //  sets note map         channel,index,note  none
#define dcSET_OUTPUT       		5  //  turn output on/off    output,on           none
#define dcGET_INPUT        		6  //  get input value       input               X = 0 or 1
#define dcGET_HOLD_COUNT   		7  //  get note duration     none                X = duration
#define dcSET_HOLD_COUNT   		8  //  set note duration     value               none
#define dcGET_TABLE_SIZE   		9  //  returns size of map   none                X = flag count Y = notes per flag
#define dcGET_ADJUST_KNOB 		10 //  return knob position  none	     		 X = position
#define dcSET_LOGGING     		11 //  enable/disable log    X (on/off)		     none
#define dcREAD_EEPROM	  		12 //  read eeprom	       	 Address		 	 X = eeprom data value
#define dcWRITE_EEPROM	  		13 //  write eeprom	       	 Address, Data 	     none
#define dcGET_VEL_THRESH  		14 //  get min note velocity none				 X = min velocity
#define dcSET_VEL_THRESH  		15 //  set min note velocity Value (0-127)       none
#define dcGET_MAP_COUNT   		16 //  get max map count     none                X = map count
#define dcGET_MAP_NUMBER  		17 //  get midi map number   none                X = map number
#define dcSET_MAP_NUMBER  		18 //  set midi map number   map number (0-1)    none
#define dcGET_SWAP_NOTE   		19 //  get map swap note     none                X = note number
#define dcSET_SWAP_NOTE	  		20 //  set map swap note     note number         none
#define dcGET_HIHAT_THRESHOLD 	21 //  get hi hat threshold  none                X = threshold value
#define dcSET_HIHAT_THRESHOLD   22 //  set hi hat threshold  threshold			 none		 
#define dcGET_FEATURES			23 //  get device features   none				 X,Y = feature bits
#define dcSET_GAME_MODE			24 //  set game mode         value				 none

/*
Process a command from the host. The command and parameters are in g_HostCmdBuffer:
Buffer Index    Value
0				prefix (should always be BA)
1				command ID number
2				command sequence number (MIDI Rocker needs to return the same value)
3-7				command parameters (X,Y...)
*/
void ProcessHostCommand(void)
{
	BYTE nParam;

	// first check for proper cmd prefix	
	if (g_HostCmdBuffer[0] != 0xBA)
		return;

	/*
	Command responses are sent in the XYZ axis values. The Z axis value is set to the 
	sequence number of the command we are responding to (as provided by the host).
	*/
	g_HostCmdResponseX = 0; 
	g_HostCmdResponseY = 0; 			
	g_HostCmdResponseZ = g_HostCmdBuffer[2]; 

	g_HostCmdMode = TRUE;

	// act on the command
	switch (g_HostCmdBuffer[1]) 
	{
		case dcCLEAR: // Command 0: Clear cmd flag. Returns input report back to default value
			g_HostCmdMode = FALSE;
#if defined(LOG_MIDI_DATA)
			m_DataLoggingIsEnabled = FALSE;
#endif
			break;

		case dcGET_VERSION:
			g_HostCmdResponseX = MAJOR_REV_NUMBER;
			g_HostCmdResponseY = MINOR_REV_NUMBER; 			
			break;

		case dcSET_XY:
			g_HostCmdResponseX = g_HostCmdBuffer[3];
			g_HostCmdResponseY = g_HostCmdBuffer[4]; 			
			break;
			
		case dcGET_NOTE_MAPPING: // Get Midi Note Mapping, Param1 = channel number, Param2 = note index
			g_HostCmdResponseX = GetMidiMapEntry(g_HostCmdBuffer[3], g_HostCmdBuffer[4]);
			break;

		case dcSET_NOTE_MAPPING: // Set Midi Map entry. Param1 = channel number, Param2 = note index, Param3 = note
			SetMidiMapEntry(g_HostCmdBuffer[3], g_HostCmdBuffer[4], g_HostCmdBuffer[5]);
			break;

		case dcSET_OUTPUT: // turn on/off the specified output, Param1 = output number, Param2 = on/off
			SetOutput(g_HostCmdBuffer[3], g_HostCmdBuffer[4]);
			break;

		case dcGET_HOLD_COUNT:
			g_HostCmdResponseX = g_MidiHoldCount;
			break;

		case dcSET_HOLD_COUNT:
			g_MidiHoldCount = g_HostCmdBuffer[3];
			WriteEEData(EEADDR_HOLD_COUNT, g_MidiHoldCount);
			break;

		case dcGET_TABLE_SIZE:
			g_HostCmdResponseX = MIDI_CHANNEL_COUNT;
			g_HostCmdResponseY = NOTES_PER_CHANNEL; 			
			break;
			
		case dcGET_ADJUST_KNOB:
#if defined(MR_LX)
			g_HostCmdResponseX = 0;
#else
			g_HostCmdResponseX = g_AdjustKnobPos;
#endif
			break;

#if defined(LOG_MIDI_DATA)
		case dcSET_LOGGING:
			m_DataLoggingIsEnabled = (g_HostCmdBuffer[3]);
			break;
#endif

		case dcGET_VEL_THRESH:
			g_HostCmdResponseX = g_MinVelocity;
			break;

		case dcSET_VEL_THRESH:
			g_MinVelocity = g_HostCmdBuffer[3];
			WriteEEData(EEADDR_VEL_THRESH, g_MinVelocity);
			break;

		case dcGET_MAP_COUNT:
			g_HostCmdResponseX = MIDI_MAP_COUNT;
			break;

		case dcGET_MAP_NUMBER:
			g_HostCmdResponseX = g_MidiMapNumber;
			break;

		case dcSET_MAP_NUMBER:
			SetMidiMapNumber(g_HostCmdBuffer[3], TRUE);
			break;
			
#ifdef MAP_SWAP_NOTE
		case dcGET_SWAP_NOTE:
			g_HostCmdResponseX = g_MidiSwapNote;
			break;
			
		case dcSET_SWAP_NOTE:
			g_MidiSwapNote = g_HostCmdBuffer[3];
			WriteEEData(EEADDR_SWAP_NOTE, g_MidiSwapNote);
			break;
			
#endif			

#ifdef USE_HIHAT_THRESHOLD
		case dcGET_HIHAT_THRESHOLD:
			g_HostCmdResponseX = g_HiHatThreshold;
			break;
			
		case dcSET_HIHAT_THRESHOLD:
			g_HiHatThreshold = g_HostCmdBuffer[3];
			WriteEEData(EEADDR_HIHAT_THRESHOLD, g_HiHatThreshold);
			break;
#endif

		case dcGET_FEATURES:
			// hardware option bits in g_HostCmdResponseX
			g_HostCmdResponseX = 0x01; // default
			#if defined(MR_LX)
				g_HostCmdResponseX |= 0x02;
			#endif
			#if defined(LX_EXT_OUTS)
				g_HostCmdResponseX |= 0x04;
			#endif
			#if defined(XBOX_RB2_INTERFACE)
				g_HostCmdResponseX |= 0x08;
			#endif
			#if defined(MIDI_OUT_ADAPTER)
				g_HostCmdResponseX |= 0x10;
			#endif
						
			// put software option bits in g_HostCmdResponseY
			#if defined(LOG_MIDI_DATA)
				g_HostCmdResponseY |= 0x01;
			#endif
			#ifdef MAP_SWAP_NOTE
				g_HostCmdResponseY |= 0x02;
			#endif			
			#ifdef USE_HIHAT_THRESHOLD
				g_HostCmdResponseY |= 0x04;
			#endif			
			break;
			
		case dcSET_GAME_MODE:
			// this will just affect the format of the HID report data, NOT the PID or VID
			g_GameMode = g_HostCmdBuffer[3] & 0x01; // valid value is either 0 or 1
			
			// save in EEPROM for next time
			WriteEEData(EEADDR_GAME_MODE, g_GameMode);
			break;
			
		default:
			// unknown command - put invalid value in Z
			g_HostCmdResponseZ = !g_HostCmdBuffer[2]; 
			break;
	} 
}
#endif


/*
Get stored settings from EEPROM. Initializes values to defaults if old settings are found.
*/
void RecallStoredSettings(void)
{
	/*
	Get stored values from eeprom
	*/
	if (ReadEEData(EEADDR_VERSION) == EE_VERSION) // check stored version number
	{
		g_MidiHoldCount = ReadEEData(EEADDR_HOLD_COUNT); 
		g_MinVelocity = ReadEEData(EEADDR_VEL_THRESH);
		
	#ifdef MAP_SWAP_NOTE
		g_MidiSwapNote = ReadEEData(EEADDR_SWAP_NOTE);
	#endif
	
	#ifdef USE_HIHAT_THRESHOLD
		g_HiHatThreshold = ReadEEData(EEADDR_HIHAT_THRESHOLD);
	#endif
	}
	else // invalid version, so reset to defaults
	{
		ErrorMessage(ERR_VERSION, FALSE);

		g_MidiHoldCount	= MIDI_HOLD_COUNT;
		WriteEEData(EEADDR_HOLD_COUNT, g_MidiHoldCount);

		g_MinVelocity = DEFAULT_VELOCITY_THRESHOLD;
		WriteEEData(EEADDR_VEL_THRESH, g_MinVelocity);

	#ifdef MAP_SWAP_NOTE
		g_MidiSwapNote = INVALID_NOTE_NUMBER;
		WriteEEData(EEADDR_SWAP_NOTE, g_MidiSwapNote);
	#endif		

	#ifdef USE_HIHAT_THRESHOLD
		g_HiHatThreshold = INVALID_NOTE_NUMBER; // default to disabled
		WriteEEData(EEADDR_HIHAT_THRESHOLD, g_HiHatThreshold);
	#endif
			
		// init maps to defaults
		SetMidiMapNumber(0, FALSE); // to set g_MidiMapEEPROMAddress
		RestoreDefaultMap(0);
		SetMidiMapNumber(1, FALSE); // to set g_MidiMapEEPROMAddress
		RestoreDefaultMap(1);		
		
		// update stored version number
		WriteEEData(EEADDR_VERSION, EE_VERSION);
	}
}


#if defined(XBOX_RB2_INTERFACE)
/*
Computes the proper hold count depending on the velocity and channel. For the Rock 
Band 2 Xbox interface, the hold count is used to control the pulse width of the 
external output signals in order to mimic the user hitting a pad or cymbal with 
different amounts of force. These values were determined thru trial and error.
*/
static BYTE ScaleHoldCount(BYTE Channel, BYTE Velocity)
{
	BYTE Result;

	switch (Channel)
	{
		case 4: // kick pedal 
			// It's not velocity sensitive, but some versions of the controller seem to need 
			// a longer pulse.
			return 20 * g_MidiHoldCount; //was 64; 	

		// cymbals
		case 5:
		case 6:
		case 7:
			Result = (Velocity / 5) + 5;
			if (Result > 25)			
				Result = 25;
			return Result;

		// drum pads
		default:
			return (Velocity / 12);
	} // switch
}
#endif

/*
Convert MIDI note velocity value into value needed by game
*/
static BYTE ScaleVelocity(BYTE Value)
{
	if (g_GameMode == gmGUITAR_HERO)
		return Value;
	else
		return (0xFF - (2 * Value));
}


/*
Puts the specified output into "program mode" and resets the midi note data. The output is asserted
and the corresponding LED is turned on. The ChannelNumber specifies which output. If ChannelNumber
is -1, then no outputs are to be put in program mode.
*/
static void SelectProgramMode(INT8 ChannelNumber)
{
	// turn off all the outputs and LEDs
	ledCH1 = LED_OUTPUT_OFF;
	ledCH2 = LED_OUTPUT_OFF;
	ledCH3 = LED_OUTPUT_OFF;
	ledCH4 = LED_OUTPUT_OFF;
	ledCH5 = LED_OUTPUT_OFF;
#if defined(MR_LX)
	ledCH6 = LED_OUTPUT_OFF; 
	ledALT = LED_OUTPUT_OFF;
#else
	ledCH2 = LED_OUTPUT_OFF;  // fixes a glitch when in 360 mode. Note sure why.
	outEXT6 = 0;
	outEXT7 = 0;
#endif

	g_MidiOnNote = INVALID_NOTE_NUMBER;
	g_MidiOffNote = INVALID_NOTE_NUMBER;

	/*
	Channels 0-3 are the drums, 4 is the kick, and	5-7 are the cymbals.
	*/
	m_MidiChannelToProgram = ChannelNumber;

	// assert the output -- turns on the corresponding LED	
	SetOutput(ChannelNumber, TRUE);

#if defined(MR_LX) // turn on/off the PROG indicator
	if (ChannelNumber < 0)
		ledPROG = LED_OUTPUT_OFF;
	else
		ledPROG = LED_OUTPUT_ON;
#endif
} // SelectProgramMode()


#if defined(LX_EXT_OUTS)

/*
Sets the state of an external output on the LX. For the RB1 inteface, all the signals
are active low, for RB2 the drums/cymbals are active-high, except the kick is active low.
*/
static void SetExtOutput_LX(BYTE Output, BOOL Active)
{
	BOOL boolHigh;

#if defined(XBOX_RB2_INTERFACE)
	boolHigh = (Active != 0); // outputs are active high (except kick pedal) 

	switch (Output)
	{
		case 0: // RED
			outEXT0 = boolHigh;
			outEXT0 = boolHigh; // it seems to take 2 times
			break;

		case 1: // YELLOW
			outEXT1 = boolHigh;
			break;

		case 2: // BLUE
			outEXT2 = boolHigh;
			break;

		case 3: // GREEN
			outEXT3 = boolHigh;
			break;

		case 4: // KICK
			outEXT4 = !boolHigh; // pedal is active low
			break;

		// NOTE: the yellow and blue cymbal connections are swapped in the board cable, so we
		// swap them here to compensate.
		
		case 5: // YELLOW CYMBAL
			outEXT6 = boolHigh;
			break;

		case 6: // BLUE CYMBAL
			outEXT5 = boolHigh;
			break;

		case 7: // GREEN CYMBAL
			outEXT7 = boolHigh;
			break;

	} // switch (Output)

#else // not XBOX_RB2_INTERFACE, must be RB1
		
	boolHigh = (Active == 0); // outputs are active-low

	switch (Output)
	{
		case 0: // RED
			outEXT0 = boolHigh;
			outEXT0 = boolHigh; // it seems to take 2 times
			break;

		case 1: // YELLOW
			outEXT1 = boolHigh;
			break;

		case 2: // BLUE
			outEXT2 = boolHigh;
			break;

		case 3: // GREEN
			outEXT3 = boolHigh;
			break;

		case 4: // KICK
			outEXT4 = boolHigh;
			break;
			
		// go ahead and activate the cymbal outputs too. They are not normally connected when
		// using the RB1 interface, but there is a customer that needs this.
		
		// NOTE: the yellow and blue cymbal connections are swapped in the interface board, so we
		// swap them here to compensate.
		case 5: // YELLOW CYMBAL
			outEXT6 = boolHigh;
			break;

		case 6: // BLUE CYMBAL
			outEXT5 = boolHigh;
			break;

		case 7: // GREEN CYMBAL
			outEXT7 = boolHigh;
			break;
	} // switch (Output)

	#endif
}
#endif


/*
Changes the USB product ID number so that the controller is identified as the desired type
of game controller. This is done when the controller is first starting up, before the USB
is initialized.
*/
void SetPID(BYTE GameMode)
{
	switch (GameMode)
	{
		case gmGUITAR_HERO:
			device_dsc.idProduct = GHWT_DRUMS_PRODUCT_ID;
			break;

		case gmROCK_BAND:
		default:
			device_dsc.idProduct = RB2_DRUMS_PRODUCT_ID;
			break;
	}
}



/*
Turn on LEDs or outputs to active the specified output channel.
*/
#if defined(MR_LX)
static void SetOutput_LX(BYTE Output, BOOL Active)
{
	switch (Output)
	{
		case 0:  // red drum
			if (Active)
				ledCH1 = LED_OUTPUT_ON;
			else
				ledCH1 = LED_OUTPUT_OFF;
			break;

		case 1: // yellow drum
			if (Active)
				ledCH2 = LED_OUTPUT_ON;
			else
				ledCH2 = LED_OUTPUT_OFF;
			break;

		case 2: // blue drum
			if (Active)
				ledCH3 = LED_OUTPUT_ON;
			else
				ledCH3 = LED_OUTPUT_OFF;
			break;

		case 3: // green drum
			if (Active)
				ledCH4 = LED_OUTPUT_ON;
			else
				ledCH4 = LED_OUTPUT_OFF;
			break;

		case 4: // bass pedal (orange)
			if (Active)
				ledCH5 = LED_OUTPUT_ON;
			else
				ledCH5 = LED_OUTPUT_OFF;
			break;

		case 5: 
			if (g_GameMode == gmGUITAR_HERO) // orange GH cymbal
			{
				if (Active)
					ledCH6 = LED_OUTPUT_ON;
				else
					ledCH6 = LED_OUTPUT_OFF;
			}
			else // yellow RB cymbal
			{
				if (Active)
					ledCH2 = LED_OUTPUT_ON;
				else
					ledCH2 = LED_OUTPUT_OFF;
				ledALT = ledCH2;
			}
			break;

		case 6: // blue RB cymbal
			if (Active)
				ledCH3 = LED_OUTPUT_ON;
			else
				ledCH3 = LED_OUTPUT_OFF;
			ledALT = ledCH3;
			break;

		case 7: // green RB cymbal
			if (Active)
				ledCH4 = LED_OUTPUT_ON;
			else
				ledCH4 = LED_OUTPUT_OFF;
			ledALT = ledCH4;
			break;
			
		case 8: // RB hi hat pedal
			if (Active)
				ledCH6 = LED_OUTPUT_ON;
			else
				ledCH6 = LED_OUTPUT_OFF;
			break;
	} // switch (Output)

} // SetOutput_LX()

#else

static void SetOutput_MR(BYTE Output, BOOL Active)
{
	switch (Output)
	{
		case 0:  // red drum
			if (Active)
				ledCH1 = LED_OUTPUT_ON;
			else
				ledCH1 = LED_OUTPUT_OFF;
			break;

		case 1: // yellow drum
			if (Active)
				ledCH2 = LED_OUTPUT_ON;
			else
				ledCH2 = LED_OUTPUT_OFF;
			break;

		case 2: // blue drum
			if (Active)
				ledCH3 = LED_OUTPUT_ON;
			else
				ledCH3 = LED_OUTPUT_OFF;
			break;

		case 3: // green drum
			if (Active)
				ledCH4 = LED_OUTPUT_ON;
			else
				ledCH4 = LED_OUTPUT_OFF;
			break;

		case 4: // kick pedal 
			if (Active)
				ledCH5 = LED_OUTPUT_ON;
			else
				ledCH5 = LED_OUTPUT_OFF;
			break;

		/*
		Outputs 5-8 are special cases on the original MIDI Rocker since there aren't enough
		LEDs. When in program mode, we use one of the 5 LEDs to indicate that one of these special
		channels is being programmed, otherwise no LEDs are used.
		*/

		case 5: // yellow cymbal in RB, or orange cymbal in GH
			if ((g_SystemMode != SYS_MODE_XBOX) || (m_bSystemMode != MODE_PLAY))
			{
				if (g_GameMode == gmGUITAR_HERO) // orange GH cymbal
				{
					if (Active)
						ledCH5 = LED_OUTPUT_ON;
					else
						ledCH5 = LED_OUTPUT_OFF;
				}
				else // yellow RB cymbal
				{
					if (Active)
						ledCH2 = LED_OUTPUT_ON;
					else
						ledCH2 = LED_OUTPUT_OFF;
				}
			}
			break;

		case 6: // blue RB cymbal
			if ((g_SystemMode != SYS_MODE_XBOX) || (m_bSystemMode != MODE_PLAY))
			{
				if (Active)
					ledCH3 = LED_OUTPUT_ON;
				else
					ledCH3 = LED_OUTPUT_OFF;
			}
			// activate external output
			if (Active)
				outEXT6 = 1;
			else
				outEXT6 = 0;
			break;

		case 7: // green RB cymbal
			if ((g_SystemMode != SYS_MODE_XBOX) || (m_bSystemMode != MODE_PLAY))
			{
				if (Active)
					ledCH4 = LED_OUTPUT_ON;
				else
					ledCH4 = LED_OUTPUT_OFF;
			}
			// activate external output
			if (Active)
				outEXT7 = 1;
			else
				outEXT7 = 0;
			break;
			
		case 8: // RB hi hat pedal - uses same LED as kick pedal
			if (Active)
				ledCH5 = LED_OUTPUT_ON;
			else
				ledCH5 = LED_OUTPUT_OFF;
			break;
			

	} // switch (Output)
}
#endif // if MR_LX..else

// button press counts (16bit value)
#if defined(XBOX_RB2_INTERFACE)
	// Xbox loop runs much faster, so counts have to be longer
	#define PRESS_COUNT 	80   // how long button has to be pressed (or released) before the state changes
	#define HOLD_COUNT  	2000 // how long a button has to be pressed before it's considered held down
	#define LONG_HOLD_COUNT 6000 // long press hold count
	#define SUPER_LONG_HOLD_COUNT 16000 // super long press count 
#else
	#define PRESS_COUNT 	4   // how long button has to be pressed (or released) before the state changes
	#define HOLD_COUNT  	100 // how long a button has to be pressed before it's considered held down
	#define LONG_HOLD_COUNT 300	// long press hold count
	#define SUPER_LONG_HOLD_COUNT 800 // super long press count 
#endif

static void DoButtonStateMachine(BOOL Pressed, TButtonStatus * PState)
{
/*
Given the current state of the button (pressed or not), this routine updates the button status depending
on how long it has been in the current state, what the previous state was etc...
*/
	BYTE bsState;

	PState->StateChanged = FALSE; // default to no change
	bsState = PState->State; // current state

	// if switch has changed position, reset the count
	if (Pressed != PState->Pressed)
	{
		PState->Count = 0;
		PState->Pressed = Pressed;

		if (!Pressed) // no longer pressed
		{
			// if switch was pressed/held, then change it to be released
			if (bsState & bsHELD_DOWN)
				bsState = bsHOLD_RELEASED;
			else if (bsState & bsPRESSED)
				bsState = bsPRESS_RELEASED;
		}
	}
	else // switch hasn't changed position
	{
		// increment count
		if (PState->Count < SUPER_LONG_HOLD_COUNT)
			++PState->Count;

		// if not pressed long enough, then don't update any further
		if (PState->Count < PRESS_COUNT)
			return;

		if (Pressed) // button is pressed
		{
			// if pressed for long enough, then mark it as held down
			if (bsState & bsPRESSED)
			{
				if (PState->Count >= SUPER_LONG_HOLD_COUNT) 
					bsState |= bsHELD_DOWN_SUPER_LONG; // really long hold
				if (PState->Count >= LONG_HOLD_COUNT)
					bsState |= bsHELD_DOWN_LONG; // long hold
				else if (PState->Count >= HOLD_COUNT)
					bsState |= bsHELD_DOWN; // short hold
			}
			else 
				bsState = bsPRESSED;
		}
		else // button not pressed
		{
			if (bsState & (bsPRESS_RELEASED | bsHOLD_RELEASED)) // if was released, then now it is up
				bsState = bsUP;
			else if (bsState & bsHELD_DOWN) // if it was held down, then now it is released
				bsState = bsHOLD_RELEASED;
			else if (bsState & bsPRESSED) // if it was pressed, then now it is released
				bsState = bsPRESS_RELEASED;
		}
	}

	// update state if changed
	if (bsState != PState->State)
	{
		PState->State = bsState;
		PState->StateChanged = TRUE;
	}
}


/*
Checks the buttons and updates the states of each one. Each button is like a mini state machine.
*/
static void UpdateButtonStates(void)
{
	BYTE bCurrentPosition;

	DoButtonStateMachine(swNAV_CENTER == SW_PRESSED, &m_ButtonStatus[NAV_CENTER_INDEX]);
	DoButtonStateMachine(swNAV_LEFT == SW_PRESSED, &m_ButtonStatus[NAV_LEFT_INDEX]);
	DoButtonStateMachine(swNAV_RIGHT == SW_PRESSED, &m_ButtonStatus[NAV_RIGHT_INDEX]);
	DoButtonStateMachine(swNAV_UP == SW_PRESSED, &m_ButtonStatus[NAV_UP_INDEX]);
	DoButtonStateMachine(swNAV_DOWN == SW_PRESSED, &m_ButtonStatus[NAV_DOWN_INDEX]);
	DoButtonStateMachine(swSTART == SW_PRESSED, &m_ButtonStatus[START_BTN_INDEX]);
	DoButtonStateMachine(swBACK == SW_PRESSED, &m_ButtonStatus[BACK_BTN_INDEX]);

#if !defined(MR_LX)
	// do extra buttons on the MR-1
	DoButtonStateMachine(swHOME == SW_PRESSED, &m_ButtonStatus[HOME_BTN_INDEX]);
	DoButtonStateMachine(swFUNCTION == SW_PRESSED, &m_ButtonStatus[FSWITCH_INDEX]);
#endif

#if defined(EXT_PEDAL)
	DoButtonStateMachine(swEXT_PEDAL == SW_PRESSED, &m_ButtonStatus[EXT_PEDAL_INDEX]);
#endif
}


#if defined(MR_LX)
void UpdateButtonFlags_LX(void)
{
	/*
	Check the status of the button state machine and update the button bit flags accordingly.
	This is for PLAY mode only.
	*/	
	if (g_HostCmdMode)
	{
		/*
		In host command mode, just report the current state. In 
		normal mode, the button action may depend on whether the button is held down or not.
		*/
	
		// START button ('+' on the Wii)
		if (m_ButtonStatus[START_BTN_INDEX].State & bsPRESSED)
			m_wButtonFlags |= START_BUTTON;
	
		// BACK button ('-' on the Wii)
		if (m_ButtonStatus[BACK_BTN_INDEX].State & bsPRESSED)
			m_wButtonFlags |= BACK_BUTTON;
		
		// nav center button
		if (m_ButtonStatus[NAV_CENTER_INDEX].State & bsPRESSED)
			m_wButtonFlags |= SELECT_BUTTON;
	
		// check nav switch direction
		if (m_ButtonStatus[NAV_LEFT_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_LEFT;
		if (m_ButtonStatus[NAV_RIGHT_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_RIGHT;
		if (m_ButtonStatus[NAV_UP_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_UP;
		if (m_ButtonStatus[NAV_DOWN_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_DOWN;
	}
	else // normal play mode
	{
		// quick-change map function is activated by pressing the UP and BACK
		if ((m_ButtonStatus[BACK_BTN_INDEX].StateChanged)
		&&  (m_ButtonStatus[BACK_BTN_INDEX].State & bsPRESSED) 
		&&  (m_ButtonStatus[NAV_UP_INDEX].State & bsPRESSED))
		{
			// change the active map
			g_MidiMapNumber = (g_MidiMapNumber + 1) % MIDI_MAP_COUNT;			
			SetMidiMapNumber(g_MidiMapNumber, TRUE);
			return;
		}
				
		// START button ('+' on the Wii) - activate when pressed
		if (m_ButtonStatus[START_BTN_INDEX].State & bsPRESSED)
			m_wButtonFlags |= START_BUTTON;
	
		// BACK button ('-' on the Wii) - activate when pressed
		if (m_ButtonStatus[BACK_BTN_INDEX].State & bsPRESSED)
			m_wButtonFlags |= BACK_BUTTON;
		
		// if CENTER button pressed and released, then activate the green drum
		if (m_ButtonStatus[NAV_CENTER_INDEX].State & bsPRESS_RELEASED)
			m_wButtonFlags |= GREEN_DRUM;
		// if CENTER button held down, then active SELECT
		else if (m_ButtonStatus[NAV_CENTER_INDEX].State & bsHELD_DOWN)
			m_wButtonFlags |= SELECT_BUTTON; // Note: does nothing on Wii

		// check nav switch direction
		else if (m_ButtonStatus[NAV_LEFT_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_LEFT;
		else if (m_ButtonStatus[NAV_RIGHT_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_RIGHT;
		else if (m_ButtonStatus[NAV_UP_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_UP;
		else if (m_ButtonStatus[NAV_DOWN_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_DOWN;
	
	#if defined(STICKY_SORT)
		/*
		In Rock Band you have to hold down the kick pedal to activate the "sort menu". On a real
		drumkit there is no way to hold down the pedal (it is a percussion), so we simulate this feature 
		by holding the NAV button to the LEFT. There is also a "make a setlist" feature which is 
		activated by holding down the X button -- this is simulated by holding the NAV button to the RIGHT.
		*/
		// if nav LEFT held down, then activate kick (this is for sorting in Rock Band)
		if (m_ButtonStatus[NAV_LEFT_INDEX].State & bsHELD_DOWN)
		{
			m_bNavButtonFlags = joyHAT_CENTER;
			m_bStickyButtonFlag = KICK_PEDAL;
		}
		// if nav RIGHT held down, activate the Green drum (same as X on a PS) for making a playlist
		else if (m_ButtonStatus[NAV_RIGHT_INDEX].State & bsHELD_DOWN)
		{
			m_bNavButtonFlags = joyHAT_CENTER;
			m_bStickyButtonFlag = GREEN_DRUM;
		}
		// cancel the sort feature if button pressed to right
		else if ((m_bStickyButtonFlag == KICK_PEDAL) && (m_ButtonStatus[NAV_RIGHT_INDEX].State & bsPRESSED))
		{ 
			m_bStickyButtonFlag = 0;
		}
		// cancel the playlist feature if button pressed to left
		else if ((m_bStickyButtonFlag == GREEN_DRUM) && (m_ButtonStatus[NAV_LEFT_INDEX].State & bsPRESSED))
		{ 
			m_bStickyButtonFlag = 0;
		}
		
		m_wButtonFlags |= m_bStickyButtonFlag;
	#endif
	
		// Check for BACK button held down to activate PS button
		if (m_ButtonStatus[BACK_BTN_INDEX].State & bsHELD_DOWN) 
		{
			m_wButtonFlags = SYSTEM_BUTTON;	
		}
	} // if g_HostCmdMode... else
} // UpdateButtonFlags_LX


void UpdateInputReportData_LX(void)
{
#if defined(LOG_MIDI_DATA)
	/*
	In logging mode, the input report data is filled in with the log data instead
	of the normal input report.
	*/
	if (m_DataLoggingIsEnabled)
	{
		SendDataLog();
		return;
	}
#endif	

  	/*
	The input report hid_report_in has the following structure:

	ROCK BAND 2 DRUMS

	Byte	Description
	0		button states - buttons 1-8	(1 bit for each button)
	1 		button states - buttons 9-13 (1 bit for each button)

	2 		hat switch (4 LSBs)

	3		X axis position - (used in host command mode)
	4		Y axis position - (used in host command mode)
    5		Z axis position - (used in host command mode)
	6		Rz axis position - not used

	11		Yellow drum/cymbal velocity
	12		Red drum/cymbal velocity
	13		Green drum/cymbal velocity
	14		Blue drum/cymbal velocity


	GUITAR HERO DRUMS
	Byte	Description
	0		button states - buttons 1-8	(1 bit for each button)
	1 		button states - buttons 9-13 (1 bit for each button)

	2 		hat switch (4 LSBs)

	3		X axis position - (used in host command mode)
	4		Y axis position - (used in host command mode)
    5		Z axis position - (used in host command mode)
	6		Rz axis position - not used

	11		Yellow cymbal velocity
	12		Red drum velocity
	13		Green drum velocity
	14		Blue drum velocity
	15		Pedal velocity
	16		Orange cymbal velocity

	*/

	InitReportData(); // set values to defaults

	// clear button bit flags -- these are copied to the report buffer later..
	m_wButtonFlags = 0;
	m_bNavButtonFlags = joyHAT_CENTER; // default to center

	// clear output bit flags -- these get set by button presses or mapped MIDI notes
	m_ChannelOutputFlags = 0;

	/*
	Check button press states
	*/
	UpdateButtonStates(); // updates the state machine

	/*
	Check for play/prog mode change by looking for START button to be held down.
	*/
	if (!g_HostCmdMode && m_ButtonStatus[START_BTN_INDEX].StateChanged)
	{
		if (m_ButtonStatus[START_BTN_INDEX].State & bsHELD_DOWN)
		{
			if (m_bSystemMode == MODE_PLAY)
			{
				// go to map programming mode
				m_bSystemMode = MODE_PROG_MAP;
				SelectProgramMode(0);
			}
			else
			{
				// exit program mode, go to play mode
				m_bSystemMode = MODE_PLAY;
				SelectProgramMode(-1); // update LED display for normal mode
				SetMidiMapNumber(g_MidiMapNumber, FALSE); // update LED display
			}
		}
	}
	
	/*
	Force play mode if host command mode is enabled. 
	*/
	if (g_HostCmdMode && (m_bSystemMode != MODE_PLAY))
	{
		m_bSystemMode = MODE_PLAY;
		SelectProgramMode(-1); // exit program mode
	}

	if (m_bSystemMode == MODE_PLAY)
	{
		// turn on PROG LED to indicate host mode 
		if (g_HostCmdMode)
			ledPROG = LED_OUTPUT_ON;
		else
			ledPROG = LED_OUTPUT_OFF;

		/* 
		PLAY MODE ==========================================
		Normal mode (not program mode). Just "play" notes according to what keys are pressed and/or
		what MIDI notes are received.
		*/
		
#ifdef MAP_SWAP_NOTE
		// Check for MIDI note used to swap maps
		if ((g_MidiSwapNote != INVALID_NOTE_NUMBER) && (g_MidiOnNote == g_MidiSwapNote))
		{
			// change the active map
			g_MidiMapNumber = (g_MidiMapNumber + 1) % MIDI_MAP_COUNT;			
			SetMidiMapNumber(g_MidiMapNumber, TRUE);
			g_MidiOnNote = INVALID_NOTE_NUMBER;
		}
#endif

#if defined(EXT_PEDAL) // Note to self - the ICD will cause this to happen at reset
		// check for ext pedal switch pressed - used to swap maps
 		if (m_ButtonStatus[EXT_PEDAL_INDEX].StateChanged && (m_ButtonStatus[EXT_PEDAL_INDEX].State == bsPRESSED))
		{
			// change the active map
			g_MidiMapNumber = (g_MidiMapNumber + 1) % MIDI_MAP_COUNT;			
			SetMidiMapNumber(g_MidiMapNumber, TRUE);
			g_MidiOnNote = INVALID_NOTE_NUMBER;
		}
#endif
		
		UpdateButtonFlags_LX(); // update flags for play mode
		
		/*
		Check for a MIDI input - updates m_ChannelOutputFlags, 
		fills in velocity values in hid report array.
		*/
		m_MidiChannelToProgram = -1;
		DoMidiMapping(); // updates m_ChannelOutputFlags

	#if defined(STICKY_SORT)
		// clear the sort flag if a pedal note is hit or the start, back or home buttons pressed
		if ((m_ChannelOutputFlags & ofPEDAL1) || (m_wButtonFlags & (START_BUTTON | BACK_BUTTON)))
			m_bStickyButtonFlag = 0;

		// clear the sort flag if nav center was pressed -- but need to wait until AFTER it is pressed
 		if (m_ButtonStatus[NAV_CENTER_INDEX].StateChanged && (m_ButtonStatus[NAV_CENTER_INDEX].State == bsUP))
			m_bStickyButtonFlag = 0;
	#endif
	
	
		/*
		Turn on/off indicator lights.
		*/
		if (g_GameMode == gmGUITAR_HERO)
		{
			// Guitar Hero mode (6 channels)
			SetOutput(0, m_ChannelOutputFlags & ofRED_PAD);
			SetOutput(1, m_ChannelOutputFlags & ofYELLOW_PAD); // yellow cymbal on GH
			SetOutput(2, m_ChannelOutputFlags & ofBLUE_PAD);
			SetOutput(3, m_ChannelOutputFlags & ofGREEN_PAD);
			SetOutput(4, m_ChannelOutputFlags & ofPEDAL1);
			SetOutput(5, m_ChannelOutputFlags & ofORANGE_CYMBAL); // orange cymbal on GH
		}
		else 
		{
			// Rock Band mode (5 channels plus 3 cymbals)
			SetOutput(0, (m_ChannelOutputFlags & ofRED_PAD));
			SetOutput(1, (m_ChannelOutputFlags & (ofYELLOW_PAD | ofYELLOW_CYMBAL)));
			SetOutput(2, (m_ChannelOutputFlags & (ofBLUE_PAD | ofBLUE_CYMBAL)));
			SetOutput(3, (m_ChannelOutputFlags & (ofGREEN_PAD | ofGREEN_CYMBAL)));
			SetOutput(4, (m_ChannelOutputFlags & ofPEDAL1));
			SetOutput(8, (m_ChannelOutputFlags & ofPEDAL2) != 0); // hi hat pedal in RB3

			// turn on ALT LED if any of the cymbals is active
			if (m_ChannelOutputFlags & (ofYELLOW_CYMBAL | ofBLUE_CYMBAL | ofGREEN_CYMBAL))
				ledALT = LED_OUTPUT_ON;
			else
				ledALT = LED_OUTPUT_OFF;

	#if defined(LX_EXT_OUTS) 
			/*
			Activate the external outputs for the Xbox interface. The RB1 and RB2 interfaces are a 
			little different since RB1 doesn't have the cymbals.
			
			The main drum pads and kick pedal are activated by a mapped MIDI note (which sets a bit in 
			m_ChannelOutputFlags), OR by one of the front panel buttons so that the user can navigate 
			the menus on the Xbox using the buttons.
			*/
		#if defined(XBOX_RB2_INTERFACE)
			// drum pad outputs
			SetExtOutput_LX(0, (m_ChannelOutputFlags & ofRED_PAD)    || (m_wButtonFlags & BACK_BUTTON));		
			SetExtOutput_LX(1, (m_ChannelOutputFlags & ofYELLOW_PAD) || (m_bNavButtonFlags == joyHAT_UP));
			SetExtOutput_LX(2, (m_ChannelOutputFlags & ofBLUE_PAD)   || (m_bNavButtonFlags == joyHAT_DOWN));
			SetExtOutput_LX(3, (m_ChannelOutputFlags & ofGREEN_PAD)  || (m_wButtonFlags & GREEN_DRUM));
			
			// activate pedal output when sticky flag is set too
			SetExtOutput_LX(4, (m_ChannelOutputFlags & ofPEDAL1) || (m_bStickyButtonFlag == KICK_PEDAL));

			// RB2 cymbal outputs
			SetExtOutput_LX(5, m_ChannelOutputFlags & ofYELLOW_CYMBAL);
			SetExtOutput_LX(6, m_ChannelOutputFlags & ofBLUE_CYMBAL);
			SetExtOutput_LX(7, m_ChannelOutputFlags & ofGREEN_CYMBAL);
			
		#elif defined(ARCADE_INTERFACE)
		
			// for arcade interface, pads and cymbals are separate
			SetExtOutput_LX(0, (m_ChannelOutputFlags & ofRED_PAD)    || (m_wButtonFlags & BACK_BUTTON));		
			SetExtOutput_LX(1, (m_ChannelOutputFlags & ofYELLOW_PAD) || (m_bNavButtonFlags == joyHAT_UP));
			SetExtOutput_LX(2, (m_ChannelOutputFlags & ofBLUE_PAD)   || (m_bNavButtonFlags == joyHAT_DOWN));
			SetExtOutput_LX(3, (m_ChannelOutputFlags & ofGREEN_PAD)  || (m_wButtonFlags & START_BUTTON));
			
			// activate pedal output when sticky flag is set too
			SetExtOutput_LX(4, (m_ChannelOutputFlags & ofPEDAL1) || (m_bStickyButtonFlag == KICK_PEDAL));
			
			// RB2 cymbal outputs
			SetExtOutput_LX(5, m_ChannelOutputFlags & ofYELLOW_CYMBAL);
			SetExtOutput_LX(6, m_ChannelOutputFlags & ofBLUE_CYMBAL);
			SetExtOutput_LX(7, m_ChannelOutputFlags & ofGREEN_CYMBAL);
			
		#else
		
			// for RB1, cymbals use same outputs as pads
			SetExtOutput_LX(0, (m_ChannelOutputFlags & ofRED_PAD)                        || (m_wButtonFlags & BACK_BUTTON));		
			SetExtOutput_LX(1, (m_ChannelOutputFlags & (ofYELLOW_PAD | ofYELLOW_CYMBAL)) || (m_bNavButtonFlags == joyHAT_UP));
			SetExtOutput_LX(2, (m_ChannelOutputFlags & (ofBLUE_PAD | ofBLUE_CYMBAL))     || (m_bNavButtonFlags == joyHAT_DOWN));
			SetExtOutput_LX(3, (m_ChannelOutputFlags & (ofGREEN_PAD | ofGREEN_CYMBAL))   || (m_wButtonFlags & START_BUTTON));
			
			// activate pedal output when sticky flag is set too
			SetExtOutput_LX(4, (m_ChannelOutputFlags & ofPEDAL1) || (m_bStickyButtonFlag == KICK_PEDAL));
		#endif
	#endif
		} 

	}
	else if (m_bSystemMode == MODE_PROG_VELOCITY) // PROG1 mode is used to adjust the velocity threshold
	{
		int nLevel;

		ledPROG = LED_OUTPUT_ON;
		ledALT = LED_OUTPUT_ON;
		ledM1 = LED_OUTPUT_OFF;
		ledM2 = LED_OUTPUT_OFF;
		DisplayValue(g_MinVelocity / VELOCITY_INCREMENT);

		// if BACK button (button 2) is held down, reset velocity to default
		if (m_ButtonStatus[BACK_BTN_INDEX].StateChanged && (m_ButtonStatus[BACK_BTN_INDEX].State & bsHELD_DOWN))
		{
			g_MinVelocity = DEFAULT_VELOCITY_THRESHOLD;
			WriteEEData(EEADDR_VEL_THRESH, g_MinVelocity);
		}

		/*
		The NAV UP/DOWN button is used to change the velocity setting		
		*/
		nLevel = (g_MinVelocity / VELOCITY_INCREMENT);

		if (m_ButtonStatus[NAV_UP_INDEX].StateChanged && (m_ButtonStatus[NAV_UP_INDEX].State == bsPRESSED))
		{
			// decrement velocity
			if (nLevel > 1)
				g_MinVelocity = (nLevel - 1) * VELOCITY_INCREMENT;
			else
				g_MinVelocity = 0;

			WriteEEData(EEADDR_VEL_THRESH, g_MinVelocity);
		}
		else if (m_ButtonStatus[NAV_DOWN_INDEX].StateChanged && (m_ButtonStatus[NAV_DOWN_INDEX].State == bsPRESSED))
		{
			// increment velocity
			if (nLevel < 6)
				g_MinVelocity = (nLevel + 1) * VELOCITY_INCREMENT;
			else
				g_MinVelocity = 6 * VELOCITY_INCREMENT; // max MIDI velocity is 127

			WriteEEData(EEADDR_VEL_THRESH, g_MinVelocity);
		}

		// If NAV CENTER button is held down, then go to PROG3 mode
		if (m_ButtonStatus[NAV_CENTER_INDEX].StateChanged 
		&& (m_ButtonStatus[NAV_CENTER_INDEX].State & bsHELD_DOWN))
		{
			m_bSystemMode = MODE_PROG_DURATION; // go to note duration edit mode
		}
	}
	else if (m_bSystemMode == MODE_PROG_MAP)
	{
		/* 
		MR LX PROGRAM MODE (MODE_PROG_MAP) -- program MIDI maps
		*/

		/*
		The NAV LEF/RIGHT button is used to select which map is used	
		*/
		if (m_ButtonStatus[NAV_LEFT_INDEX].StateChanged && (m_ButtonStatus[NAV_LEFT_INDEX].State == bsPRESSED))
		{
			if (g_MidiMapNumber > 0)
			{
				--g_MidiMapNumber;
				SetMidiMapNumber(g_MidiMapNumber, TRUE);
				SelectProgramMode(0);
			}
		}
		else if (m_ButtonStatus[NAV_RIGHT_INDEX].StateChanged && (m_ButtonStatus[NAV_RIGHT_INDEX].State == bsPRESSED))
		{
			if (g_MidiMapNumber < MIDI_MAP_COUNT - 1)
			{
				++g_MidiMapNumber;
				SetMidiMapNumber(g_MidiMapNumber, TRUE);
				SelectProgramMode(0);
			}
		}
		
		/*
		The NAV UP/DOWN button is used to select which output to program		
		*/
		if (m_ButtonStatus[NAV_UP_INDEX].StateChanged && (m_ButtonStatus[NAV_UP_INDEX].State == bsPRESSED))
		{
			if (m_MidiChannelToProgram > 0)
			{
				--m_MidiChannelToProgram;
				SelectProgramMode(m_MidiChannelToProgram);
			}
		}
		else if (m_ButtonStatus[NAV_DOWN_INDEX].StateChanged && (m_ButtonStatus[NAV_DOWN_INDEX].State == bsPRESSED))
		{
			// max channel index in guitar hero mode is 5
			if (g_GameMode == gmGUITAR_HERO)
			{
				if (m_MidiChannelToProgram < 5)
				{
					++m_MidiChannelToProgram;
					SelectProgramMode(m_MidiChannelToProgram);
				}
			}
			else if (m_MidiChannelToProgram < MIDI_CHANNEL_COUNT - 1)
			{
				++m_MidiChannelToProgram;
				SelectProgramMode(m_MidiChannelToProgram);
			}
		}

		// if BACK button held down for a super long time, reset map to default
		if (m_ButtonStatus[BACK_BTN_INDEX].StateChanged && (m_ButtonStatus[BACK_BTN_INDEX].State & bsHELD_DOWN_SUPER_LONG))
		{
			DisplayValue(0x0F);
			DelayMillisecs(75);
			
			DisplayValue(0x00);
			DelayMillisecs(75);
			
			DisplayValue(0x0F);
			DelayMillisecs(75);
			
			DisplayValue(0x00);
			DelayMillisecs(75);
			
			DisplayValue(0x0F);
			DelayMillisecs(75);
			
			RestoreDefaultMap(g_MidiMapNumber);
			
			m_bSystemMode = MODE_PLAY;
			SelectProgramMode(-1); // exit program mode
		}
		// if BACK button held down for a long time, erase the whole map
		else if (m_ButtonStatus[BACK_BTN_INDEX].StateChanged && (m_ButtonStatus[BACK_BTN_INDEX].State & bsHELD_DOWN_LONG))
		{
			UINT8 nChannel;
			
			// erase all channels
			for (nChannel = 0; nChannel < MIDI_CHANNEL_COUNT; ++nChannel)
			{
				ledPROG = LED_OUTPUT_OFF;
				DisplayValue(0x0F);
				DelayMillisecs(50);

				ClearMidiMapChannel(nChannel);

				ledPROG = LED_OUTPUT_ON;
				DisplayValue(0x00);
				DelayMillisecs(50);
			}
			
			// restore LEDs to proper state
			SelectProgramMode(m_MidiChannelToProgram);
		}
		// if BACK button is held down, clear out the map for this output
		else if (m_ButtonStatus[BACK_BTN_INDEX].StateChanged && (m_ButtonStatus[BACK_BTN_INDEX].State & bsHELD_DOWN))
		{
			ClearMidiMapChannel(m_MidiChannelToProgram);
			ledPROG = LED_OUTPUT_OFF;
			DelayMillisecs(250);
			ledPROG = LED_OUTPUT_ON;
		}

		// check for MIDI data, assign to the channel
		DoMidiMapProgramming(); // uses m_MidiChannelToProgram, updates g_MidiOnNote and velocity data		

		// If NAV CENTER button is held down, then go to velocity program mode
		if (m_ButtonStatus[NAV_CENTER_INDEX].StateChanged 
		&& (m_ButtonStatus[NAV_CENTER_INDEX].State & bsHELD_DOWN))
		{
			SelectProgramMode(-1); // exit MIDI program mode
			m_bSystemMode = MODE_PROG_VELOCITY; // goto velocity threshold edit mode
		}
	} // if (m_bSystemMode == MODE_PROG_MAP)
	else if (m_bSystemMode == MODE_PROG_DURATION) 
	{
		/*
		LX PROG MODE 3 - for changing the note duration
		*/
		ledPROG = LED_OUTPUT_ON;
		ledALT = LED_OUTPUT_OFF;
		ledM1 = LED_OUTPUT_OFF;
		ledM2 = LED_OUTPUT_OFF;
		DisplayValue(g_MidiHoldCount);

		// if BACK button is held down, reset value to default
		if (m_ButtonStatus[BACK_BTN_INDEX].StateChanged && (m_ButtonStatus[BACK_BTN_INDEX].State & bsHELD_DOWN))
		{
			g_MidiHoldCount = MIDI_HOLD_COUNT;
			WriteEEData(EEADDR_HOLD_COUNT, g_MidiHoldCount);
		}

		/*
		The NAV UP/DOWN button is used to change the note duration setting		
		*/

		if (m_ButtonStatus[NAV_UP_INDEX].StateChanged && (m_ButtonStatus[NAV_UP_INDEX].State == bsPRESSED))
		{
			// decrement value
			if (g_MidiHoldCount > 1)
				--g_MidiHoldCount;

			WriteEEData(EEADDR_HOLD_COUNT, g_MidiHoldCount);
		}
		else if (m_ButtonStatus[NAV_DOWN_INDEX].StateChanged && (m_ButtonStatus[NAV_DOWN_INDEX].State == bsPRESSED))
		{
			// increment value
			if (g_MidiHoldCount < 6)
				++g_MidiHoldCount;

			WriteEEData(EEADDR_HOLD_COUNT, g_MidiHoldCount);
		}

		// If NAV CENTER button is held down, then go to note map program mode
		if (m_ButtonStatus[NAV_CENTER_INDEX].StateChanged 
		&& (m_ButtonStatus[NAV_CENTER_INDEX].State & bsHELD_DOWN))
		{
			SetMidiMapNumber(g_MidiMapNumber, FALSE); // to turn the LEDs back on
			SelectProgramMode(0); // default to programming CH1
			m_bSystemMode = MODE_PROG_MAP;
		}
	} // else if (m_bSystemMode == MODE_PROG_DURATION)  

	/*
	Set button report bit flags according to which outputs are activated.
	*/
	if (m_ChannelOutputFlags & ofRED_PAD)
		m_wButtonFlags |= RED_DRUM;
	
	if (m_ChannelOutputFlags & ofYELLOW_PAD)
		m_wButtonFlags |= YELLOW_DRUM;

	if (m_ChannelOutputFlags & ofBLUE_PAD)
		m_wButtonFlags |= BLUE_DRUM;

	if (m_ChannelOutputFlags & ofGREEN_PAD)
		m_wButtonFlags |= GREEN_DRUM;

	if (m_ChannelOutputFlags & ofPEDAL1)
		m_wButtonFlags |= KICK_PEDAL;


	if (g_GameMode == gmGUITAR_HERO)
	{
		if (m_ChannelOutputFlags & ofORANGE_CYMBAL)
			m_wButtonFlags |= ORANGE_CYMBAL;
	}
	else // ROCK BAND
	{
		if (m_ChannelOutputFlags & ofPEDAL2)
			m_wButtonFlags |= HIHAT_PEDAL;
			
		// set the drum flag if a drum channel is active (RB mode only)
		if (m_ChannelOutputFlags & (ofRED_PAD | ofYELLOW_PAD | ofBLUE_PAD | ofGREEN_PAD))
			m_wButtonFlags |= DRUM_FLAG; // Wii - set bit 8 whenever a drum is hit

		if (m_ChannelOutputFlags & ofYELLOW_CYMBAL)
		{
			m_wButtonFlags |= YELLOW_DRUM | CYMBAL_FLAG;
			m_bNavButtonFlags = joyHAT_UP;
		}
	
		if (m_ChannelOutputFlags & ofBLUE_CYMBAL)
		{
			m_wButtonFlags |= BLUE_DRUM | CYMBAL_FLAG;
			m_bNavButtonFlags = joyHAT_DOWN;
		}
	
		if (m_ChannelOutputFlags & ofGREEN_CYMBAL)
			m_wButtonFlags |= GREEN_DRUM | CYMBAL_FLAG;
	}

	// copy bit flags to report buffer
	hid_report_in[0] = (BYTE)m_wButtonFlags; // reports buttons 1-8 
	hid_report_in[1] = (BYTE)(m_wButtonFlags >> 8); // buttons 9 thru 13
	hid_report_in[2] = m_bNavButtonFlags; // reports strummer or nav switch position

	#if defined(PROCESS_HOST_CMD)
	/*
	In host command mode we return special values in the XYZ axes and other values for diagnositics.
	*/
	if (g_HostCmdMode)
	{
		hid_report_in[3] = g_HostCmdResponseX; // send command response in X axis data
		hid_report_in[4] = g_HostCmdResponseY; // send command response in Y axis data
		hid_report_in[5] = g_HostCmdResponseZ; // send command response in Z axis data

		hid_report_in[15] = (BYTE)m_wButtonFlags;
		hid_report_in[16] = (BYTE)(m_wButtonFlags >> 8);
		hid_report_in[17] = m_bNavButtonFlags;
	}
	#endif
}
#endif // defined(MR_LX)


#if !defined(MR_LX)
void UpdateButtonFlags_MR(void)
{
	/*
	Set flags according to button status
	*/
	if (g_HostCmdMode)
	{
		/*
		In host command mode, just report the current state. In 
		normal mode, the button action may depend on whether the button is held down or not.
		*/
		// check the system button
		if (m_ButtonStatus[HOME_BTN_INDEX].State & bsPRESSED)
			m_wButtonFlags |= SYSTEM_BUTTON;

		// START button ('+' on the Wii)
		if (m_ButtonStatus[START_BTN_INDEX].State & bsPRESSED)
			m_wButtonFlags |= START_BUTTON;
	
		// BACK button ('-' on the Wii)
		if (m_ButtonStatus[BACK_BTN_INDEX].State & bsPRESSED)
			m_wButtonFlags |= BACK_BUTTON;
		
		// nav center button
		if (m_ButtonStatus[NAV_CENTER_INDEX].State & bsPRESSED)
			m_wButtonFlags |= SELECT_BUTTON;

		// check nav switch direction
		if (m_ButtonStatus[NAV_LEFT_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_LEFT;
		if (m_ButtonStatus[NAV_RIGHT_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_RIGHT;
		if (m_ButtonStatus[NAV_UP_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_UP;
		if (m_ButtonStatus[NAV_DOWN_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_DOWN;
	}
	else // normal mode
	{
		// START button ('+' on the Wii) - activate when released
		if (m_ButtonStatus[START_BTN_INDEX].State & bsPRESSED)
			m_wButtonFlags |= START_BUTTON;
	
		// BACK button ('-' on the Wii) - activate when released
		if (m_ButtonStatus[BACK_BTN_INDEX].State & bsPRESS_RELEASED)
			m_wButtonFlags |= RED_DRUM;
		
		// if CENTER button pressed and released, then activate the green drum
		if (m_ButtonStatus[NAV_CENTER_INDEX].State & bsPRESS_RELEASED)
			m_wButtonFlags |= GREEN_DRUM;
		// if CENTER button held down, then active SELECT
		else if (m_ButtonStatus[NAV_CENTER_INDEX].State & bsHELD_DOWN)
			m_wButtonFlags |= SELECT_BUTTON; // Note: does nothing on Wii
		// check nav switch direction
		else if (m_ButtonStatus[NAV_LEFT_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_LEFT;
		else if (m_ButtonStatus[NAV_RIGHT_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_RIGHT;
		else if (m_ButtonStatus[NAV_UP_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_UP;
		else if (m_ButtonStatus[NAV_DOWN_INDEX].State & bsPRESSED)
			m_bNavButtonFlags = joyHAT_DOWN;
	
		// check the system button
		if (m_ButtonStatus[HOME_BTN_INDEX].State & bsPRESSED)
			m_wButtonFlags |= SYSTEM_BUTTON;
	} // if g_HostCmdMode... else

	// check the drum buttons
	if (swDRUM1 == SW_PRESSED)
		m_ChannelOutputFlags |= ofRED_PAD;
	if (swDRUM2 == SW_PRESSED)
		m_ChannelOutputFlags |= ofYELLOW_PAD;
	if (swDRUM3 == SW_PRESSED)
		m_ChannelOutputFlags |= ofBLUE_PAD;
	if (swDRUM4 == SW_PRESSED)
		m_ChannelOutputFlags |= ofGREEN_PAD;
	if (swDRUM5 == SW_PRESSED)
		m_ChannelOutputFlags |= ofPEDAL1;
} // UpdateButtonFlags_MR


void UpdateInputReportData_MR(void)
{
/*
Polls all the switches and midi channels and updates the HID input report accordingly.
*/
#if defined(LOG_MIDI_DATA)
	/*
	In logging mode, the input report data is filled in with the log data instead
	of the normal input report.
	*/
	if (m_DataLoggingIsEnabled)
	{
		SendDataLog();
		return;
	}
#endif	

  	/*
	The input report hid_report_in has the following structure:

	ROCK BAND 2 DRUMS

	Byte	Description
	0		button states - buttons 1-8	(1 bit for each button)
	1 		button states - buttons 9-13 (1 bit for each button)

	2 		hat switch (4 LSBs)

	3		X axis position - (used in host command mode)
	4		Y axis position - (used in host command mode)
    5		Z axis position - (used in host command mode)
	6		Rz axis position - not used

	11		Yellow drum/cymbal velocity
	12		Red drum/cymbal velocity
	13		Green drum/cymbal velocity
	14		Blue drum/cymbal velocity


	GUITAR HERO DRUMS
	Byte	Description
	0		button states - buttons 1-8	(1 bit for each button)
	1 		button states - buttons 9-13 (1 bit for each button)

	2 		hat switch (4 LSBs)

	3		X axis position - (used in host command mode)
	4		Y axis position - (used in host command mode)
    5		Z axis position - (used in host command mode)
	6		Rz axis position - not used

	11		Yellow cymbal velocity
	12		Red drum velocity
	13		Green drum velocity
	14		Blue drum velocity
	15		Pedal velocity
	16		Orange cymbal velocity

	*/

	InitReportData(); // set values to defaults

	// clear button bit flags -- these are copied to the report buffer later..
	m_wButtonFlags = 0;
	m_bNavButtonFlags = joyHAT_CENTER;

	// clear output bit flags -- these get set by button presses or mapped MIDI notes
	m_ChannelOutputFlags = 0;

	/*
	Check button press states
	*/
	UpdateButtonStates();

	/*
	Get position of analog knobs (mode, velocity)
	*/
	UpdateKnobPositions();	

	if (!g_HostCmdMode)
	{
		// check if function switch has moved -- change map if so
		if (m_ButtonStatus[FSWITCH_INDEX].StateChanged)
		{
			// function switch determines which map to use
			if (m_ButtonStatus[FSWITCH_INDEX].Pressed)
				g_MidiMapNumber = 1;
			else
				g_MidiMapNumber = 0;
		
			SetMidiMapNumber(g_MidiMapNumber, TRUE); // gets saved map
		}

		// check if mode has changed
		if (m_bSystemMode != m_bModePos)
		{
			SelectProgramMode(-1);
			m_bSystemMode = m_bModePos;
		}
	}

	/*
	Force play mode if host command mode is enabled. 
	*/
	if (g_HostCmdMode && (m_bSystemMode != MODE_PLAY))
	{
		m_bSystemMode = MODE_PLAY;
		SelectProgramMode(-1);
	}

	if (m_bSystemMode == MODE_PLAY)
	{
#ifdef MAP_SWAP_NOTE
		// Check for MIDI note used to swap maps
		if ((g_MidiSwapNote != INVALID_NOTE_NUMBER) && (g_MidiOnNote == g_MidiSwapNote))
		{
			// change the active map
			g_MidiMapNumber = (g_MidiMapNumber + 1) % MIDI_MAP_COUNT;			
			SetMidiMapNumber(g_MidiMapNumber, TRUE);
			g_MidiOnNote = INVALID_NOTE_NUMBER;
		}
#endif

#if defined(EXT_PEDAL) 
		// check for ext pedal switch pressed - used to swap maps
 		if (m_ButtonStatus[EXT_PEDAL_INDEX].StateChanged && (m_ButtonStatus[EXT_PEDAL_INDEX].State == bsPRESSED))
		{
			// change the active map
			g_MidiMapNumber = (g_MidiMapNumber + 1) % MIDI_MAP_COUNT;			
			SetMidiMapNumber(g_MidiMapNumber, TRUE);
			g_MidiOnNote = INVALID_NOTE_NUMBER;
		}
#endif

		/* 
		PLAY MODE ==========================================
		Normal mode (not program mode). Just "play" notes according to what keys are pressed and/or
		what MIDI notes are received.
		*/
		UpdateButtonFlags_MR(); // updates m_wButtonFlags,  
		
		/*
		Check for a MIDI input - updates m_ChannelOutputFlags, m_bNavButtonFlags
		fills in velocity values in hid report array.
		*/
		m_MidiChannelToProgram = -1;
		DoMidiMapping(); // updates m_ChannelOutputFlags

		/*
		Turn on/off indicator lights.
		*/
		if (g_SystemMode == SYS_MODE_XBOX)
		{	
			// Drum pad outputs - activated by m_ChannelOutputFlags or some of the buttons. The
			// buttons are used to activate outputs so that they can be used for navigation
			// in the game when using a Xbox controller
			SetOutput(0, (m_ChannelOutputFlags & ofRED_PAD)    || (m_wButtonFlags & BACK_BUTTON));		
			SetOutput(1, (m_ChannelOutputFlags & ofYELLOW_PAD) || (m_bNavButtonFlags == joyHAT_UP));
			SetOutput(2, (m_ChannelOutputFlags & ofBLUE_PAD)   || (m_bNavButtonFlags == joyHAT_DOWN));
			SetOutput(3, (m_ChannelOutputFlags & ofGREEN_PAD)  || (m_wButtonFlags & GREEN_DRUM));
			SetOutput(4, m_ChannelOutputFlags & ofPEDAL1);
			
			SetOutput(5, m_ChannelOutputFlags & ofYELLOW_CYMBAL);
			SetOutput(6, m_ChannelOutputFlags & ofBLUE_CYMBAL);
			SetOutput(7, m_ChannelOutputFlags & ofGREEN_CYMBAL);
		}
		else
		{
			if (g_GameMode == gmGUITAR_HERO)
			{
				SetOutput(0, m_ChannelOutputFlags & ofRED_PAD);
				SetOutput(1, m_ChannelOutputFlags & ofYELLOW_PAD);
				SetOutput(2, m_ChannelOutputFlags & ofBLUE_PAD);
				SetOutput(3, m_ChannelOutputFlags & ofGREEN_PAD);
				
				// kick and orange cymbal use same LED in GH mode
				SetOutput(4, m_ChannelOutputFlags & (ofPEDAL1 | ofORANGE_CYMBAL));
			}
			else // Rock Band
			{
				SetOutput(0, m_ChannelOutputFlags & ofRED_PAD);
				SetOutput(1, m_ChannelOutputFlags & (ofYELLOW_PAD | ofYELLOW_CYMBAL));
				SetOutput(2, m_ChannelOutputFlags & (ofBLUE_PAD | ofBLUE_CYMBAL));
				SetOutput(3, m_ChannelOutputFlags & (ofGREEN_PAD | ofGREEN_CYMBAL));
				
				// kick and hi hat pedals share same LED
				SetOutput(4, (m_ChannelOutputFlags & (ofPEDAL1 | ofPEDAL2)) != 0);
			}
		}
	} // if g_SystemMode == MODE_PLAY
	else 
	{
		/*
		Regular MIDI Rocker program mode -- PROG1 is for programming the drums, PROG2 is for the cymbals
		*/

		/*
		Check input keys -- if a key is pressed then toggle program mode on the corresponding output.
		SelectProgramMode() sets m_MidiChannelToProgram to the corresponding value
		*/
		if (m_bSystemMode == MODE_PROG_VELOCITY)
		{
			if (swDRUM1 == SW_PRESSED)
				SelectProgramMode(0);
			else if (swDRUM2 == SW_PRESSED)	
				SelectProgramMode(1);
			else if (swDRUM3 == SW_PRESSED)	
				SelectProgramMode(2);
			else if (swDRUM4 == SW_PRESSED)	
				SelectProgramMode(3);
			else if (swDRUM5 == SW_PRESSED)	
				SelectProgramMode(4);
		}
		else // MODE_PROG_MAP - program cymbals
		{
			if (g_GameMode == gmROCK_BAND)
			{
				if (swDRUM2 == SW_PRESSED)	// yellow cymbal
					SelectProgramMode(5);
				else if (swDRUM3 == SW_PRESSED)	// blue cymbal
					SelectProgramMode(6);
				else if (swDRUM4 == SW_PRESSED)	// green cymbal
					SelectProgramMode(7);
			}	
			else 
			{
			 	if (swDRUM5 == SW_PRESSED) // orange cymbal
					SelectProgramMode(5);
			}
			
		}
		
		// if BACK button is held down, clear out the map for this output
		if ((m_MidiChannelToProgram >= 0) 
		&& m_ButtonStatus[BACK_BTN_INDEX].StateChanged && (m_ButtonStatus[BACK_BTN_INDEX].State & bsHELD_DOWN))
		{
			ClearMidiMapChannel(m_MidiChannelToProgram);

			// toggle the LED off/on/off
			SetOutput(m_MidiChannelToProgram, FALSE);
			DelayMillisecs(300);
			SetOutput(m_MidiChannelToProgram, TRUE);
		}

		// check for MIDI data, assign to the channel
		DoMidiMapProgramming(); // uses m_MidiChannelToProgram, updates g_MidiOnNote and velocity data		
	}	


	/*
	Set button report bit flags according to which outputs are activated.
	*/
	if (m_ChannelOutputFlags & ofRED_PAD)
		m_wButtonFlags |= RED_DRUM;
	
	if (m_ChannelOutputFlags & ofYELLOW_PAD)
		m_wButtonFlags |= YELLOW_DRUM;

	if (m_ChannelOutputFlags & ofBLUE_PAD)
		m_wButtonFlags |= BLUE_DRUM;

	if (m_ChannelOutputFlags & ofGREEN_PAD)
		m_wButtonFlags |= GREEN_DRUM;

	if (m_ChannelOutputFlags & ofPEDAL1)
		m_wButtonFlags |= KICK_PEDAL;

	if (g_GameMode == gmGUITAR_HERO)
	{
		if (m_ChannelOutputFlags & ofORANGE_CYMBAL)
			m_wButtonFlags |= ORANGE_CYMBAL;
	}
	else // ROCK BAND
	{
		// check hi hat pedal channel
		if (m_ChannelOutputFlags & ofPEDAL2)
			m_wButtonFlags |= HIHAT_PEDAL;
		
		// set the drum flag if a drum channel is active (RB mode only)
		if (m_ChannelOutputFlags & (ofRED_PAD | ofYELLOW_PAD | ofBLUE_PAD | ofGREEN_PAD))
			m_wButtonFlags |= DRUM_FLAG; 
		
		// check cymbals...
		
		if (m_ChannelOutputFlags & ofYELLOW_CYMBAL)
		{
			m_wButtonFlags |= YELLOW_DRUM | CYMBAL_FLAG;
			m_bNavButtonFlags = joyHAT_UP;
		}
	
		if (m_ChannelOutputFlags & ofBLUE_CYMBAL)
		{
			m_wButtonFlags |= BLUE_DRUM | CYMBAL_FLAG;
			m_bNavButtonFlags = joyHAT_DOWN;
		}
	
		if (m_ChannelOutputFlags & ofGREEN_CYMBAL)
			m_wButtonFlags |= GREEN_DRUM | CYMBAL_FLAG;
	}

	// copy bit flags to report buffer
	hid_report_in[0] = (BYTE)m_wButtonFlags; // reports drum buttons
	hid_report_in[1] = (BYTE)(m_wButtonFlags >> 8); // buttons 9 thru 13
	hid_report_in[2] = m_bNavButtonFlags; // reports strummer or nav switch position

	#if defined(PROCESS_HOST_CMD)
	/*
	In host command mode we return special values in the XYZ axes and other values for diagnositics.
	*/
	if (g_HostCmdMode)
	{
		hid_report_in[3] = g_HostCmdResponseX; // send command response in X axis data
		hid_report_in[4] = g_HostCmdResponseY; // send command response in Y axis data
		hid_report_in[5] = g_HostCmdResponseZ; // send command response in Z axis data

		hid_report_in[15] = g_AdjustKnobPos;
		hid_report_in[16] = m_bModePos;
		hid_report_in[17] = swFUNCTION;
	}
	#endif
} // UpdateInputReportData_MR


/*
Read the A/D to get the position of the mode switch and adjust knob.
*/
static void UpdateKnobPositions(void)
{
	static BYTE s_nStateCounter = 0;
	static BYTE s_bLastMode = -1;
  	BYTE bData, bMode;

	// get analog inputs
	if (ADCON0bits.GO_DONE == 0) // if A/D is done
	{
		bData = ADRESH; // get A/D reading
		
		switch (s_nStateCounter)
		{
			case 0:
				// set A/D to sample the mode position
				ADCON0 = AD_CH1 | AD_ENABLE;
				break;

			case 2:
				// determine what position the mode switch is in
				if (bData > 192)
					bMode = MODE_PLAY;
				else if (bData > 64)
					bMode = MODE_PROG_VELOCITY;
				else
					bMode = MODE_PROG_MAP;
				
				// don't update m_bModePos until current value matches the one from last time around
				if (bMode == s_bLastMode)
					m_bModePos = bMode;
				s_bLastMode = bMode;
				break;

			case 3:
				// set A/D to sample the velocity position
				ADCON0 = AD_CH0 | AD_ENABLE;
				break;

			case 5:
				// update velocity position
				g_AdjustKnobPos = bData; // max velocity value is 128
				g_MinVelocity = (bData >> 1);
				break;
		}

		// go to next state
		if (++s_nStateCounter > 5)
			s_nStateCounter = 0; 

		ADCON0 |= AD_GO; // start A/D conversion
	} // if A/D is done
} // UpdateKnobPositions()

#endif


