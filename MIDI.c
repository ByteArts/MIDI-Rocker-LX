/*------------------------------------------------------------------------------

	Filename:	MIDI.c

	Purpose:	Code for decoding a note that comes in to the MIDI port
				(the UART).

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
	Includes
------------------------------------------------------------------------------*/
#include <p18cxxx.h>
#include "EEData.h"
#include "GenericTypeDefs.h"
#include "Pinout.h"
#include "MIDI.h"
#include "App.h"

/*------------------------------------------------------------------------------
	Defines
------------------------------------------------------------------------------*/
		
#define BAUD	31250  // use 19200 for help when debugging


// TYPES ---------------------------------------------------------------

/*
These are the states in which the UART ISR can be.
*/
typedef enum
{
	WAITING_FOR_STATUS,			// 0
	WAITING_FOR_NOTE_ON,		// 1
	WAITING_FOR_ON_VELOCITY,	// 2
	WAITING_FOR_NOTE_OFF,		// 3
	WAITING_FOR_OFF_VELOCITY,	// 4
	WAITING_FOR_DATA1_ONLY,		// 5
	WAITING_FOR_DATA1_OF_2,		// 6
	WAITING_FOR_DATA2,			// 7
	COLLECT_SYS_EX_DATA,		// 8
	WAITING_FOR_CC_DATA1,		// 9
	WAITING_FOR_PEDAL_DATA,		// 10
	WAITING_FOR_CC_DATA2		// 11
} TMidiState;



/*------------------------------------------------------------------------------
	Data
------------------------------------------------------------------------------*/

BYTE g_RxData;
BYTE g_TxData;

BYTE g_MidiMapNumber = 0;
BYTE g_MidiMapEEPROMAddress = EEADDR_MIDI_MAP1;

UINT8 g_MessageID = 0;
UINT8 g_MidiOnNote = INVALID_NOTE_NUMBER;
UINT8 g_NoteVelocity = 0;
UINT8 g_MidiOffNote = INVALID_NOTE_NUMBER;
UINT8 g_MinVelocity = DEFAULT_VELOCITY_THRESHOLD;

UINT8 g_HiHatPedalPosition = 0;
UINT8 g_HiHatThreshold = 64; // default to half-way pressed

TMidiState	g_MessageState = WAITING_FOR_STATUS;

/*
Table which contains state of all the MIDI note flags. A flag is set when one of the notes 
to which it's mapped is recieved.
*/
BOOL g_MidiChannelOutputs[MIDI_CHANNEL_COUNT] =
{
	0, 0, 0, 0, 
	0, 0, 0, 0
};

UINT8 g_MidiChannelVelocity[MIDI_CHANNEL_COUNT];

static BYTE m_SysExtDataBuf[16];
static BYTE m_SysExtDataIndex = 0;

/*
Table that converts between the incoming MIDI note and a user-defined drum pad bit.
The index into the table is the drum pad bit number, and the 
contents of the table are the MIDI note corresponding to that pad.
*/
static UINT8 m_MidiMapTable[MIDI_CHANNEL_COUNT][NOTES_PER_CHANNEL];

const static UINT8 DEFAULT_MAP0[MIDI_CHANNEL_COUNT][NOTES_PER_CHANNEL] =
{
	{  31,  48,  45,  39,  33,  22,  25,  49 },
	{  34,  50,  47,  41,  35,  26,  51,  52 },
	{  37, 255, 255,  43,  36,  42,  53,  55 },
	{  38, 255, 255, 255, 255,  44,  56,  57 },
	{  40, 255, 255, 255, 255,  46,  59, 255 },
	{ 255, 255, 255, 255, 255,  54, 255, 255 },
	{ 255, 255, 255, 255, 255, 255, 255, 255 },
	{ 255, 255, 255, 255, 255, 255, 255, 255 }
};

const static UINT8 DEFAULT_MAP1[MIDI_CHANNEL_COUNT][NOTES_PER_CHANNEL] =
{
	{  31,  22,  48,  39,  33,  49, 255, 255 },
	{  34,  26,  50,  41,  35,  52, 255, 255 },
	{  37,  42, 255,  43,  36,  55, 255, 255 },
	{  38,  44, 255,  45, 255,  57, 255, 255 },
	{  40,  46, 255,  47, 255, 255, 255, 255 },
	{ 255,  85, 255, 255, 255, 255, 255, 255 },
	{ 255, 255, 255, 255, 255, 255, 255, 255 },
	{ 255, 255, 255, 255, 255, 255, 255, 255 }
};

