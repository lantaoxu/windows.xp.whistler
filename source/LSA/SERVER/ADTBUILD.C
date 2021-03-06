/*++

Copyright (c) 2000  Microsoft Corporation

Module Name:

    adtbuild.c

Abstract:

    Local Security Authority - Audit Log Management

    Functions in this module build unicode strings for
    various parameter types.  Some parameter string build
    routines may also be found in other modules (such as
    LsapAdtBuildAccessesString() in adtobjs.c).

Author:

    Jim Kelly           (JimK)         29-Oct-2000


Environment:

Revision History:

--*/

#include <msaudite.h>
#include "lsasrvp.h"
#include "adtp.h"
#include "ausrvp.h"

#ifndef LSAP_ADT_UMTEST
//
// pick up definitions of privately callable LSA services ONLY if we
// aren't building a user mode test.
//

#include "aup.h"

#endif








////////////////////////////////////////////////////////////////////////
//                                                                    //
//  Local Macro definitions and local function prototypes             //
//                                                                    //
////////////////////////////////////////////////////////////////////////



#ifdef LSAP_ADT_UMTEST

//
// Define all external routines that we won't pick up in a user mode test
//

NTSTATUS
LsapGetLogonSessionAccountInfo(
    IN PLUID Value,
    OUT PUNICODE_STRING AccountName,
    OUT PUNICODE_STRING AuthorityName
    );



#endif



////////////////////////////////////////////////////////////////////////
//                                                                    //
//  Data types used within this module                                //
//                                                                    //
////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////
//                                                                    //
//  Variables global within this module                               //
//                                                                    //
////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////
//                                                                    //
//  Services exported by this module.                                 //
//                                                                    //
////////////////////////////////////////////////////////////////////////


NTSTATUS
LsapAdtBuildUlongString(
    IN ULONG Value,
    OUT PUNICODE_STRING ResultantString,
    OUT PBOOLEAN FreeWhenDone
    )

/*++

Routine Description:

    This function builds a unicode string representing the passed value.

    The resultant string will be formatted as a decimal value with not
    more than 10 digits.


Arguments:

    Value - The value to be transformed to printable format (Unicode string).

    ResultantString - Points to the unicode string header.  The body of this
        unicode string will be set to point to the resultant output value
        if successful.  Otherwise, the Buffer field of this parameter
        will be set to NULL.

    FreeWhenDone - If TRUE, indicates that the body of the ResultantString
        must be freed to process heap when no longer needed.


Return Values:

    STATUS_NO_MEMORY - indicates memory could not be allocated
        for the string body.

    All other Result Codes are generated by called routines.

--*/

{
    NTSTATUS                Status;



    //
    // Maximum length is 10 wchar characters plus a null termination character.
    //

    ResultantString->Length        = 0;
    ResultantString->MaximumLength = 11 * sizeof(WCHAR); // 10 digits & null termination

    ResultantString->Buffer = RtlAllocateHeap( RtlProcessHeap(), 0,
                                               ResultantString->MaximumLength);
    if (ResultantString->Buffer == NULL) {
        return(STATUS_NO_MEMORY);
    }




    Status = RtlIntegerToUnicodeString( Value, 10, ResultantString );
    ASSERT(NT_SUCCESS(Status));


    (*FreeWhenDone) = TRUE;
    return(STATUS_SUCCESS);



}


NTSTATUS
LsapAdtBuildLuidString(
    IN PLUID Value,
    OUT PUNICODE_STRING ResultantString,
    OUT PBOOLEAN FreeWhenDone
    )

/*++

Routine Description:

    This function builds a unicode string representing the passed LUID.

    The resultant string will be formatted as follows:

        (0x00005678,0x12340000)

Arguments:

    Value - The value to be transformed to printable format (Unicode string).

    ResultantString - Points to the unicode string header.  The body of this
        unicode string will be set to point to the resultant output value
        if successful.  Otherwise, the Buffer field of this parameter
        will be set to NULL.

    FreeWhenDone - If TRUE, indicates that the body of the ResultantString
        must be freed to process heap when no longer needed.


Return Values:

    STATUS_NO_MEMORY - indicates memory could not be allocated
        for the string body.

    All other Result Codes are generated by called routines.

--*/

