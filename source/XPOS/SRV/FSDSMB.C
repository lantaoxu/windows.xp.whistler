/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    fsdsmb.c

Abstract:

    This module implements SMB processing routines and their support
    routines for the File System Driver of the LAN Manager server.

    *** This module must be nonpageable.

Author:

    Chuck Lenzmeier (chuckl) 19-Mar-1990

Revision History:

--*/

//
//  This module is laid out as follows:
//      Includes
//      Local #defines
//      Local type definitions
//      Forward declarations of local functions
//      SMB processing routines
//      Restart routines and other support routines
//

#include "precomp.h"
#pragma hdrstop

#ifdef ALLOC_PRAGMA
//#pragma alloc_text( PAGE8FIL, SrvFsdRestartRead )
#pragma alloc_text( PAGE8FIL, SrvFsdRestartReadAndX )
#pragma alloc_text( PAGE8FIL, SrvFsdRestartWrite )
#pragma alloc_text( PAGE8FIL, SrvFsdRestartWriteAndX )
#endif

//
// Functions imported from smbrdwrt.c
//

STATIC
VOID
RestartChainedClose (
    IN OUT PWORK_CONTEXT WorkContext
    );


#if !defined(SRV_ASM) || !defined(i386)
VOID
SrvFsdRestartRead (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes file read completion for a Read SMB.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{
    PREQ_READ request;
    PRESP_READ response;

    NTSTATUS status;
    PRFCB rfcb;
    SHARE_TYPE shareType;
    KIRQL oldIrql;
    USHORT readLength;

    UNLOCKABLE_CODE( 8FIL );

    IF_DEBUG(FSD2) SrvPrint0( " - SrvFsdRestartRead\n" );

    //
    // Get the request and response parameter pointers.
    //

    request = (PREQ_READ)WorkContext->RequestParameters;
    response = (PRESP_READ)WorkContext->ResponseParameters;

    //
    // Get the file pointer.
    //

    rfcb = WorkContext->Rfcb;
    shareType = rfcb->ShareType;
    IF_DEBUG(FSD2) {
        SrvPrint2( "  connection 0x%lx, RFCB 0x%lx\n",
                    WorkContext->Connection, rfcb );
    }

    //
    // If the read failed, set an error status in the response header.
    // (If we tried to read entirely beyond the end of file, we return a
    // normal response indicating that nothing was read.)
    //

    status = WorkContext->Irp->IoStatus.Status;
    readLength = (USHORT)WorkContext->Irp->IoStatus.Information;

    if ( status == STATUS_BUFFER_OVERFLOW && shareType == ShareTypePipe ) {

        //
        // If this is an named pipe and the error is
        // STATUS_BUFFER_OVERFLOW, set the error in the smb header, but
        // return all the data to the client.
        //

        SrvSetBufferOverflowError( WorkContext );

    } else if ( !NT_SUCCESS(status) ) {

        if ( status != STATUS_END_OF_FILE ) {

            IF_DEBUG(ERRORS) SrvPrint1( "Read failed: %X\n", status );
            if ( KeGetCurrentIrql() >= DISPATCH_LEVEL ) {
                WorkContext->FspRestartRoutine = SrvFsdRestartRead;
                QUEUE_WORK_TO_FSP( WorkContext );
            } else {
                SrvSetSmbError( WorkContext, status );
                SrvFsdSendResponse( WorkContext );
            }

            IF_DEBUG(FSD2) SrvPrint0( "SrvFsdRestartRead complete\n" );
            return;
        } else {
            readLength = 0;
        }
    }

#ifdef SLMDBG
    {
        PRFCB_TRACE entry;
        PCHAR readAddress;
        ACQUIRE_GLOBAL_SPIN_LOCK( Fsd, &oldIrql );
        rfcb->OperationCount++;
        entry = &rfcb->Trace[rfcb->NextTrace];
        if ( ++rfcb->NextTrace == SLMDBG_TRACE_COUNT ) {
            rfcb->NextTrace = 0;
            rfcb->TraceWrapped = TRUE;
        }
        RELEASE_GLOBAL_SPIN_LOCK( Fsd, oldIrql );
        entry->Command = WorkContext->NextCommand;
        KeQuerySystemTime( &entry->Time );
        entry->Data.ReadWrite.Offset = SmbGetUlong( &request->Offset );
        entry->Data.ReadWrite.Length = readLength;
        readAddress = (PCHAR)response->Buffer;
    }
#endif

    //
    // The read completed successfully.  If this is a disk file, update
    // the file position.
    //

    if (shareType == ShareTypeDisk) {

        LARGE_INTEGER position;

        position.QuadPart = SmbGetUlong( &request->Offset ) + readLength;

        ACQUIRE_SPIN_LOCK( &rfcb->SpinLock, &oldIrql );
        rfcb->CurrentPosition = position;
        RELEASE_SPIN_LOCK( &rfcb->SpinLock, oldIrql );

    }

    //
    // Save the count of bytes read, to be used to update the server
    // statistics database.
    //

    UPDATE_READ_STATS( readLength );

    //
    // Build the response message.
    //

    response->WordCount = 5;
    SmbPutUshort( &response->Count, readLength );
    RtlZeroMemory( (PVOID)&response->Reserved[0], sizeof(response->Reserved) );
    SmbPutUshort(
        &response->ByteCount,
        (USHORT)(readLength + FIELD_OFFSET(RESP_READ,Buffer[0]) -
                                FIELD_OFFSET(RESP_READ,BufferFormat))
        );
    response->BufferFormat = SMB_FORMAT_DATA;
    SmbPutUshort( &response->DataLength, readLength );

    WorkContext->ResponseParameters = NEXT_LOCATION(
                                        response,
                                        RESP_READ,
                                        readLength
                                        );

    //
    // Processing of the SMB is complete.  Send the response.
    //

    SrvFsdSendResponse( WorkContext );

    IF_DEBUG(FSD2) SrvPrint0( "SrvFsdRestartRead complete\n" );
    return;

} // SrvFsdRestartRead
#endif // !defined(SRV_ASM) || !defined(i386)


VOID
SrvFsdRestartReadAndX (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes file read completion for a ReadAndX SMB.

    This routine may be called in the FSD or the FSP.  If the chained
    command is Close, it will be called in the FSP.

    *** This routine cannot look at the original ReadAndX request!
        This is because the read data may have overlaid the request.
        All necessary information from the request must be stored
        in WorkContext->Parameters.ReadAndX.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{
    PRESP_READ_ANDX response;

    NTSTATUS status;
    PRFCB rfcb;
    SHARE_TYPE shareType;
    KIRQL oldIrql;
    PCHAR readAddress;
    CLONG bufferOffset;
    USHORT readLength;

    UNLOCKABLE_CODE( 8FIL );

    IF_DEBUG(FSD2) SrvPrint0( " - SrvFsdRestartReadAndX\n" );

    //
    // Get the response parameter pointer.
    //

    response = (PRESP_READ_ANDX)WorkContext->ResponseParameters;

    //
    // Get the file pointer.
    //

    rfcb = WorkContext->Rfcb;
    shareType = rfcb->ShareType;
    IF_DEBUG(FSD2) {
        SrvPrint2( "  connection 0x%lx, RFCB 0x%lx\n",
                    WorkContext->Connection, rfcb );
    }

    //
    // If the read failed, set an error status in the response header.
    // (If we tried to read entirely beyond the end of file, we return a
    // normal response indicating that nothing was read.)
    //

    status = WorkContext->Irp->IoStatus.Status;
    readLength = (USHORT)WorkContext->Irp->IoStatus.Information;

    if ( status == STATUS_BUFFER_OVERFLOW && shareType == ShareTypePipe ) {

        //
        // If this is an named pipe and the error is
        // STATUS_BUFFER_OVERFLOW, set the error in the smb header, but
        // return all the data to the client.
        //

        SrvSetBufferOverflowError( WorkContext );

    } else if ( !NT_SUCCESS(status) ) {

        if ( status != STATUS_END_OF_FILE ) {

            IF_DEBUG(ERRORS) SrvPrint1( "Read failed: %X\n", status );
            if ( KeGetCurrentIrql() >= DISPATCH_LEVEL ) {
                WorkContext->FspRestartRoutine = SrvFsdRestartReadAndX;
                QUEUE_WORK_TO_FSP( WorkContext );
            } else {
                SrvSetSmbError( WorkContext, status );
                SrvFsdSendResponse( WorkContext );
            }

            IF_DEBUG(FSD2) SrvPrint0( "SrvFsdRestartReadAndX complete\n" );
            return;
        } else {
            readLength = 0;
        }
    }

    //
    // The read completed successfully.  Generate information about the
    // destination of the read data.  Find out how much was actually
    // read.  If none was read, we don't have to worry about the offset.
    //

    if ( readLength != 0 ) {

        readAddress = WorkContext->Parameters.ReadAndX.ReadAddress;
        bufferOffset = readAddress - (PCHAR)WorkContext->ResponseHeader;

        //
        // Save the count of bytes read, to be used to update the server
        // statistics database.
        //

        UPDATE_READ_STATS( readLength );

    } else {

        readAddress = (PCHAR)response->Buffer;
        bufferOffset = 0;

    }

#ifdef SLMDBG
    {
        PRFCB_TRACE entry;
        ACQUIRE_GLOBAL_SPIN_LOCK( Fsd, &oldIrql );
        rfcb->OperationCount++;
        entry = &rfcb->Trace[rfcb->NextTrace];
        if ( ++rfcb->NextTrace == SLMDBG_TRACE_COUNT ) {
            rfcb->NextTrace = 0;
            rfcb->TraceWrapped = TRUE;
        }
        RELEASE_GLOBAL_SPIN_LOCK( Fsd, oldIrql );
        entry->Command = WorkContext->NextCommand;
        KeQuerySystemTime( &entry->Time );
        entry->Data.ReadWrite.Offset =
                            WorkContext->Parameters.ReadAndX.ReadOffset.LowPart;
        ASSERT (WorkContext->Parameters.ReadAndX.ReadOffset.HighPart == 0);
        entry->Data.ReadWrite.Length = readLength;
    }
#endif

    if (shareType == ShareTypePipe) {

        //
        // If this is NPFS then, Irp->Overlay.AllocationSize actually
        // contains the number bytes left to read on this side of the named
        // pipe.  Return this information to the client.
        //

        if (WorkContext->Irp->Overlay.AllocationSize.LowPart != 0) {
            SmbPutUshort(
                &response->Remaining,
                (USHORT)WorkContext->Irp->Overlay.AllocationSize.LowPart - readLength
                );
        } else {
            SmbPutUshort(
                &response->Remaining,
                0
                );
        }

    } else {

        if ( shareType == ShareTypeDisk ) {

            //
            // If this is a disk file, then update the file position.
            //

            LARGE_INTEGER position;

            position.QuadPart =
                WorkContext->Parameters.ReadAndX.ReadOffset.QuadPart +
                readLength;

            ACQUIRE_SPIN_LOCK( &rfcb->SpinLock, &oldIrql );
            rfcb->CurrentPosition = position;
            RELEASE_SPIN_LOCK( &rfcb->SpinLock, oldIrql );
        }

        SmbPutUshort( &response->Remaining, (USHORT)-1 );
    }

    //
    // Build the response message.  (Note that if no data was read, we
    // return a byte count of 0 -- we don't add padding.)
    //
    // *** Note that even though there may have been a chained command,
    //     we make this the last response in the chain.  This is what
    //     the OS/2 server does.  (Sort of -- it doesn't bother to
    //     update the AndX fields of the response.) Since the only legal
    //     chained commands are Close and CloseAndTreeDisc, this seems
    //     like a reasonable thing to do.  It does make life easier --
    //     we don't have to find the end of the read data and write
    //     another response there.  Besides, the read data might have
    //     completely filled the SMB buffer.
    //

    response->WordCount = 12;
    response->AndXCommand = SMB_COM_NO_ANDX_COMMAND;
    response->AndXReserved = 0;
    SmbPutUshort( &response->AndXOffset, 0 );
    SmbPutUshort( &response->DataCompactionMode, 0 );
    SmbPutUshort( &response->Reserved, 0 );
    SmbPutUshort( &response->DataLength, (USHORT)readLength );
    SmbPutUshort( &response->DataOffset, (USHORT)bufferOffset );
    SmbPutUshort( &response->Reserved2, 0 );
    RtlZeroMemory( (PVOID)&response->Reserved3[0], sizeof(response->Reserved3) );
    SmbPutUshort(
        &response->ByteCount,
        (USHORT)(readLength + (readAddress - response->Buffer))
        );

    WorkContext->ResponseParameters = NEXT_LOCATION(
                                        response,
                                        RESP_READ_ANDX,
                                        readLength +
                                            (readAddress - response->Buffer)
                                        );

    //
    // Processing of the SMB is complete, except that the file may still
    // need to be closed.  If not, just send the response.  If this is a
    // ReadAndX and Close, we need to close the file first.
    //
    // *** Note that other chained commands are illegal, but are ignored
    //     -- no error is returned.
    //

    if ( WorkContext->NextCommand != SMB_COM_CLOSE ) {

        //
        // Not a chained Close.  Just send the response.
        //

        SrvFsdSendResponse( WorkContext );

    } else {

        ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

        //
        // Remember the file last write time, to correctly set this on
        // close.
        //

        WorkContext->Parameters.LastWriteTime =
                WorkContext->Parameters.ReadAndX.LastWriteTimeInSeconds;

        //
        // This is a ReadAndX and Close.  Call RestartChainedClose to
        // do the close and send the response.
        //

        RestartChainedClose( WorkContext );

    }

    IF_DEBUG(FSD2) SrvPrint0( "SrvFsdRestartReadAndX complete\n" );
    return;

} // SrvFsdRestartReadAndX


VOID
SrvFsdRestartWrite (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes file write completion for a Write SMB.

    This routine is called in the FSP for a write and close SMB so that
    it can free the pageable MFCB and for a write and unlock SMB so that
    it can do the unlock; for other SMBs, it is called in the FSD.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{
    PREQ_WRITE request;
    PRESP_WRITE response;

    NTSTATUS status;
    PRFCB rfcb;
    KIRQL oldIrql;
    USHORT writeLength;

    UNLOCKABLE_CODE( 8FIL );

    IF_DEBUG(FSD2) SrvPrint0( " - SrvFsdRestartWrite\n" );

    //
    // Get the request and response parameter pointers.
    //

    request = (PREQ_WRITE)WorkContext->RequestParameters;
    response = (PRESP_WRITE)WorkContext->ResponseParameters;

    //
    // Get the file pointer.
    //

    rfcb = WorkContext->Rfcb;
    IF_DEBUG(FSD2) {
        SrvPrint2( "  connection 0x%lx, RFCB 0x%lx\n",
                    WorkContext->Connection, rfcb );
    }

    //
    // If the write failed, set an error status in the response header.
    //

    status = WorkContext->Irp->IoStatus.Status;

    if ( !NT_SUCCESS(status) ) {

        IF_DEBUG(ERRORS) SrvPrint1( "Write failed: %X\n", status );
        if ( KeGetCurrentIrql() >= DISPATCH_LEVEL ) {
            WorkContext->FspRestartRoutine = SrvFsdRestartWrite;
            QUEUE_WORK_TO_FSP( WorkContext );
            return;
        }

        SrvSetSmbError( WorkContext, status );

    } else {

        //
        // The write succeeded.
        //

        writeLength = (USHORT)WorkContext->Irp->IoStatus.Information;

        //
        // Save the count of bytes written, to be used to update the
        // server statistics database.
        //

        UPDATE_WRITE_STATS( writeLength );

        if ( rfcb->ShareType == ShareTypeDisk ) {

            LARGE_INTEGER position;

            //
            // Update the file position.
            //

            position.QuadPart = SmbGetUlong( &request->Offset ) + writeLength;

            ACQUIRE_SPIN_LOCK( &rfcb->SpinLock, &oldIrql );
            rfcb->CurrentPosition = position;
            RELEASE_SPIN_LOCK( &rfcb->SpinLock, oldIrql );

            if ( WorkContext->NextCommand == SMB_COM_WRITE ) {
                response->WordCount = 1;
                SmbPutUshort( &response->Count, writeLength );
                SmbPutUshort( &response->ByteCount, 0 );

                WorkContext->ResponseParameters =
                                         NEXT_LOCATION( response, RESP_WRITE, 0 );

                //
                // Processing of the SMB is complete.  Send the response.
                //

                SrvFsdSendResponse( WorkContext );

                IF_DEBUG(FSD2) SrvPrint0( "SrvFsdRestartWrite complete\n" );
                return;
            }

        } else if ( rfcb->ShareType == ShareTypePrint ) {

            LARGE_INTEGER position;

            //
            // Update the file position.
            //

            ACQUIRE_SPIN_LOCK( &rfcb->SpinLock, &oldIrql );
            if ( WorkContext->NextCommand == SMB_COM_WRITE_PRINT_FILE ) {
                position.QuadPart = rfcb->CurrentPosition.QuadPart + writeLength;
            } else {
                position.QuadPart = SmbGetUlong( &request->Offset ) + writeLength;
            }
            rfcb->CurrentPosition = position;
            RELEASE_SPIN_LOCK( &rfcb->SpinLock, oldIrql );
        }

        //
        // If this was a Write and Unlock request, do the unlock.  This
        // is safe because we are restarted in the FSP in this case.
        //
        // Note that if the write failed, the range remains locked.
        //

        if ( WorkContext->NextCommand == SMB_COM_WRITE_AND_UNLOCK ) {

            IF_SMB_DEBUG(READ_WRITE1) {
                SrvPrint0( "SrvFsdRestartWrite: unlock requested -- "
                            "passing request to FSP\n" );
            }

            SrvRestartWriteAndUnlock( WorkContext );
            return;

        } else if ( WorkContext->NextCommand == SMB_COM_WRITE_AND_CLOSE ) {

            WorkContext->Parameters.LastWriteTime = SmbGetUlong(
                &((PREQ_WRITE_AND_CLOSE)request)->LastWriteTimeInSeconds );

        }

        //
        // If everything worked, build a response message.  (If something
        // failed, an error indication has already been placed in the SMB.)
        //

        if ( WorkContext->NextCommand == SMB_COM_WRITE_PRINT_FILE ) {

            //
            // ByteCount has a different offset for WRITE_PRINT_FILE
            //

            PRESP_WRITE_PRINT_FILE response2;

            response2 = (PRESP_WRITE_PRINT_FILE)WorkContext->ResponseParameters;
            response2->WordCount = 0;
            SmbPutUshort( &response2->ByteCount, 0 );

            WorkContext->ResponseParameters =
                          NEXT_LOCATION( response2, RESP_WRITE_PRINT_FILE, 0 );
        } else {

            response->WordCount = 1;
            SmbPutUshort( &response->Count, writeLength );
            SmbPutUshort( &response->ByteCount, 0 );

            WorkContext->ResponseParameters =
                                     NEXT_LOCATION( response, RESP_WRITE, 0 );
        }
    }

    //
    // If this was a Write and Close request, close the file.  It is
    // safe to close the RFCB here because if this is a Write and Close,
    // we're actually in the FSP, not in the FSD.
    //

    if ( WorkContext->NextCommand == SMB_COM_WRITE_AND_CLOSE ) {

        ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

        RestartChainedClose( WorkContext );

        return;

    }

    //
    // Processing of the SMB is complete.  Send the response.
    //

    SrvFsdSendResponse( WorkContext );

    IF_DEBUG(FSD2) SrvPrint0( "SrvFsdRestartWrite complete\n" );
    return;

} // SrvFsdRestartWrite


VOID
SrvFsdRestartWriteAndX (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes file write completion for a Write and X SMB.

    This routine may be called in the FSD or the FSP.  If the chained
    command is Close, it will be called in the FSP.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{
    PREQ_WRITE_ANDX request;
    PREQ_NT_WRITE_ANDX ntRequest;
    PRESP_WRITE_ANDX response;

    NTSTATUS status;
    PRFCB rfcb;
    KIRQL oldIrql;
    USHORT writeLength;
    USHORT requestedWriteLength;
    UCHAR nextCommand;
    USHORT nextOffset;
    USHORT reqAndXOffset;
    LARGE_INTEGER position;

    PREQ_CLOSE closeRequest;

    UNLOCKABLE_CODE( 8FIL );

    IF_DEBUG(FSD2) SrvPrint0( " - SrvFsdRestartWriteAndX\n" );

    //
    // Get the request and response parameter pointers.
    //

    request = (PREQ_WRITE_ANDX)WorkContext->RequestParameters;
    ntRequest = (PREQ_NT_WRITE_ANDX)WorkContext->RequestParameters;
    response = (PRESP_WRITE_ANDX)WorkContext->ResponseParameters;

    //
    // Get the file pointer.
    //

    rfcb = WorkContext->Rfcb;
    IF_DEBUG(FSD2) {
        SrvPrint2( "  connection 0x%lx, RFCB 0x%lx\n",
                    WorkContext->Connection, rfcb );
    }

    //
    // Remember where the follow-on request begins, and what the next
    // command is, as we are about to overwrite this information.
    //

    reqAndXOffset = SmbGetUshort( &request->AndXOffset );

    nextCommand = request->AndXCommand;
    WorkContext->NextCommand = nextCommand;
    nextOffset = SmbGetUshort( &request->AndXOffset );

    //
    // If the write failed, set an error status in the response header.
    // We still return a valid parameter block, in case some bytes were
    // written before the error occurred.  Note that we do _not_ process
    // the next command if the write failed.
    //
    // *** OS/2 server behavior.  Note that this is _not_ done for core
    //     Write.
    //

    status = WorkContext->Irp->IoStatus.Status;

    if ( !NT_SUCCESS(status) ) {

        IF_DEBUG(ERRORS) SrvPrint1( "Write failed: %X\n", status );
        if ( KeGetCurrentIrql() >= DISPATCH_LEVEL ) {
            WorkContext->FspRestartRoutine = SrvFsdRestartWriteAndX;
            QUEUE_WORK_TO_FSP( WorkContext );
            return;
        }

        SrvSetSmbError( WorkContext, status );
        nextCommand = SMB_COM_NO_ANDX_COMMAND;
    }

    //
    // Update the file position.
    //

    writeLength = (USHORT)WorkContext->Irp->IoStatus.Information;

    if ( rfcb->ShareType != ShareTypePipe ) {

        if ( request->ByteCount == 12 ) {

            //
            // The client supplied a 32-bit file offset.
            //

            position.QuadPart = SmbGetUlong( &request->Offset ) + writeLength;

        } else {

            //
            // The client supplied a 64-bit file offset.
            //

            LARGE_INTEGER liOffset;

            liOffset.HighPart = SmbGetUlong( &ntRequest->Offset );
            liOffset.LowPart = SmbGetUlong( &ntRequest->OffsetHigh );

            position.QuadPart = liOffset.QuadPart + writeLength;

        }

        ACQUIRE_SPIN_LOCK( &rfcb->SpinLock, &oldIrql );
        rfcb->CurrentPosition = position;
        RELEASE_SPIN_LOCK( &rfcb->SpinLock, oldIrql );
    }

    //
    // Save the count of bytes written, to be used to update the server
    // statistics database.
    //

    UPDATE_WRITE_STATS( writeLength );

    IF_SMB_DEBUG(READ_WRITE1) {
        SrvPrint2( "SrvFsdRestartWriteAndX:  Fid 0x%lx, wrote %ld bytes\n",
                  rfcb->Fid, writeLength );
    }

    //
    // Build the response message.
    //

    requestedWriteLength = SmbGetUshort( &request->DataLength );

    response->AndXCommand = nextCommand;
    response->AndXReserved = 0;
    SmbPutUshort(
        &response->AndXOffset,
        GET_ANDX_OFFSET(
            WorkContext->ResponseHeader,
            WorkContext->ResponseParameters,
            RESP_WRITE_ANDX,
            0
            )
        );

    response->WordCount = 6;

    if ( WorkContext->Parameters.Transaction == NULL ) {
        SmbPutUshort( &response->Count, writeLength );
    } else {
        SmbPutUshort( &response->Count, requestedWriteLength );
    }
    SmbPutUshort( &response->Remaining, (USHORT)-1 );
    SmbPutUlong( &response->Reserved, 0 );
    SmbPutUshort( &response->ByteCount, 0 );

    WorkContext->ResponseParameters = (PCHAR)WorkContext->ResponseHeader +
                                        SmbGetUshort( &response->AndXOffset );

    WorkContext->RequestParameters = (PUCHAR)WorkContext->RequestHeader + reqAndXOffset;

    //
    // If this was a raw mode write, queue the work to the FSP for
    // completion.  The FSP routine will handling dispatching of the
    // AndX command.
    //

    if ( WorkContext->Parameters.Transaction != NULL ) {
        WorkContext->FspRestartRoutine = SrvRestartWriteAndXRaw;
        SrvQueueWorkToFsp( WorkContext );
        return;
    }

    //
    // Test for a legal followon command, and dispatch as appropriate.
    // Close is handled specially.
    //

    switch ( nextCommand ) {

    case SMB_COM_NO_ANDX_COMMAND:

        //
        // No more commands.  Send the response.
        //

        SrvFsdSendResponse( WorkContext );
        break;

    case SMB_COM_READ:
    case SMB_COM_READ_ANDX:
    case SMB_COM_LOCK_AND_READ:
    case SMB_COM_WRITE_ANDX:

        //
        // Queue the work item back to the FSP for further processing.
        //

        WorkContext->FspRestartRoutine = SrvRestartSmbReceived;
        SrvQueueWorkToFsp( WorkContext );

        break;

    case SMB_COM_CLOSE:

        //
        // Save the last write time, to correctly set it.  Call
        // RestartChainedClose to close the file and send the response.
        //

        closeRequest = (PREQ_CLOSE)
            ((PUCHAR)WorkContext->RequestHeader + reqAndXOffset);
        WorkContext->Parameters.LastWriteTime =
            closeRequest->LastWriteTimeInSeconds;

        RestartChainedClose( WorkContext );

        break;

    default:                            // Illegal followon command

        IF_DEBUG(SMB_ERRORS) {
            SrvPrint1( "SrvFsdRestartWriteAndX: Illegal followon "
                        "command: 0x%lx\n", nextCommand );
        }

        if ( KeGetCurrentIrql() >= DISPATCH_LEVEL ) {
            WorkContext->Irp->IoStatus.Status = STATUS_INVALID_SMB;
            WorkContext->FspRestartRoutine = SrvBuildAndSendErrorResponse;
            WorkContext->FsdRestartRoutine = SrvFsdRestartSmbComplete; // after response
            QUEUE_WORK_TO_FSP( WorkContext );
        } else {
            SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
            SrvFsdSendResponse( WorkContext );
        }

    }

    IF_DEBUG(TRACE2) SrvPrint0( "SrvFsdRestartWriteAndX complete\n" );
    return;

} // SrvFsdRestartWriteAndX