/*------------------------------------------------------------------------------
	Prototypes
------------------------------------------------------------------------------*/

#if defined(NOTE_OFF_CLEARS_FLAG)
	static void ClearMidiOutput(UINT8 MidiNote);
#endif
static UINT8 DoMidiTableLookup(UINT8 MidiNote, UINT StartingOutputIndex);
static void	HandleSystemMessage(void);
static void	SetMidiOutputFlag(UINT8 MidiNote, UINT8 Velocity);

#if defined(MIDI_OUT_ADAPTER)
	static UINT8 TranslateNoteForGHWT(UINT8 Note);
	static UINT8 TranslateNoteForMidiPro(UINT8 Note);
#endif

/*------------------------------------------------------------------------------
	Functions
------------------------------------------------------------------------------*/

void MIDI_Initialize(void)
{
/*
Sets up the UART for the MIDI input.
*/
	SSPCON1				= 0;		// Make sure SPI is disabled

	// Note: InitIO initializes the TRISC register

#if BAUD == 19200
	SPBRG				= 0x70;
	SPBRGH				= 0x02;	// 0x0270 for 48MHz -> 19200 baud
#else // use 31250
	SPBRG				= 0x7F;
	SPBRGH				= 0x01;	// 0x017F for 48MHz -> 31250 baud
#endif

#if defined(MIDI_OUT_ADAPTER)
	// setup UART output
	//                      76543210
	TRISC               &=0b10111111;   // configure TX pin (RC6) as an output
	TXSTA				= 0b00100100;	// TXEN (xmit enabled), BRGH (select high baud rate), asynchronous mode 
#else	
	TXSTA				= 0b00000100;	// BRGH (select high baud rate), asynchronous mode 	
#endif	

	// setup UART input
	RCSTA				= 0x90;	// SPEN (serial port enabled), CREN (continuous RX)
	BAUDCON				= 0x08;	// BRG16 (16bit baud generator)

#ifdef MIDI_INTERRUPT
	RCON				|= 0x80;	// Enable priority interrupt selection

	IPR1				|= 0x20;	// RCIP = 1; // Receive ISR High Priority
//	IPR1				&= ~0x20;	// RCIP = 0; // Receive ISR Low Priority

	PIE1				|= 0x20;	// RCIE = 1; // Enable Receive ISR
	/*
	Enable interrupts: global enable, enable peripheral interrupts (UART),
	enable timer0 interrupt.
	*/
	INTCON	|= 0b11000000; // enable interrupts : global, priority
#endif
}


void ClearMidiOutputs(void)
{
/*
Clear all the midi "channel" output flags. 
*/
	UINT8 nFlag;

	for (nFlag = 0; nFlag < MIDI_CHANNEL_COUNT; ++nFlag)
		g_MidiChannelOutputs[nFlag] = FALSE;
}


void RecallMidiMap(void)
{
/*
Read the pre-programmed notes (if any) from EEPROM into RAM (m_MidiMapTable).
g_MidiMapEEPROMAddress must be set to the desired map address prior to this
(see SetMidiMapNumber())
*/
	UINT8	nIndex, nNote; 
	UINT8 * pTable;

	pTable = &m_MidiMapTable[0][0]; // pointer to start of table array
	for (nIndex = 0; nIndex < MIDI_TABLE_SIZE; ++nIndex)
	{
		// Note: g_MidiMapEEPROMAddress changes depending on the current map number
		*pTable = ReadEEData(g_MidiMapEEPROMAddress + nIndex);
		
		// a little delay here seems to fix read errors
		if (*pTable == 0)
			;
			
		++pTable; // index next array entry
	}
}

void RestoreDefaultMap(UINT8 MapNumber)
{
	UINT8 nNoteIndex, nChannel, nNoteValue;
	const UINT8 * pMap; // (* pMap)[MIDI_CHANNEL_COUNT][NOTES_PER_CHANNEL];
	
	if (MapNumber == 0)
		pMap = &DEFAULT_MAP0[0][0];
	else
		pMap = &DEFAULT_MAP1[0][0];
	
	for (nChannel = 0; nChannel < MIDI_CHANNEL_COUNT; ++nChannel)
	{
		for (nNoteIndex = 0; nNoteIndex < NOTES_PER_CHANNEL; ++nNoteIndex)
		{
			nNoteValue = *(pMap + (nNoteIndex * NOTES_PER_CHANNEL) + nChannel);
			SetMidiMapEntry(nChannel, nNoteIndex, nNoteValue);
		}
	}
}


