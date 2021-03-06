/*++

Copyright (c) 2000 Microsoft Corporation

Module Name:

    dcebind.cxx

Abstract:

    This module contains the code implementing the Binding Object DCE RPC
    runtime APIs.  APIs which are used only by server applications do not
    live here.

Author:

    Michael Montague (mikemon) 25-Sep-2000

Revision History:

--*/

#include <sysinc.h>
#include <rpc.h>
#include <rpcdcep.h>
#include <util.hxx>
#include <rpcuuid.hxx>
#include <binding.hxx>
#include <linklist.hxx>
#include <handle.hxx>
#include <binding.hxx>


RPC_STATUS RPC_ENTRY
RpcBindingCopy (
    IN RPC_BINDING_HANDLE SourceBinding,
    OUT RPC_BINDING_HANDLE PAPI * DestinationBinding
    )
/*++

Routine Description:

    This routine copies binding information and creates a new binding
    handle.

Arguments:

    SourceBinding - Supplies the binding to be duplicated.

    DestinationBinding - Returns a new binding which is a duplicate of
        SourceBinding.

Return Value:

    The status for the operation is returned.

--*/
{
    MESSAGE_OBJECT * Binding;

    InitializeIfNecessary();

    Binding = (MESSAGE_OBJECT *) SourceBinding;
    if (Binding->InvalidHandle(BINDING_HANDLE_TYPE
                | CCONNECTION_TYPE | SCONNECTION_TYPE))
        return(RPC_S_INVALID_BINDING);
    if (Binding->Type() == SCONNECTION_TYPE)
        return(RPC_S_WRONG_KIND_OF_BINDING);

    return(Binding->BindingCopy((BINDING_HANDLE * PAPI *) DestinationBinding,
            0));
}


RPC_STATUS RPC_ENTRY
I_RpcBindingCopy (
    IN RPC_BINDING_HANDLE SourceBinding,
    OUT RPC_BINDING_HANDLE PAPI * DestinationBinding
    )
/*++

Routine Description:

    This routine copies binding information and creates a new binding
    handle.  In addition, context is being maintained by the server over
    this binding handle.

Arguments:

    SourceBinding - Supplies the binding to be duplicated.

    DestinationBinding - Returns a new binding which is a duplicate of
        SourceBinding.

Return Value:

    The status for the operation is returned.

--*/
{
    MESSAGE_OBJECT * Binding;

    InitializeIfNecessary();

    Binding = (MESSAGE_OBJECT *) SourceBinding;
    if (Binding->InvalidHandle(BINDING_HANDLE_TYPE
                | CCONNECTION_TYPE | SCONNECTION_TYPE))
        return(RPC_S_INVALID_BINDING);
    if (Binding->Type() == SCONNECTION_TYPE)
        return(RPC_S_WRONG_KIND_OF_BINDING);

    return(Binding->BindingCopy((BINDING_HANDLE * PAPI *) DestinationBinding,
            1));
}


RPC_STATUS RPC_ENTRY
RpcBindingFree (
    IN OUT RPC_BINDING_HANDLE PAPI * Binding
    )
/*++

Routine Description :

    RpcBindingFree releases binding handle resources.

Arguments:

    Binding - Supplies the binding handle to be freed, and returns zero.

Return Value:

    The status of the operation is returned.

--*/
{
    BINDING_HANDLE * BindingHandle;
    RPC_STATUS Status;

    InitializeIfNecessary();

    ASSERT(!RpcpCheckHeap());

    BindingHandle = (BINDING_HANDLE *) *Binding;
    if (BindingHandle->InvalidHandle(BINDING_HANDLE_TYPE | SCONNECTION_TYPE))
        return(RPC_S_INVALID_BINDING);
    if (BindingHandle->Type() == SCONNECTION_TYPE)
        return(RPC_S_WRONG_KIND_OF_BINDING);
    Status = BindingHandle->BindingFree();
    *Binding = 0;

    return(Status);
}


