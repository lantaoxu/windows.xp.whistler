//
//
//  CTESTUFF.C
//
//  This file contains Common Transport Environment code to handle
//  OS dependent functions such as allocating memory etc.
//
//
#include "types.h"

// to convert a millisecond to a 100ns time
//
#define MILLISEC_TO_100NS   10000


//----------------------------------------------------------------------------
PVOID
CTEStartTimer(
    IN  CTETimer        *pTimerIn,
    IN  ULONG           DeltaTime,
    IN  CTEEventRtn     TimerExpiry,
    IN  PVOID           Context OPTIONAL
        )
/*++
Routine Description:

    This Routine starts a timer.

Arguments:

    Timer       - Timer structure
    TimerExpiry - completion routine

Return Value:

    PVOID - a pointer to the memory or NULL if a failure

--*/

{
    LARGE_INTEGER   Time;

    //
    // initialize the DPC to have the correct completion routine and context
    //
    KeInitializeDpc(&pTimerIn->t_dpc,
                    (PVOID)TimerExpiry,     // completion routine
                    Context);               // context value

    //
    // convert to 100 ns units by multiplying by 10,000
    //
    Time = RtlEnlargedUnsignedMultiply(DeltaTime,(LONG)MILLISEC_TO_100NS);

    //
    // to make a delta time, negate the time
    //
    Time = RtlLargeIntegerNegate(Time);

    ASSERT(RtlLargeIntegerLessThanZero(Time));

    (VOID)KeSetTimer(&pTimerIn->t_timer,Time,&pTimerIn->t_dpc);

    return(NULL);
}
//----------------------------------------------------------------------------
VOID
CTEInitTimer(
    IN  CTETimer        *pTimerIn
        )
/*++
Routine Description:

    This Routine initializes a timer.

Arguments:

    Timer       - Timer structure
    TimerExpiry - completion routine

Return Value:

    PVOID - a pointer to the memory or NULL if a failure

--*/

{
    KeInitializeTimer(&pTimerIn->t_timer);
}