UINT8 GetMidiMapEntry(INT8 ChannelNumber, UINT8 NoteIndex)
{
/*
Get the specified note map entry.
*/
	return m_MidiMapTable[ChannelNumber][NoteIndex];
}



void MIDI_ServiceUARTRx(void)
{
/*
This routine is called when data is available from the UART
*/
	static UINT8 m_PendingNote; // Note ON data goes here
	UINT8 nTranslatedNote; // value for sending out to GH or MIDI Pro adapter
	BOOL boolSendDataOut;  // if true, then data isn't sent out MIDI OUT
	
	// read MIDI data from UART
	g_RxData = RCREG;
	
	#if defined(MIDI_OUT_ADAPTER)	
		boolSendDataOut = FALSE; // don't send data by default
		g_TxData = g_RxData; // data passthru 
	#endif
		
	/*
	The first byte of any MIDI message is the "status" byte. It is different
	from a "data" byte in that it has bit 7 set.
	No matter what state we were in, if this is a status byte, reset
	the state machine, unless we're collecting a sys-ex message.
	*/
	if (g_RxData & 0x80)	// Bit 7 is set in the first byte of every message.
	{
		/*
		Find out what type of message this is.
		*/
		g_MessageID	= g_RxData & 0xF0;

		switch (g_MessageID)
		{
			case NOTE_ON:
				boolSendDataOut = TRUE;
				g_MessageState = WAITING_FOR_NOTE_ON;
				break;

			case NOTE_OFF:
				g_MessageState = WAITING_FOR_NOTE_OFF;
				break;

			case CONTROL_CHANGE:
				g_MessageState = WAITING_FOR_CC_DATA1;
				break;
			
			case POLY_AFTERTOUCH:
			case PITCH_BEND:
				g_MessageState = WAITING_FOR_DATA1_OF_2;
				break;

			case PROGRAM_CHANGE:
			case CHANNEL_AFTERTOUCH:			
				g_MessageState = WAITING_FOR_DATA1_ONLY;
				break;

			case SYS_EX_START: 
				HandleSystemMessage();
				break;
				
			case ACTIVE_SENSING:
			case TIMING_CLOCK:
				break;
				
			default:
				break;
		}
	}
	else
	{
		/*
		The incoming byte is data for a message. Figure out what 
		to do with it based on the state (set by the first byte).
		*/
		#if defined(LOG_MIDI_DATA)
			AddDataToLog(g_MessageState, g_RxData); 
		#endif
		
		switch (g_MessageState)
		{
			case WAITING_FOR_NOTE_ON:
				ledMIDI = LED_OUTPUT_ON; 
				m_PendingNote = g_RxData; 
				
				#if defined(MIDI_OUT_ADAPTER)
					if (g_GameMode == gmGUITAR_HERO)
					{
						/*
						Translate note into value to be sent out to GHWT controller,
						returns INVALID_NOTE_NUMBER if note is not mapped. Variable m_PendingNote is 
						NOT set to the translated value because that would affect the value used to
						map the output channel later in the WAITING_FOR_ON_VELOCITY event, which
						calls SetMidiOutputFlag() and which in turns calls DoMidiTableLookup(). 
						*/
						nTranslatedNote = TranslateNoteForGHWT(g_RxData); // uses DoMidiTableLookup()
						
						// if note is mapped, then go ahead and send it
						if (nTranslatedNote != INVALID_NOTE_NUMBER)
						{
							g_TxData = nTranslatedNote; // send translated note
							boolSendDataOut = TRUE;
						}
					}
					else if (g_GameMode == gmROCK_BAND)
					{
						/*
						Translate note into value to be sent out to MIDI PRO controller, 
						returns INVALID_NOTE_NUMBER if note is not mapped. See note above about
						the translated note value.
						*/
						nTranslatedNote = TranslateNoteForMidiPro(g_RxData); // uses DoMidiTableLookup()
						
						// if note is mapped, then go ahead and send it
						if (nTranslatedNote != INVALID_NOTE_NUMBER)
						{
							g_TxData = nTranslatedNote; // send translated note
							boolSendDataOut = TRUE;
						}
					}
				#endif
								
				g_MidiOffNote = INVALID_NOTE_NUMBER;
				g_MessageState = WAITING_FOR_ON_VELOCITY;
				break;
		
			case WAITING_FOR_ON_VELOCITY:
				g_NoteVelocity = g_RxData;
				g_MidiOnNote = m_PendingNote; // note is "official" now that we got the velocity data

				// NOTE ON with velocity 0 is really a NOTE OFF
				if (g_NoteVelocity == 0) 
				{
					g_MidiOffNote = g_MidiOnNote;
					g_MidiOnNote = INVALID_NOTE_NUMBER; // invalidate the ON note
					
					#if defined(NOTE_OFF_CLEARS_FLAG)
						ClearMidiOutput(g_MidiOnNote);
					#endif
					
					ledMIDI = LED_OUTPUT_OFF; 
				}
				else if (g_NoteVelocity >= g_MinVelocity) 
				{
					// Lookup the note and map it to an output
					SetMidiOutputFlag(g_MidiOnNote, g_NoteVelocity); // uses DoMidiTableLookup()
				}
			#if defined(MIDI_OUT_ADAPTER)
				else 
					g_TxData = 0; // velocity < theshold, so set it to 0 so GHWT controller ignores the note

				// if note wasn't supressed, then go ahead and send velocity
				if (m_PendingNote != INVALID_NOTE_NUMBER)
					boolSendDataOut = TRUE;
			#endif					
				
				/*
				There is a note in one of the MIDI technical documents that
				says a note-on message can be followed by another one, 
				without having a "status" byte.
				*/
				g_MessageState = WAITING_FOR_NOTE_ON;
				break;
			
			case WAITING_FOR_NOTE_OFF:
				g_MidiOffNote = g_RxData;
				g_MidiOnNote = INVALID_NOTE_NUMBER; // note on is now invalidated
				
				#if defined(NOTE_OFF_CLEARS_FLAG)
					ClearMidiOutput(g_MidiOffNote);
				#endif
				
				ledMIDI = LED_OUTPUT_OFF; 
				g_MessageState = WAITING_FOR_OFF_VELOCITY; // next data is OFF velocity
				break;

			case WAITING_FOR_OFF_VELOCITY:
				g_MessageState = WAITING_FOR_STATUS;
				break;
			
			case WAITING_FOR_DATA1_OF_2:			
				g_MessageState = WAITING_FOR_DATA2;
			 	break;
			
			case COLLECT_SYS_EX_DATA:
				// Store this byte in the sys-ex array. First make sure there's room.
				if (m_SysExtDataIndex < (sizeof(m_SysExtDataBuf) - 1))
				{
					m_SysExtDataBuf[m_SysExtDataIndex++] = g_RxData;
					m_SysExtDataBuf[m_SysExtDataIndex] = 0;
				}
				break;
				
			case WAITING_FOR_CC_DATA1: // controller change data -- this is the controller number
				if (g_RxData == 4) // hi hat pedal is controller 4
				{
					g_MessageState = WAITING_FOR_PEDAL_DATA; // get pedal position in next step
				#if defined(MIDI_OUT_ADAPTER)
					if (g_GameMode == gmROCK_BAND)
						boolSendDataOut = TRUE;
				#endif					
				}
				else
					g_MessageState = WAITING_FOR_CC_DATA2; // data for different controller

				break;
			
			case WAITING_FOR_PEDAL_DATA: // hi hat pedal (controller 4) position
				g_HiHatPedalPosition = g_RxData;
				g_MessageState = WAITING_FOR_STATUS;
				
			#if defined(MIDI_OUT_ADAPTER)
				if (g_GameMode == gmROCK_BAND)
					boolSendDataOut = TRUE;
			#endif					
				break;

			case WAITING_FOR_DATA1_ONLY:
			case WAITING_FOR_DATA2:
			case WAITING_FOR_CC_DATA2:
			default:
				g_MessageState = WAITING_FOR_STATUS;
				break;
		}
	}

	#if defined(MIDI_OUT_ADAPTER)
		if (boolSendDataOut)	
		{
			// echo the recieved data to the MIDI OUT
			if (PIR1bits.TXIF) // ready to send? (TXIF is set when TXREG is empty)
				TXREG = g_TxData; // send data to MIDI OUT
		}
	#endif
} // MIDI_ServiceUARTRx