RPC_STATUS RPC_ENTRY
RpcBindingReset (
    IN RPC_BINDING_HANDLE Binding
    )
/*++

Routine Description:

    This routine sets the endpoint in the supplied binding handle to
    zero.  This makes the binding handle a partiallly bound binding
    handle.  NOTE: this routine will fail if the binding handle has
    already been used to make remote procedure calls.  Based on how
    this routine will be used (to iterate through the entries in the
    endpoint mapper database), this should not be a problem.

Arguments:

    Binding - Supplies the binding handle for which the endpoint will
        be set to zero, hence making it a partially bound binding handle.

Return Value:

    RPC_S_OK - The operation completed successfully.

    RPC_S_INVALID_BINDING - The supplied argument is not a valid binding.

    RPC_S_WRONG_KIND_OF_BINDING - Either the supplied binding is not a
        client binding handle or it is a client binding handle which has
        already been used to make remote procedure calls.

--*/
{
    BINDING_HANDLE * BindingHandle;

    BindingHandle = (BINDING_HANDLE *) Binding;
    if (BindingHandle->InvalidHandle(BINDING_HANDLE_TYPE | SCONNECTION_TYPE))
        return(RPC_S_INVALID_BINDING);
    if (BindingHandle->Type() == SCONNECTION_TYPE)
        return(RPC_S_WRONG_KIND_OF_BINDING);

    return(BindingHandle->BindingReset());
}


RPC_STATUS RPC_ENTRY
RpcBindingFromStringBindingW (
    IN RPC_CHAR PAPI * StringBinding,
    OUT RPC_BINDING_HANDLE PAPI * Binding
    )
/*++

Routine Description:

    This routine returns a binding handle from a string representation
    of a binding handle.

Arguments:

    StringBinding - Supplies the string representation of a binding handle.

    Binding - Returns a binding handle constructed from the string
        representation.

Return Value:

    The status of the operation is returned.

--*/
{
    DCE_BINDING * DceBinding;
    RPC_STATUS Status;
    BINDING_HANDLE * BindingHandle;

    InitializeIfNecessary();

    ASSERT(!RpcpCheckHeap());

    *Binding = 0;

    DceBinding = new DCE_BINDING(StringBinding, &Status);
    if (DceBinding == 0)
        Status = RPC_S_OUT_OF_MEMORY;

    if (Status == RPC_S_OK)
        {
        BindingHandle = DceBinding->CreateBindingHandle(&Status);
        if (Status == RPC_S_OK)
            *Binding = BindingHandle;
        }

    return(Status);
}


RPC_STATUS RPC_ENTRY
RpcBindingSetObject (
    IN RPC_BINDING_HANDLE Binding,
    IN UUID PAPI * ObjectUuid
    )
/*++

Routine Description:

    This routine sets the object UUID value in a binding handle.

Arguments:

    Binding - Supplies the binding handle for which the object UUID is
        to be set.

    ObjectUuid - Supplies the UUID value to put into the binding handle.

Return Values:

    RPC_S_OK - The operation completed successfully.

    RPC_S_INVALID_BINDING - The binding argument does not specify a binding
        handle.

    RPC_S_WRONG_KIND_OF_BINDING - The binding argument does specify a
        binding handle, but it is not a client binding handle (ie. one
        owned by the client side rpc runtime).

--*/
{
    BINDING_HANDLE * BindingHandle;

    InitializeIfNecessary();

    BindingHandle = (BINDING_HANDLE *) Binding;
    if (BindingHandle->InvalidHandle(BINDING_HANDLE_TYPE
            | SCONNECTION_TYPE))
        return(RPC_S_INVALID_BINDING);
    if (BindingHandle->Type() == SCONNECTION_TYPE)
        return(RPC_S_WRONG_KIND_OF_BINDING);

    BindingHandle->SetObjectUuid((RPC_UUID PAPI *) ObjectUuid);

    return(RPC_S_OK);
}


