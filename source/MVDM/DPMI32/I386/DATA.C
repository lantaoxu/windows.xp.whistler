/*++

Copyright (c) 2000  Microsoft Corporation

Module Name:

    data.c

Abstract:

    This module contains the x86 specific data for the dpmi library.
    Common data is found in dpmi32.c

Author:

    Dave Hastings (daveh) creation-date 09-Feb-1994

Revision History:


--*/
#include "dpmi32p.h"
//
// Pointers to the low memory buffers
//
PUCHAR SmallXlatBuffer;
PUCHAR LargeXlatBuffer;
BOOL SmallBufferInUse;
USHORT LargeBufferInUseCount = 0;
//
// Segment of realmode dosx stack
//
USHORT DosxStackSegment;

//
// segment of realmode dosx code
//
USHORT DosxRmCodeSegment;

//
// selector of realmode code
//
USHORT DosxRmCodeSelector;

//
// Address of pointer to next frame on Dosx stack
//
PUSHORT DosxStackFramePointer;

//
// selector for Dosx DGROUP
//
USHORT DosxPmDataSelector;

//
// Segment to Selector routine (maybe should be 32 bit?)
//  NOTE: This is a 16:16 pointer in a ULONG, not a flat address
//
// Entry:
//      ds = Dosx DGROUP
//      ss = Dosx stack
//      AX = Paragraph to convert
//      BX = Access rights for new selector
//
// Exit:
//      Carry set for error
//      AX = New selector

ULONG DosxSegmentToSelector;

//
// Size of dosx stack frame
//
USHORT DosxStackFrameSize;

//
// Dpmi flags for the current application
//
USHORT CurrentAppFlags;

//
// Address of Bop fe for ending interrupt simulation
//
ULONG RmBopFe;

//
// Address of buffer for DTA in Dosx
//
PUCHAR DosxDtaBuffer;

//
// Information about the current DTA
//
// N.B.  The selector:offset, and CurrentDta following MAY point to
//       different linear addresses.  This will be the case if the
//       dta selector is in high memory.
//       CurrentDosDta holds the "cached" value of the Dta that has
//       actually been issued to DOS.
PUCHAR CurrentDta;
PUCHAR CurrentPmDtaAddress;
PUCHAR CurrentDosDta;
USHORT CurrentDtaSelector;
USHORT CurrentDtaOffset;

//
// Selector limits
//
#if DBG
ULONG SelectorLimit[LDT_SIZE];
#endif