#if defined(NOTE_OFF_CLEARS_FLAG)
static void ClearMidiOutput(UINT8 MidiNote)
/*
Clear the output which is mapped to the specified note.
*/
{
	UINT8 nChannel = 0;
#if defined(MULTIPLE_CHANNELS_PER_NOTE)
	while (TRUE)
	{
#endif
		nChannel = DoMidiTableLookup(MidiNote, nChannel);
	
		if (nChannel == INVALID_TABLE_INDEX)
			return;	
	
		g_MidiChannelOutputs[nChannel] = FALSE; // deactivate this channel

#if defined(MULTIPLE_CHANNELS_PER_NOTE)
		++nChannel;	// check the next channel to see if note is also assigned to it
	} // while
#endif

}
#endif

/*
Updates the map in memory, and also writes the new value to EEPROM. Be sure that
g_MidiMapEEPROMAddress is set correctly first!
*/
void SetMidiMapEntry(INT8 ChannelNumber, UINT8 NoteIndex, UINT8 MidiNote)
{
	UINT8 nAddress;

	if ((ChannelNumber >= 0) && (ChannelNumber < MIDI_CHANNEL_COUNT) && (NoteIndex < NOTES_PER_CHANNEL))
	{
		m_MidiMapTable[ChannelNumber][NoteIndex] = MidiNote;

		// save the new map value in EEPROM
		nAddress = g_MidiMapEEPROMAddress + (ChannelNumber * NOTES_PER_CHANNEL) + NoteIndex;
		WriteEEData(nAddress, MidiNote);
	}
}