RPC_STATUS RPC_ENTRY
RpcBindingVectorFree (
    IN OUT RPC_BINDING_VECTOR PAPI * PAPI * BindingVector
    )
/*++

Routine Description:

    This routine frees the binding handles contained in the vector, and
    the vector itself.

Arguments:

    BindingVector - Supplies a vector of binding handles which will be
        freed.  On return, the pointer to the binding vector will be
        set to zero.

Return Value:

    The status of the operation will be returned.

--*/
{
    unsigned int Index, Count;
    RPC_BINDING_VECTOR PAPI * Vector;
    RPC_STATUS Status;

    InitializeIfNecessary();

    for (Index = 0, Vector = *BindingVector,
            Count = (unsigned int) Vector->Count;
            Index < Count; Index++)
        if (Vector->BindingH[Index] != 0)
            {
            Status = RpcBindingFree(&(Vector->BindingH[Index]));
            if (Status != RPC_S_OK)
                return(Status);
            }
    RpcpFarFree(*BindingVector);
    *BindingVector = 0;

    return(RPC_S_OK);
}


RPC_STATUS RPC_ENTRY
RpcEpResolveBinding (
    IN RPC_BINDING_HANDLE Binding,
    IN RPC_IF_HANDLE IfSpec
    )
/*++

Routine Description:

    This routine is used to resolve a partially-bound binding handle
    into a fully-bound binding handle.  A partially-bound binding
    handle is one in which the endpoint is not specified.  To make
    the binding handle fully-bound, we need to determine the endpoint.

Arguments:

    Binding - Supplies a partially-bound binding handle to resolve into
        a fully bound one.  Specifying a fully-bound binding handle to
        this routine is not an error; it has no effect on the binding
        handle.

    IfSpec - Supplies a handle to the description of the interface for
        which we wish to resolve the endpoint.  This information will
        be used to find the correct server on the machine specified by
        the network address in the binding handle.

Return Value:

    RPC_S_OK - The binding handle is now fully-bound.

    RPC_S_NO_ENDPOINT_FOUND - We were unable to resolve the endpoint
        for this particular combination of binding handle (network address)
        and interface.

    RPC_S_OUT_OF_MEMORY - Insufficient memory is available to resolve
        the endpoint.

    RPC_S_INVALID_BINDING - The binding argument does not specify a binding
        handle.

    RPC_S_WRONG_KIND_OF_BINDING - The binding argument does specify a
        binding handle, but it is not a client binding handle (ie. one
        owned by the client side rpc runtime).

--*/
{
    BINDING_HANDLE * BindingHandle;

    InitializeIfNecessary();

    BindingHandle = (BINDING_HANDLE *) Binding;
    if (BindingHandle->InvalidHandle(BINDING_HANDLE_TYPE | SCONNECTION_TYPE))
        return(RPC_S_INVALID_BINDING);
    if (BindingHandle->Type() == SCONNECTION_TYPE)
        return(RPC_S_WRONG_KIND_OF_BINDING);

    return(BindingHandle->ResolveBinding((PRPC_CLIENT_INTERFACE) IfSpec));
}


RPC_STATUS RPC_ENTRY
RpcNsBindingInqEntryNameW (
    IN RPC_BINDING_HANDLE Binding,
    IN unsigned long EntryNameSyntax,
    OUT RPC_CHAR PAPI * PAPI * EntryName
    )