{
    NTSTATUS                Status;

    UNICODE_STRING          IntegerString;


    ULONG                   Buffer[(16*sizeof(WCHAR))/sizeof(ULONG)];


    IntegerString.Buffer = (PWCHAR)&Buffer[0];
    IntegerString.MaximumLength = 16*sizeof(WCHAR);


    //
    // Length (in WCHARS) is  3 for   (0x
    //                       10 for   1st hex number
    //                        3 for   ,0x
    //                       10 for   2nd hex number
    //                        1 for   )
    //                        1 for   null termination
    //

    ResultantString->Length        = 0;
    ResultantString->MaximumLength = 28 * sizeof(WCHAR);

    ResultantString->Buffer = RtlAllocateHeap( RtlProcessHeap(), 0,
                                               ResultantString->MaximumLength);
    if (ResultantString->Buffer == NULL) {
        return(STATUS_NO_MEMORY);
    }



    Status = RtlAppendUnicodeToString( ResultantString, L"(0x" );
    ASSERT(NT_SUCCESS(Status));


    Status = RtlIntegerToUnicodeString( Value->HighPart, 16, &IntegerString );
    ASSERT(NT_SUCCESS(Status));
    Status = RtlAppendUnicodeToString( ResultantString, IntegerString.Buffer );
    ASSERT(NT_SUCCESS(Status));


    Status = RtlAppendUnicodeToString( ResultantString, L",0x" );
    ASSERT(NT_SUCCESS(Status));

    Status = RtlIntegerToUnicodeString( Value->LowPart, 16, &IntegerString );
    ASSERT(NT_SUCCESS(Status));
    Status = RtlAppendUnicodeToString( ResultantString, IntegerString.Buffer );
    ASSERT(NT_SUCCESS(Status));

    Status = RtlAppendUnicodeToString( ResultantString, L")" );
    ASSERT(NT_SUCCESS(Status));


    (*FreeWhenDone) = TRUE;
    return(STATUS_SUCCESS);



}


NTSTATUS
LsapAdtBuildSidString(
    IN PSID Value,
    OUT PUNICODE_STRING ResultantString,
    OUT PBOOLEAN FreeWhenDone
    )

/*++

Routine Description:

    This function builds a unicode string representing the passed LUID.

    The resultant string will be formatted as follows:

        S-1-281736-12-72-9-110
              ^    ^^ ^^ ^ ^^^
              |     |  | |  |
              +-----+--+-+--+---- Decimal

Arguments:

    Value - The value to be transformed to printable format (Unicode string).

    ResultantString - Points to the unicode string header.  The body of this
        unicode string will be set to point to the resultant output value
        if successful.  Otherwise, the Buffer field of this parameter
        will be set to NULL.

    FreeWhenDone - If TRUE, indicates that the body of the ResultantString
        must be freed to process heap when no longer needed.


Return Values:

    STATUS_NO_MEMORY - indicates memory could not be allocated
        for the string body.

    All other Result Codes are generated by called routines.

--*/

{
    NTSTATUS                Status;


    //
    // Tell the RTL routine to allocate the buffer.
    //

    Status = RtlConvertSidToUnicodeString( ResultantString, Value, TRUE );



    (*FreeWhenDone) = TRUE;
    return(Status);



}



NTSTATUS
LsapAdtBuildDashString(
    OUT PUNICODE_STRING ResultantString,
    OUT PBOOLEAN FreeWhenDone
    )

/*++

Routine Description:

    This function returns a string containing a dash ("-").
    This is commonly used to represent "No value" in audit records.


Arguments:


    ResultantString - Points to the unicode string header.  The body of this
        unicode string will be set to point to the resultant output value
        if successful.  Otherwise, the Buffer field of this parameter
        will be set to NULL.

    FreeWhenDone - If TRUE, indicates that the body of the ResultantString
        must be freed to process heap when no longer needed.


Return Values:

    STATUS_SUCCESS only.

--*/

{
//    NTSTATUS                Status;


    RtlInitUnicodeString(ResultantString, L"-");

    (*FreeWhenDone) = FALSE;
    return(TRUE);



}




NTSTATUS
LsapAdtBuildFilePathString(
    IN PUNICODE_STRING Value,
    OUT PUNICODE_STRING ResultantString,
    OUT PBOOLEAN FreeWhenDone
    )

/*++

Routine Description:

    This function builds a unicode string representing the passed file
    path name.  If possible, the string will be generated using drive
    letters instead of object architecture namespace.


Arguments:

    Value - The original file path name.  This is expected (but does not
        have to be) a standard NT object architecture name-space pathname.

    ResultantString - Points to the unicode string header.  The body of this
        unicode string will be set to point to the resultant output value
        if successful.  Otherwise, the Buffer field of this parameter
        will be set to NULL.

    FreeWhenDone - If TRUE, indicates that the body of the ResultantString
        must be freed to process heap when no longer needed.


Return Values:

    STATUS_NO_MEMORY - indicates memory could not be allocated
        for the string body.

    All other Result Codes are generated by called routines.

--*/