/*
Add a note to the note map for the specified output.
Returns the number of notes that are mapped to that output, 0 if note is already
programmed, or -1 on error.
*/
INT8 AddMidiNoteMapping(INT8 ChannelNumber, UINT8 MidiNote, UINT8 Velocity, BOOL Append)
{
	UINT8 nNoteIndex, nAddress;

	if ((ChannelNumber >= 0) && (ChannelNumber < MIDI_CHANNEL_COUNT))
	{
		/*
		If appending a note to the map, then put the new note in the next available note slot for
		this flag, otherwise just put the note in the first slot.
		*/
		if (Append)
		{
			// find the first duplicate or unassigned note slot in the map table
			for (nNoteIndex = 0; nNoteIndex < NOTES_PER_CHANNEL; ++nNoteIndex)
			{
				if (m_MidiMapTable[ChannelNumber][nNoteIndex] == MidiNote)
					return 0; // note already programmed -- we are done

				if (m_MidiMapTable[ChannelNumber][nNoteIndex] == INVALID_NOTE_NUMBER)
					break; // found unused slot to store the note in
			}

			// if all the slots are used up, then re-use the last one
			if (nNoteIndex >= NOTES_PER_CHANNEL)
				nNoteIndex = NOTES_PER_CHANNEL - 1;
		}
		else
			nNoteIndex = 0; // not appending, so put in first slot
		
		SetMidiMapEntry(ChannelNumber, nNoteIndex, MidiNote);
		
		// if not last slot, then write an "invalid" note marker into the next slot
		if (nNoteIndex < NOTES_PER_CHANNEL - 1)
			SetMidiMapEntry(ChannelNumber, nNoteIndex + 1, INVALID_NOTE_NUMBER);

		return nNoteIndex + 1;
	}

	return -1;
}

/*
Sets the output to which the specified note is assigned and records the velocity also. If the
note is not assigned to any outputs, then it doesn't do anything.
*/
static void SetMidiOutputFlag(UINT8 MidiNote, UINT8 Velocity)
{
	UINT8 nChannel = 0;
	
#if defined(USE_HIHAT_THRESHOLD) 
	/*
	Change the hi hat open note to hi hat close note depending on the hi hat pedal position. This is
	for some drum controllers which don't handle this very well.
	*/
	if (g_HiHatThreshold != INVALID_NOTE_NUMBER)	
	{
		// change the hi hat note from an "open" to a "closed" depending on the current pedal position	
		if ((MidiNote == HIHAT_OPEN_NOTE) || (MidiNote == HIHAT_RIM_OPEN_NOTE) 
		|| (MidiNote == HIHAT_EDGE_OPEN_NOTE))
		{
			// Note: the position value increases as the pedal is pressed, 0 means not pressed at all
			// while 127 means fully pressed.
			
			// if pedal is past threshold, then it's a "closed" note
			if (g_HiHatPedalPosition > g_HiHatThreshold)
				MidiNote = HIHAT_CLOSED_NOTE;
			else
				MidiNote = HIHAT_OPEN_NOTE;
		}
	}
#endif	
	
#if defined(MULTIPLE_CHANNELS_PER_NOTE)
	// check the entire table for all matches, not just the first
	while (TRUE)
	{
#endif
		nChannel = DoMidiTableLookup(MidiNote, nChannel);
	
		if (nChannel == INVALID_TABLE_INDEX)
			return;	
	
		g_MidiChannelOutputs[nChannel] = TRUE; // activate this channel
		g_MidiChannelVelocity[nChannel] = Velocity; // record the note velocity

#if defined(MULTIPLE_CHANNELS_PER_NOTE)
		++nChannel;	// check the next channel to see if note is also assigned to it
	} // while
#endif
}