/*++

Routine Description:

Arguments:

Return Value:

    RPC_S_OK - The entry name for the binding handle has been successfully
        inquired and returned.

    RPC_S_INVALID_BINDING - The binding argument does not specify a binding
        handle.

    RPC_S_WRONG_KIND_OF_BINDING - The binding argument does specify a
        binding handle, but it is not a client binding handle (ie. one
        owned by the client side rpc runtime).

--*/
{
    BINDING_HANDLE * BindingHandle;

    InitializeIfNecessary();

    BindingHandle = (BINDING_HANDLE *) Binding;
    if (BindingHandle->InvalidHandle(BINDING_HANDLE_TYPE | SCONNECTION_TYPE))
        return(RPC_S_INVALID_BINDING);
    if (BindingHandle->Type() == SCONNECTION_TYPE)
        return(RPC_S_WRONG_KIND_OF_BINDING);

    return(BindingHandle->InquireEntryName(EntryNameSyntax, EntryName));
}


RPC_STATUS RPC_ENTRY
I_RpcNsBindingSetEntryName (
    IN RPC_BINDING_HANDLE Binding,
    IN unsigned long EntryNameSyntax,
    IN RPC_CHAR PAPI * EntryName
    )
/*++

Routine Description:

    This routine is a private entry point for use by name service support
    dlls; it allows them to set the entry name in a binding handle
    before returning it from import or lookup.  If an entry name already
    exists in the binding handle, we just go ahead and overwrite the
    old one with new one.

Arguments:

    Binding - Supplies the binding handle for which we want to set the
        entry name.

    EntryNameSyntax - Supplies the syntax used by the entry name.  We need
        to save this information for when the entry name is inquired.

    EntryName - Supplies the entry name for this binding handle.

Return Value:

    RPC_S_OK - The entry name has been successfully set for the binding
        handle.

    RPC_S_OUT_OF_MEMORY - Insufficient memory is available to complete
        the operation.

    RPC_S_INVALID_BINDING - The binding argument does not specify a binding
        handle.

    RPC_S_WRONG_KIND_OF_BINDING - The binding argument does specify a
        binding handle, but it is not a client binding handle (ie. one
        owned by the client side rpc runtime).

--*/
{
    BINDING_HANDLE * BindingHandle;

    InitializeIfNecessary();

    BindingHandle = (BINDING_HANDLE *) Binding;
    if (BindingHandle->InvalidHandle(BINDING_HANDLE_TYPE | SCONNECTION_TYPE))
        return(RPC_S_INVALID_BINDING);
    if (BindingHandle->Type() == SCONNECTION_TYPE)
        return(RPC_S_WRONG_KIND_OF_BINDING);

    return(BindingHandle->SetEntryName(EntryNameSyntax, EntryName));

}


RPC_STATUS RPC_ENTRY
RpcBindingInqAuthInfoW (
    IN RPC_BINDING_HANDLE Binding,
    OUT RPC_CHAR PAPI * PAPI * ServerPrincName, OPTIONAL
    OUT unsigned long PAPI * AuthnLevel, OPTIONAL
    OUT unsigned long PAPI * AuthnSvc, OPTIONAL
    OUT RPC_AUTH_IDENTITY_HANDLE PAPI * AuthIdentity, OPTIONAL
    OUT unsigned long PAPI * AuthzSvc OPTIONAL
    )
