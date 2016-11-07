#ifndef _INC_EEDATA
#define _INC_EEDATA

#include <GenericTypeDefs.h>

BYTE ReadEEData(BYTE Address);
void WriteEEData(BYTE Address, BYTE Data);

#endif