/*------------------------------------------------------------------------------

	Function:	HandleSystemMessage

	Synopsis:	Ignore most system messages, but deal with them correctly.
				Some of them expect data.

	Parameters:	none

	Returns:	none

	Warnings:	

------------------------------------------------------------------------------*/
static void HandleSystemMessage(void)
{
	switch (g_RxData)
	{
		case SYS_EX_START:
		{
			g_MessageState = COLLECT_SYS_EX_DATA;
			m_SysExtDataIndex = 0;
		} break;
		
		case QUARTER_FRAME:
		case SONG_SELECT:
		{
			g_MessageState = WAITING_FOR_DATA1_ONLY;
		} 
		break;
		
		case SONG_POSITION_PTR:
		{
			g_MessageState = WAITING_FOR_DATA1_OF_2;
		} break;
		
		case SYS_EX_END:
		{
			g_MessageState = WAITING_FOR_STATUS;
		} break;

		case ACTIVE_SENSING:
		{
		} break;
		
		default: 
			break;
	}
}


void ClearMidiMapChannel(INT8 ChannelNumber)
{
	UINT8 nNoteIndex, nAddress;
	
	for (nNoteIndex = 0; nNoteIndex < NOTES_PER_CHANNEL; ++nNoteIndex)
	{
		SetMidiMapEntry(ChannelNumber, nNoteIndex, INVALID_NOTE_NUMBER);	
	}
}



static UINT8 DoMidiTableLookup(UINT8 MidiNote, UINT StartingOutputIndex)
{
/*
Return the output number to which the specified note is mapped. The StartingOutputIndex is allow
for the case where the same note may be assigned to multiple outputs -- we can call this function
multiple times and start the search at a different offset.
*/
	UINT8 nOutputIndex, nNoteIndex, nTableEntry;
	
	for (nOutputIndex = StartingOutputIndex; nOutputIndex < MIDI_CHANNEL_COUNT; ++nOutputIndex)
	{
		for (nNoteIndex = 0; nNoteIndex < NOTES_PER_CHANNEL; ++nNoteIndex)
		{
			nTableEntry = m_MidiMapTable[nOutputIndex][nNoteIndex];			
			
			// invalid note marks the end of the note maps for this output
			if (nTableEntry == INVALID_NOTE_NUMBER)
				break;

			if (nTableEntry == MidiNote)
			{
				/*
				Each output can have multiple MIDI note mappings, so we return the number
				corresponding to the output, not the table index.
				*/
				return (nOutputIndex);			
			}
		}
	}

	return (INVALID_TABLE_INDEX);
}


#if defined(MIDI_GUITAR)

static void ProcessExtendedSystemData(void)
{
/*
Handle special system data in m_SysExtDataBuf[]. Count is in m_SysExtDataIndex.
*/
	if (m_SysExtDataIndex < 7)
		return;

	/*
	On the Yamaha Ez_EG guitar, we are looking for special data which tells us about strings 
	and frets. The data format is:
	bytes 0-3 = special message preamble
	byte 4 = function ID
	byte 5,6 = function data
	*/	

	// check for proper message preamble (43 7F 00 00)
	if ((m_SysExtDataBuf[0] != 0x43) || (m_SysExtDataBuf[1] != 0x7F) 
	||  (m_SysExtDataBuf[2] != 0x00) || (m_SysExtDataBuf[3] != 0x00))
		return;
	
	// next byte to check is the function ID
	switch (m_SysExtDataBuf[4])
	{
/*
		case 0x01: // fret switch on
		case 0x02: // fret switch off
		{
			UINT8 nFret, nNote, nString;

			// get string, fret number from the note data
			nString = m_SysExtDataBuf[5];
			nNote = m_SysExtDataBuf[6];

			if (nString > 2)
				nFret = nNote - (((6 - nString) * 5) + 0x28);
			else
				nFret = nNote - (((6 - nString) * 5) + 0x27);
			
			nFret = nFret % 5;

			if (m_SysExtDataBuf[4] == 0x01)
				g_MidiOutputFlags[nFret] = TRUE;
			else
				g_MidiOutputFlags[nFret] = FALSE;
		}
			break;
*/
		case 0x05: // string data
			// don't care which string is pressed, just activate the strum channel
			// check velocity
			if (m_SysExtDataBuf[6] > g_MinVelocity)
			{
				g_MidiChannelOutputs[5] = TRUE; // activate the strum 
#if defined(LOG_MIDI_DATA)
				AddDataToLog(COLLECT_SYS_EX_DATA, m_SysExtDataBuf[5]); // log the string number
#endif
			}
			break;
	}

}
#endif