/*++

Routine Description:

    This routine is used to return the authentication and authorization for
    a binding handle.  You should also see RpcBindingSetAuthInfoW.

Arguments:

    Binding - Supplies the binding handle for which we wish to query the
        authentication and authorization information.

    ServerPrincName - Optionally returns the server principal name set for
        the binding handle.

    AuthnLevel - Optionally returns the authentication level set for the
        binding handle.

    AuthnSvc - Optionally returns the authentication service set for the
        binding handle.

    AuthIdentity - Optionally returns a handle to the security context
        being used for authentication and authorization.

    AuthzSvc -  Optionally returns the authorization service set for the
        binding handle.

Return Value:

    RPC_S_OK - We successfully returned the information requested.

    RPC_S_OUT_OF_MEMORY - Insufficient memory is available to complete
        the operation.

    RPC_S_INVALID_BINDING - The value supplied as the binding argument is
        not a valid binding handle.

    RPC_S_WRONG_KIND_OF_BINDING - A binding handle on the client side must
        be specified as the binding argument.

    RPC_S_BINDING_HAS_NO_AUTH - RpcBindingInqAuthInfo has not yet been
        called on the binding handle, so there is not authentication or
        authorization to be returned.

--*/
{
    BINDING_HANDLE * BindingObject;
    CLIENT_AUTH_INFO PAPI * ClientAuthInfo;

    InitializeIfNecessary();

    BindingObject = (BINDING_HANDLE *) Binding;
    if ( BindingObject->InvalidHandle(BINDING_HANDLE_TYPE | SCONNECTION_TYPE) )
        {
        return(RPC_S_INVALID_BINDING);
        }
    if ( BindingObject->Type() == SCONNECTION_TYPE )
        {
        return(RPC_S_WRONG_KIND_OF_BINDING);
        }

    ClientAuthInfo = BindingObject->InquireAuthInformation();
    if ( ClientAuthInfo == 0 )
        {
        return(RPC_S_BINDING_HAS_NO_AUTH);
        }

    if (ARGUMENT_PRESENT(AuthnLevel))
        {
        *AuthnLevel = ClientAuthInfo->AuthenticationLevel;
        }
    if (ARGUMENT_PRESENT(AuthnSvc))
        {
        *AuthnSvc = ClientAuthInfo->AuthenticationService;
        }
    if (ARGUMENT_PRESENT(AuthIdentity))
        {
        *AuthIdentity = ClientAuthInfo->AuthIdentity;
        }
    if (ARGUMENT_PRESENT(AuthzSvc))
        {
        *AuthzSvc = ClientAuthInfo->AuthorizationService;
        }
    if (ARGUMENT_PRESENT(ServerPrincName))
        {
        if ( ClientAuthInfo->ServerPrincipalName == 0 )
            {
            *ServerPrincName = 0;
            }
        else
            {
            *ServerPrincName = DuplicateStringPAPI(
                    ClientAuthInfo->ServerPrincipalName);
            if ( *ServerPrincName == 0 )
                {
                return(RPC_S_OUT_OF_MEMORY);
                }
            }
        }
    return(RPC_S_OK);
}


RPC_STATUS RPC_ENTRY
RpcBindingSetAuthInfoW (
    IN RPC_BINDING_HANDLE Binding,
    IN RPC_CHAR PAPI * ServerPrincName,
    IN unsigned long AuthnLevel,
    IN unsigned long AuthnSvc,
    IN RPC_AUTH_IDENTITY_HANDLE AuthIdentity, OPTIONAL
    IN unsigned long AuthzSvc
    )
