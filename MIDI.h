#ifndef _INC_MIDI
#define _INC_MIDI


#include "GenericTypeDefs.h"

// CONST

/*
MIDI message status ID's.
*/
#define NOTE_OFF			0x80
#define NOTE_ON				0x90
#define POLY_AFTERTOUCH		0xA0
#define CONTROL_CHANGE		0xB0
#define MODE_CHANGE			0xB0
#define PROGRAM_CHANGE		0xC0
#define CHANNEL_AFTERTOUCH	0xD0
#define PITCH_BEND			0xE0

#define SYS_EX_START		0xF0
#define QUARTER_FRAME		0xF1
#define SONG_POSITION_PTR	0xF2
#define SONG_SELECT			0xF3
#define UNDEFINED_1			0xF4
#define UNDEFINED_2			0xF5
#define TUNE_REQUEST		0xF6
#define SYS_EX_END			0xF7

#define TIMING_CLOCK		0xF8
#define UNDEFINED_3			0xF9
#define MIDI_START			0xFA
#define MIDI_CONTINUE		0xFB
#define MIDI_STOP			0xFC
#define UNDEFINED_4			0xFD
#define ACTIVE_SENSING		0xFE
#define MIDI_RESET			0xFF

#define DEFAULT_VELOCITY_THRESHOLD	10 // Minimum velocity allowable for a note to be recognized.


#define INVALID_NOTE_NUMBER 0xFF
#define INVALID_TABLE_INDEX -1

#define MIDI_CHANNEL_COUNT 8  // how many channels can be programed with a MIDI note
#define NOTES_PER_CHANNEL  8  // how many notes can be programmed to single channel
#define MIDI_TABLE_SIZE (MIDI_CHANNEL_COUNT * NOTES_PER_CHANNEL)
#define MIDI_MAP_COUNT 2

// special hi hat notes used by Roland (and others)
#define HIHAT_OPEN_NOTE			46
#define HIHAT_RIM_OPEN_NOTE		26
#define HIHAT_EDGE_OPEN_NOTE	28
#define HIHAT_CLOSED_NOTE		42

// GLOBAL DATA ===========================================================

extern BYTE g_MidiMapNumber;
extern BYTE g_MidiMapEEPROMAddress;

extern BYTE g_MessageID;
extern BYTE g_MidiOnNote;
extern BYTE g_NoteVelocity;
extern BYTE g_MidiOffNote;
extern UINT8 g_MinVelocity;
extern BOOL g_MidiChannelOutputs[MIDI_CHANNEL_COUNT];
extern UINT8 g_MidiChannelVelocity[MIDI_CHANNEL_COUNT];
extern UINT8 g_HiHatPedalPosition;
extern UINT8 g_HiHatThreshold;


// GLOBAL FUNCTIONS ======================================================

extern INT8 AddMidiNoteMapping(INT8 ChannelNumber, UINT8 MidiNote, UINT8 Velocity, BOOL Append);
extern void ClearMidiOutputs(void);
extern void ClearMidiMapChannel(INT8 ChannelNumber);
extern void EraseMidiMap(void);
extern UINT8 GetMidiMapEntry(INT8 ChannelNumber, UINT8 NoteIndex);
extern void MIDI_Initialize(void);
extern void MIDI_ServiceUARTRx(void);
extern void RecallMidiMap(void);
extern void RestoreDefaultMap(UINT8 MapNumber);
extern void SetMidiMapEntry(INT8 ChannelNumber, UINT8 NoteIndex, UINT8 MidiNote);
extern void SetMidiMapNumber(BYTE Value, BOOL ReadTableFromEEPROM);

#endif // _INC_MIDI