void SetMidiMapNumber(BYTE Value, BOOL ReadTableFromEEPROM)
{
	g_MidiMapNumber = Value;
	g_MidiMapEEPROMAddress = EEADDR_MIDI_MAP1 + (Value * MIDI_TABLE_SIZE);

	if (ReadTableFromEEPROM)
		RecallMidiMap();

#if defined(MR_LX)
	// update LED indicators
	ledM1 = LED_OUTPUT_OFF;
	ledM2 = LED_OUTPUT_OFF;

	if (g_MidiMapNumber == 0)
		ledM1 = LED_OUTPUT_ON;
	else if (g_MidiMapNumber == 1)
		ledM2 = LED_OUTPUT_ON;
#endif
}


#if defined(MIDI_OUT_ADAPTER)
static UINT8 TranslateNoteForGHWT(UINT8 Note)
{
/*
Translates the input Note to a value expected by the Guitar Hero World Tour drums MIDI input.
It uses the note map to find out which channel the input note is assigned to.
*/
	UINT8 nChannel, Result;
	
	Result = INVALID_NOTE_NUMBER;
	
	nChannel = DoMidiTableLookup(Note, 0);
	
	switch (nChannel)
	{
		case 0: 
			Result = 38; // GH red note
			break;
			
		case 1: 
			Result = 46; // GH yellow note
			break;
			
		case 2:
			Result = 48; // GH blue note
			break;
			
		case 3:
			Result = 45; // GH green note
			break;
			
		case 4:
			Result = 36; // GH kick note
			break;
			
		case 5:
			Result = 49; // GH orange note
			break;
	}
	
	return Result;
}


static UINT8 TranslateNoteForMidiPro(UINT8 Note)
{
/*
Translates the input Note to a value expected by the MIDI Pro adapter.
It uses the note map to find out which channel the input note is assigned to.
*/
	UINT8 nChannel, Result;
	
	Result = INVALID_NOTE_NUMBER;
	
	nChannel = DoMidiTableLookup(Note, 0);
	
	switch (nChannel)
	{
		case 0: // red pad
			Result = 38; 
			break;
			
		case 1: // yellow pad
			Result = 48; 
			break;
			
		case 2: // blue pad
			Result = 45; 
			break;
			
		case 3: // green pad
			Result = 41; 
			break;
			
		case 4: // kick pedal
			Result = 33;
			break;
			
		case 5: // yellow cymbal
			Result = 22; 
			break;
			
		case 6: // blue cymbal
			Result = 51;
			break;
			
		case 7: // green cymbal
			Result = 49;
			break;
	}
	
	return Result;
}


#endif

/*------------------------------------------------------------------------------

	Function:	_high_ISR

	Synopsis:	This is the Interrupt Service Routine for the high-priority
				interrupts. The only interrupt set up to be high-priority
				is the UART.

	Warnings:	This function jumped to by the bootloader, which expects the
				high-priority ISR to be at address 808: if you change the
				bootloader, you must make sure it jumps to the correct
				address for a high-priority interrupt.
				Also, make sure this function is not so big that it encroaches
				into the code space for the low-priority interrupt.

------------------------------------------------------------------------------*/

#ifdef MIDI_INTERRUPT
#pragma code _HIGH_INTERRUPT_VECTOR = 0x000808
void _high_ISR(void)
{
	if (PIR1bits.RCIF)
	{
		MIDI_ServiceUARTRx();
	}

    _asm 
		retfie 0 
	_endasm
}
#endif