{
    NTSTATUS                Status = STATUS_SUCCESS;



    //
    // For now, don't do the conversion.
    // Do this if we have time before we ship.
    //

    ResultantString->Length        = Value->Length;
    ResultantString->Buffer        = Value->Buffer;
    ResultantString->MaximumLength = Value->MaximumLength;


    (*FreeWhenDone) = FALSE;
    return(Status);
}




NTSTATUS
LsapAdtBuildLogonIdStrings(
    IN PLUID LogonId,
    OUT PUNICODE_STRING ResultantString1,
    OUT PBOOLEAN FreeWhenDone1,
    OUT PUNICODE_STRING ResultantString2,
    OUT PBOOLEAN FreeWhenDone2,
    OUT PUNICODE_STRING ResultantString3,
    OUT PBOOLEAN FreeWhenDone3
    )

/*++

Routine Description:

    This function builds a 3 unicode strings representing the specified
    logon ID.  These strings will contain the username, domain, and
    LUID string of the specified logon session (respectively).


Arguments:

    Value - The logon ID.

    ResultantString1 - Points to the unicode string header.  The body of this
        unicode string will be set to point to the resultant output value
        if successful.  Otherwise, the Buffer field of this parameter
        will be set to NULL.

        This parameter will contain the username.


    FreeWhenDone1 - If TRUE, indicates that the body of ResultantString1
        must be freed to process heap when no longer needed.

    ResultantString2 - Points to the unicode string header.  The body of this
        unicode string will be set to point to the resultant output value
        if successful.  Otherwise, the Buffer field of this parameter
        will be set to NULL.

        This parameter will contain the username.


    FreeWhenDone2 - If TRUE, indicates that the body of ResultantString2
        must be freed to process heap when no longer needed.

    ResultantString3 - Points to the unicode string header.  The body of this
        unicode string will be set to point to the resultant output value
        if successful.  Otherwise, the Buffer field of this parameter
        will be set to NULL.

        This parameter will contain the username.


    FreeWhenDone3 - If TRUE, indicates that the body of ResultantString3
        must be freed to process heap when no longer needed.


Return Values:

    STATUS_NO_MEMORY - indicates memory could not be allocated
        for the string body.

    All other Result Codes are generated by called routines.

--*/

{
    NTSTATUS                Status;

    //
    // Try to convert the LUID first.
    //

    Status = LsapAdtBuildLuidString( LogonId, ResultantString3, FreeWhenDone3 );

    if (NT_SUCCESS(Status)) {


        //
        // Now get the username and domain names
        //

        Status = LsapGetLogonSessionAccountInfo( LogonId,
                                                 ResultantString1,
                                                 ResultantString2
                                                 );

        if (NT_SUCCESS(Status)) {

            (*FreeWhenDone1) = TRUE;
            (*FreeWhenDone2) = TRUE;

        } else {

            //
            // The LUID may be the system LUID
            //

            LUID SystemLuid = SYSTEM_LUID;

            if ( RtlEqualLuid( LogonId, &SystemLuid )) {

                RtlInitUnicodeString(ResultantString1, L"SYSTEM");
                RtlInitUnicodeString(ResultantString2, L"SYSTEM");

                (*FreeWhenDone1) = FALSE;
                (*FreeWhenDone2) = FALSE;

                Status = STATUS_SUCCESS;

            } else {

                //
                // We have no clue what this is, just free what we've
                // allocated.
                //

                if ((FreeWhenDone3)) {
                    LsapFreeLsaHeap( ResultantString3->Buffer );
                }
            }
        }
    }

    return(Status);

}






////////////////////////////////////////////////////////////////////////
//                                                                    //
//  Services private to this module.                                  //
//                                                                    //
////////////////////////////////////////////////////////////////////////




#ifdef LSAP_ADT_UMTEST

//
// Define this routine only for user mode test
//

NTSTATUS
LsapGetLogonSessionAccountInfo(
    IN PLUID Value,
    OUT PUNICODE_STRING AccountName,
    OUT PUNICODE_STRING AuthorityName
    )

{

    NTSTATUS        Status = STATUS_NO_MEMORY;


    if (RtlCreateUnicodeString( AccountName, L"Bullwinkle" )) {
        if (RtlCreateUnicodeString( AuthorityName, L"The Rocky Show" )) {
            Status = STATUS_SUCCESS;
        } else {
            RtlFreeHeap( RtlProcessHeap(), Accountame->Buffer );
        }
    }

    return(Status);




}
#endif