/*++

Routine Description:

    A client application will use this routine to specify the authentication
    and authorization information in a binding handle, so that the binding
    handle can be used to make authenticated remote procedure calls.  If
    the this routine is not used, then all remote procedure calls on the
    binding handle will be unauthenticated.

Arguments:

    Binding - Supplies binding handle for which we want to set authentication
        and authorization information.

    ServerPrincName - Supplies the expected principal name of the server
        referenced by the binding handle (that is supplied as the binding
        argument).  This information is necessary for some security services
        to be able to authenticate with the server.

    AuthnLevel - Supplies a value indicating the amount (or level) of
        authentication to be performed on remote procedure calls using
        the binding handle.  If we do not support the requested level,
        we will upgrade to the next highest supported level.

        RPC_C_AUTHN_LEVEL_DEFAULT - Indicates that the default level for
            authentication service being used should be used.

        RPC_C_AUTHN_LEVEL_NONE - Do not perform any authentication.

        RPC_C_AUTHN_LEVEL_CONNECT - Authentication will be performed only
            when the client first talks to the server.

        RPC_C_AUTHN_LEVEL_CALL - For connection based protocols, we will
            use RPC_C_AUTHN_LEVEL_PKT instead; for datagram based protocols,
            authentication will be performed at the beginning of each
            remote procedure call.

        RPC_C_AUTHN_LEVEL_PKT - All data will be authenticated to insure that
            the data it is received from the expected client.

        RPC_C_AUTHN_LEVEL_PKT_INTEGRITY - In addition, to authenticating that
            the data is from the expected client, we will verify that none
            of it has been modified.

        RPC_C_AUTHN_LEVEL_PKT_PRIVACY - Finally, this includes all of the
            support in packet integrity, as well as encrypting all remote
            procedure call data.

    AuthnSvc - Supplies the authentication service to use.

    AuthIdentify - Optionally supplies authentication and authorization
        credentials to use; if this argument is not specified, the security
        context for the current address space will be used.

    AuthzSvc - Supplies the authorization service being used by the
        server.  The client must know this so that the correct authorization
        information can be sent to the server.

Return Value:

    RPC_S_OK - The supplied authentication and authorization information has
        been set in the binding handle.

    RPC_S_OUT_OF_MEMORY - Insufficient memory is available to complete the
        operation.

    RPC_S_INVALID_BINDING - The value supplied as the binding argument is
        not a valid binding handle.

    RPC_S_WRONG_KIND_OF_BINDING - A binding handle on the client side must
        be specified as the binding argument.

    RPC_S_UNKNOWN_AUTHN_SERVICE - The specified authentication service is
        not supported.

    RPC_S_UNKNOWN_AUTHN_LEVEL - The specified authentication level is
        not supported.

    RPC_S_INVALID_AUTH_IDENTITY - The specified security context (supplied
        by the auth identity argument) is invalid.

    RPC_S_UNKNOWN_AUTHZ_SERVICE - The specified authorization service is
        not supported.

--*/
{
    BINDING_HANDLE * BindingObject;

    InitializeIfNecessary();

    BindingObject = (BINDING_HANDLE *) Binding;
    if ( BindingObject->InvalidHandle(BINDING_HANDLE_TYPE | SCONNECTION_TYPE) )
        {
        return(RPC_S_INVALID_BINDING);
        }
    if ( BindingObject->Type() == SCONNECTION_TYPE )
        {
        return(RPC_S_WRONG_KIND_OF_BINDING);
        }

    //
    // For no authentication, bail out now.
    //

    if (AuthnSvc == RPC_C_AUTHN_NONE)
        {
        if ((AuthnLevel != RPC_C_AUTHN_LEVEL_NONE) &&
            (AuthnLevel != RPC_C_AUTHN_LEVEL_DEFAULT))
            {
            return(RPC_S_UNKNOWN_AUTHN_LEVEL);
            }
        return(RPC_S_OK);
        }

    return(BindingObject->SetAuthInformation(ServerPrincName, AuthnLevel,
                    AuthnSvc, AuthIdentity, AuthzSvc));
}


RPC_STATUS RPC_ENTRY
I_RpcBindingInqTransportType(
    IN RPC_BINDING_HANDLE Binding,
    OUT unsigned int __RPC_FAR * Type
    )
/*++

Routine Description:

    Determines what kind of transport this binding handle uses.

Arguments:

    Binding - Supplies the binding handle from which we wish to obtain
        the information.


    Type - Points to the type of binding if the functions succeeds.
           One of:
           TRANSPORT_TYPE_CN
           TRANSPORT_TYPE_DG
           TRANSPORT_TYPE_LPC

Return Value:

    RPC_S_OK - The operation completed successfully.

    RPC_S_INVALID_BINDING - When the argument is not a binding handle.

--*/
{
    BINDING_HANDLE * BindingHandle;

    InitializeIfNecessary();

    BindingHandle = (BINDING_HANDLE *) Binding;
    if (BindingHandle->InvalidHandle(BINDING_HANDLE_TYPE))
        return(RPC_S_INVALID_BINDING);

    return(BindingHandle->InquireTransportType(Type));
}

