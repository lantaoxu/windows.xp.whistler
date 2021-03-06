/*++

Copyright (c) 2000-1993  Microsoft Corporation

Module Name:

    spxsend.c

Abstract:

    This module contains code that implements the send engine for the
    SPX transport provider.

Author:

    Nikhil Kamkolkar (nikhilk) 11-November-1993

Environment:

    Kernel mode

Revision History:


--*/

#include "precomp.h"
#pragma hdrstop


//	Define module number for event logging entries
#define	FILENUM		SPXSEND

VOID
SpxSendComplete(
    IN PNDIS_PACKET pNdisPkt,
    IN NDIS_STATUS  NdisStatus
    )

/*++

Routine Description:

    This routine is called by the I/O system to indicate that a connection-
    oriented packet has been shipped and is no longer needed by the Physical
    Provider.

Arguments:

    ProtocolBindingContext - The ADAPTER structure for this binding.

    NdisPacket/RequestHandle - A pointer to the NDIS_PACKET that we sent.

    NdisStatus - the completion status of the send.

Return Value:

    none.

--*/

{
	PSPX_CONN_FILE	pSpxConnFile;
	PSPX_SEND_RESD	pSendResd;
	PNDIS_BUFFER	pNdisBuffer;
	CTELockHandle	lockHandle;
	UINT			bufCount;
	PREQUEST		pRequest	= NULL;
	BOOLEAN			completeReq = FALSE, freePkt = FALSE,
					orphaned 	= FALSE, lockHeld = FALSE;

	pSendResd		= (PSPX_SEND_RESD)(pNdisPkt->ProtocolReserved);

#if DBG
	if (NdisStatus != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT(SEND, DBG,
				("SpxSendComplete: For %lx with status **%lx**\n",
					pNdisPkt, NdisStatus));
	}
#endif

	//	IPX changes the length set for the first ndis buffer descriptor.
	//	Change it back to its original value here.
	NdisQueryPacket(pNdisPkt, NULL, &bufCount, &pNdisBuffer, NULL);
	NdisAdjustBufferLength(pNdisBuffer, IpxMacHdrNeeded	+ MIN_IPXSPX2_HDRSIZE);

	do
	{
		pSpxConnFile 	= pSendResd->sr_ConnFile;
		CTEGetLock(&pSpxConnFile->scf_Lock, &lockHandle);
		lockHeld = TRUE;

		CTEAssert((pSendResd->sr_State & SPX_SENDPKT_IPXOWNS) != 0);
	
		//	IPX dont own this packet nomore.
		pSendResd->sr_State		&= ~SPX_SENDPKT_IPXOWNS;
	
		//	If a send packet has been aborted, then we need to call
		//	abort send to go ahead and free up this packet, and deref associated
		//	request, if there is one, potentially completing it.
		if ((pSendResd->sr_State & SPX_SENDPKT_ABORT) != 0)
		{
			spxConnAbortSendPkt(
				pSpxConnFile,
				pSendResd,
				SPX_CALL_TDILEVEL,
				lockHandle);

			lockHeld = FALSE;
			break;
		}

		//	If there is an associated request, remove reference on it. BUT for a
		//	sequenced packet only if it has been acked and is waiting for the request
		//	to be dereferenced. It is already dequeued from queue, just free it up.
		if ((((pSendResd->sr_State & SPX_SENDPKT_REQ) != 0) &&
			 ((pSendResd->sr_State & SPX_SENDPKT_SEQ) == 0)) ||
			((pSendResd->sr_State & SPX_SENDPKT_ACKEDPKT) != 0))
		{
			freePkt = (BOOLEAN)((pSendResd->sr_State & SPX_SENDPKT_ACKEDPKT) != 0);

			pRequest		= pSendResd->sr_Request;
			CTEAssert(pRequest != NULL);

			DBGPRINT(SEND, DBG,
					("IpxSendComplete: ReqRef before dec %lx.%lx\n",
						pRequest, REQUEST_INFORMATION(pRequest)));

			//	Deref the request and see if we complete it now. We always have our
			//	own reference on the request.
			//	!!! Status should already have been set in request...!!!
			if (--(REQUEST_INFORMATION(pRequest)) == 0)
			{
				CTEAssert(REQUEST_STATUS(pRequest) != STATUS_PENDING);

				completeReq	= TRUE;

				//	If this is acked already, request is not on list.
				//	BUG #11626
				if ((pSendResd->sr_State & SPX_SENDPKT_ACKEDPKT) == 0)
				{
					RemoveEntryList(REQUEST_LINKAGE(pRequest));
				}
			}
		}
	
		//	Do we destroy this packet?
		if ((pSendResd->sr_State & SPX_SENDPKT_DESTROY) != 0)
		{
			//	Remove this packet from the send list in the connection.
			DBGPRINT(SEND, INFO,
					("IpxSendComplete: destroy packet...\n"));

			SpxConnDequeueSendPktLock(pSpxConnFile, pNdisPkt);
			freePkt = TRUE;	
		}

	} while (FALSE);

	if (lockHeld)
	{
		CTEFreeLock(&pSpxConnFile->scf_Lock, lockHandle);
	}

	if (freePkt)
	{
		DBGPRINT(SEND, INFO,
				("IpxSendComplete: free packet...\n"));

		SpxPktSendRelease(pNdisPkt);
	}

	if (completeReq)
	{
		//	If this is a send request, set info to data sent, else it will be
		//	zero.
		if (REQUEST_MINOR_FUNCTION(pRequest) == TDI_SEND)
		{
			PTDI_REQUEST_KERNEL_SEND	pParam;

			pParam 	= (PTDI_REQUEST_KERNEL_SEND)
						REQUEST_PARAMETERS(pRequest);

			REQUEST_INFORMATION(pRequest) = pParam->SendLength;
			DBGPRINT(SEND, DBG,
					("IpxSendComplete: complete req %lx.%lx...\n",
						REQUEST_STATUS(pRequest),
						REQUEST_INFORMATION(pRequest)));
	
			CTEAssert(pRequest != NULL);
			CTEAssert(REQUEST_STATUS(pRequest) != STATUS_PENDING);
			SpxCompleteRequest(pRequest);
		}
		else
		{
			DBGPRINT(SEND, DBG,
					("SpxSendComplete: %lx DISC Request %lx with %lx.%lx\n",
						pSpxConnFile, pRequest, REQUEST_STATUS(pRequest),
						REQUEST_INFORMATION(pRequest)));

			DBGPRINT(SEND, DBG,
					("SpxSendComplete: %lx.%lx.%lx\n",
						pSpxConnFile->scf_RefCount,
						pSpxConnFile->scf_Flags,
						pSpxConnFile->scf_Flags2));

			//	Set the request in the connection, and deref for it.
			InsertTailList(
				&pSpxConnFile->scf_DiscLinkage,
				REQUEST_LINKAGE(pRequest));
		}

		SpxConnFileDereference(pSpxConnFile, CFREF_VERIFY);
	}

    return;

}   //  SpxSendComplete



