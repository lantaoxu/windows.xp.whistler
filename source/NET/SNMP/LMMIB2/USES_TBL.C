//-------------------------- MODULE DESCRIPTION ----------------------------
//
//  uses_tbl.c
//
//  Copyright 2000 Technology Dynamics, Inc.
//
//  All Rights Reserved!!!
//
//      This source code is CONFIDENTIAL and PROPRIETARY to Technology
//      Dynamics. Unauthorized distribution, adaptation or use may be
//      subject to civil and criminal penalties.
//
//  All Rights Reserved!!!
//
//---------------------------------------------------------------------------
//
//  Routines to perform operations on the Workstation Uses table.
//
//  Project:  Implementation of an SNMP Agent for Microsoft's NT Kernel
//
//  $Revision:   1.5  $
//  $Date:   30 Jun 2000 13:34:40  $
//  $Author:   mlk  $
//
//  $Log:   N:/lmmib2/vcs/uses_tbl.c_v  $
//
//     Rev 1.5   30 Jun 2000 13:34:40   mlk
//  Removed some openissue comments
//
//     Rev 1.4   12 Jun 2000 19:19:34   todd
//  Added support to initialize table variable
//
//     Rev 1.3   07 Jun 2000 15:26:34   todd
//  Correct MIB prefixes for tables due to new alert mib
//
//     Rev 1.2   01 Jun 2000 12:35:50   todd
//  Added 'dynamic' field to octet string
//
//     Rev 1.1   22 May 2000 17:38:22   todd
//  Added return codes to _lmget() functions
//
//     Rev 1.0   20 May 2000 15:11:18   mlk
//  Initial revision.
//
//     Rev 1.9   02 May 2000 19:08:08   todd
//  code cleanup
//
//     Rev 1.8   27 Apr 2000 17:36:12   todd
//  Removed function and prototype for MIB_wsuses_set
//
//     Rev 1.7   27 Apr 2000 14:05:48   todd
//  Changed table prefix to be correct
//
//     Rev 1.6   27 Apr 2000 14:21:18   Chip
//  Fixed comment problem on l# 371
//
//     Rev 1.5   27 Apr 2000 12:25:48   todd
//
//     Rev 1.4   27 Apr 2000 12:21:36   todd
//  Added routines to support operations on the workstation uses table
//
//     Rev 1.3   26 Apr 2000 18:02:54   Chip
//  Fixed error in table declaratino and included new uses_tbl.h
//
//     Rev 1.2   25 Apr 2000 17:21:30   todd
//
//     Rev 1.1   24 Apr 2000 14:34:52   todd
//
//     Rev 1.0   24 Apr 2000 13:38:44   todd
//  Initial revision.
//
//---------------------------------------------------------------------------

//--------------------------- VERSION INFO ----------------------------------

static char *vcsid = "@(#) $Logfile:   N:/lmmib2/vcs/uses_tbl.c_v  $ $Revision:   1.5  $";

//--------------------------- WINDOWS DEPENDENCIES --------------------------

//--------------------------- STANDARD DEPENDENCIES -- #include<xxxxx.h> ----

#include <stdio.h>
#include <malloc.h>
#include <memory.h>

//--------------------------- MODULE DEPENDENCIES -- #include"xxxxx.h" ------

#include <snmp.h>

#include "mibfuncs.h"

//--------------------------- SELF-DEPENDENCY -- ONE #include"module.h" -----

#include "uses_tbl.h"

//--------------------------- PUBLIC VARIABLES --(same as in module.h file)--

   // Prefix to the Uses table
static UINT                usesSubids[] = { 3, 8, 1 };
static AsnObjectIdentifier MIB_UsesPrefix = { 3, usesSubids };

WKSTA_USES_TABLE MIB_WkstaUsesTable = { 0, NULL };

//--------------------------- PRIVATE CONSTANTS -----------------------------

#define USES_FIELD_SUBID       (MIB_UsesPrefix.idLength+MIB_OidPrefix.idLength)

#define USES_FIRST_FIELD       USES_LOCAL_FIELD
#define USES_LAST_FIELD        USES_STATUS_FIELD

//--------------------------- PRIVATE STRUCTS -------------------------------

//--------------------------- PRIVATE VARIABLES -----------------------------


//--------------------------- PRIVATE PROTOTYPES ----------------------------

UINT MIB_wsuses_get(
        IN OUT RFC1157VarBind *VarBind
        );

int MIB_wsuses_match(
       IN AsnObjectIdentifier *Oid,
       OUT UINT *Pos,
       IN BOOL Next
       );

UINT MIB_wsuses_copyfromtable(
        IN UINT Entry,
        IN UINT Field,
        OUT RFC1157VarBind *VarBind
        );

//--------------------------- PRIVATE PROCEDURES ----------------------------

//--------------------------- PUBLIC PROCEDURES -----------------------------

//
// MIB_wsuses_func
//    High level routine for handling operations on the uses table
//
// Notes:
//
// Return Codes:
//    None.
//
// Error Codes:
//    None.
//
UINT MIB_wsuses_func(
        IN UINT Action,
        IN MIB_ENTRY *MibPtr,
        IN OUT RFC1157VarBind *VarBind
        )

{
int     Found;
UINT    Entry;
UINT    Field;
UINT    ErrStat;


   switch ( Action )
      {
      case MIB_ACTION_GETFIRST:
         // Fill the Uses table with the info from server
         if ( SNMPAPI_ERROR == MIB_wsuses_lmget() )
            {
            ErrStat = SNMP_ERRORSTATUS_GENERR;
            goto Exit;
            }

         // If no elements in table, then return next MIB var, if one
         if ( MIB_WkstaUsesTable.Len == 0 )
            {
            if ( MibPtr->MibNext == NULL )
               {
               ErrStat = SNMP_ERRORSTATUS_NOSUCHNAME;
               goto Exit;
               }

            // Do get first on the next MIB var
            ErrStat = (*MibPtr->MibNext->MibFunc)( Action, MibPtr->MibNext,
                                                   VarBind );
            break;
            }

         //
         // Place correct OID in VarBind
         // Assuming the first field in the first record is the "start"
         {
         UINT temp_subs[] = { USES_FIRST_FIELD };
         AsnObjectIdentifier FieldOid = { 1, temp_subs };


         SNMP_oidfree( &VarBind->name );
         SNMP_oidcpy( &VarBind->name, &MIB_OidPrefix );
         SNMP_oidappend( &VarBind->name, &MIB_UsesPrefix );
         SNMP_oidappend( &VarBind->name, &FieldOid );
         SNMP_oidappend( &VarBind->name, &MIB_WkstaUsesTable.Table[0].Oid );
         }

         //
         // Let fall through on purpose
         //

      case MIB_ACTION_GET:
         ErrStat = MIB_wsuses_get( VarBind );
         break;

      case MIB_ACTION_GETNEXT:
         // Fill the Uses table with the info from server
         if ( SNMPAPI_ERROR == MIB_wsuses_lmget() )
            {
            ErrStat = SNMP_ERRORSTATUS_GENERR;
            goto Exit;
            }

         // Lookup OID in table
         Found = MIB_wsuses_match( &VarBind->name, &Entry, TRUE );

         // Determine which field
         Field = VarBind->name.ids[USES_FIELD_SUBID];

         // Index not found, but could be more fields to base GET on
         if ( Found == MIB_TBL_POS_END )
            {
            // Index not found in table, get next from field
            Field ++;

            // Make sure not past last field
            if ( Field > USES_LAST_FIELD )
               {
               // Get next VAR in MIB
               ErrStat = (*MibPtr->MibNext->MibFunc)( MIB_ACTION_GETFIRST,
                                                      MibPtr->MibNext,
                                                      VarBind );
               break;
               }
            }

         // Get next TABLE entry
         if ( Found == MIB_TBL_POS_FOUND )
            {
            Entry ++;
            if ( Entry > MIB_WkstaUsesTable.Len-1 )
               {
               Entry = 0;
               Field ++;
               if ( Field > USES_LAST_FIELD )
                  {
                  // Get next VAR in MIB
                  ErrStat = (*MibPtr->MibNext->MibFunc)( MIB_ACTION_GETFIRST,
                                                         MibPtr->MibNext,
                                                         VarBind );
                  break;
                  }
               }
            }

         //
         // Place correct OID in VarBind
         // Assuming the first field in the first record is the "start"
         {
         UINT temp_subs[1];
         AsnObjectIdentifier FieldOid;

         temp_subs[0]      = Field;
         FieldOid.idLength = 1;
         FieldOid.ids      = temp_subs;

         SNMP_oidfree( &VarBind->name );
         SNMP_oidcpy( &VarBind->name, &MIB_OidPrefix );
         SNMP_oidappend( &VarBind->name, &MIB_UsesPrefix );
         SNMP_oidappend( &VarBind->name, &FieldOid );
         SNMP_oidappend( &VarBind->name, &MIB_WkstaUsesTable.Table[Entry].Oid );
         }

         ErrStat = MIB_wsuses_copyfromtable( Entry, Field, VarBind );

         break;

      case MIB_ACTION_SET:
         ErrStat = SNMP_ERRORSTATUS_NOSUCHNAME;
         break;

      default:
         ErrStat = SNMP_ERRORSTATUS_GENERR;
      }

Exit:
   return ErrStat;
} // MIB_wsuses_func



//
// MIB_wsuses_get
//    Retrieve Uses table information.
//
// Notes:
//
// Return Codes:
//    None.
//
// Error Codes:
//    None.
//
UINT MIB_wsuses_get(
        IN OUT RFC1157VarBind *VarBind
        )

{
UINT   Entry;
int    Found;
UINT   ErrStat;


   // Fill the Uses table with the info from server
   if ( SNMPAPI_ERROR == MIB_wsuses_lmget() )
      {
      ErrStat = SNMP_ERRORSTATUS_GENERR;
      goto Exit;
      }

   Found = MIB_wsuses_match( &VarBind->name, &Entry, FALSE );

   // Look for a complete OID match
   if ( Found != MIB_TBL_POS_FOUND )
      {
      ErrStat = SNMP_ERRORSTATUS_NOSUCHNAME;
      goto Exit;
      }

   // Copy data from table
   ErrStat = MIB_wsuses_copyfromtable( Entry, VarBind->name.ids[USES_FIELD_SUBID],
                                     VarBind );

Exit:
   return ErrStat;
} // MIB_wsuses_get



//
// MIB_wsuses_match
//    Match the target OID with a location in the Uses table
//
// Notes:
//
// Return Codes:
//    None.
//
// Error Codes:
//    None
//
int MIB_wsuses_match(
       IN AsnObjectIdentifier *Oid,
       OUT UINT *Pos,
       IN BOOL Next
       )

{
AsnObjectIdentifier TempOid;
int                 nResult;

   // Remove prefix including field reference
   TempOid.idLength = Oid->idLength - MIB_OidPrefix.idLength -
                      MIB_UsesPrefix.idLength - 1;
   TempOid.ids = &Oid->ids[MIB_OidPrefix.idLength+MIB_UsesPrefix.idLength+1];

   *Pos = 0;
   while ( *Pos < MIB_WkstaUsesTable.Len )
      {
      nResult = SNMP_oidcmp( &TempOid, &MIB_WkstaUsesTable.Table[*Pos].Oid );
      if ( !nResult )
         {
         nResult = MIB_TBL_POS_FOUND;
         if (Next) {
             while ( ( (*Pos) + 1 < MIB_WkstaUsesTable.Len ) &&
                     !SNMP_oidcmp( &TempOid, &MIB_WkstaUsesTable.Table[(*Pos)+1].Oid)) {
                 (*Pos)++;
             }
         }

         goto Exit;
         }

      if ( nResult < 0 )
         {
         nResult = MIB_TBL_POS_BEFORE;

         goto Exit;
         }

      (*Pos)++;
      }

   nResult = MIB_TBL_POS_END;

Exit:
   return nResult;
} // MIB_wsuses_match



//
// MIB_wsuses_copyfromtable
//    Copy requested data from table structure into Var Bind.
//
// Notes:
//
// Return Codes:
//    None.
//
// Error Codes:
//    None.
//
UINT MIB_wsuses_copyfromtable(
        IN UINT Entry,
        IN UINT Field,
        OUT RFC1157VarBind *VarBind
        )

{
UINT ErrStat;


   // Get the requested field and save in var bind
   switch( Field )
      {
      case USES_LOCAL_FIELD:
         // Alloc space for string
         VarBind->value.asnValue.string.stream = malloc( sizeof(char)
                       * MIB_WkstaUsesTable.Table[Entry].useLocalName.length );
         if ( VarBind->value.asnValue.string.stream == NULL )
            {
            ErrStat = SNMP_ERRORSTATUS_GENERR;
            goto Exit;
            }

         // Copy string into return position
         memcpy( VarBind->value.asnValue.string.stream,
                       MIB_WkstaUsesTable.Table[Entry].useLocalName.stream,
                       MIB_WkstaUsesTable.Table[Entry].useLocalName.length );

         // Set string length
         VarBind->value.asnValue.string.length =
                          MIB_WkstaUsesTable.Table[Entry].useLocalName.length;
         VarBind->value.asnValue.string.dynamic = TRUE;

         // Set type of var bind
         VarBind->value.asnType = ASN_RFC1213_DISPSTRING;
         break;

      case USES_REMOTE_FIELD:
         // Alloc space for string
         VarBind->value.asnValue.string.stream = malloc( sizeof(char)
                       * MIB_WkstaUsesTable.Table[Entry].useRemote.length );
         if ( VarBind->value.asnValue.string.stream == NULL )
            {
            ErrStat = SNMP_ERRORSTATUS_GENERR;
            goto Exit;
            }

         // Copy string into return position
         memcpy( VarBind->value.asnValue.string.stream,
                       MIB_WkstaUsesTable.Table[Entry].useRemote.stream,
                       MIB_WkstaUsesTable.Table[Entry].useRemote.length );

         // Set string length
         VarBind->value.asnValue.string.length =
                          MIB_WkstaUsesTable.Table[Entry].useRemote.length;
         VarBind->value.asnValue.string.dynamic = TRUE;

         // Set type of var bind
         VarBind->value.asnType = ASN_RFC1213_DISPSTRING;
         break;

      case USES_STATUS_FIELD:
         VarBind->value.asnValue.number =
                               MIB_WkstaUsesTable.Table[Entry].useStatus;
         VarBind->value.asnType = ASN_INTEGER;
         break;

      default:
         printf( "Internal Error WorkstationUses Table\n" );
         ErrStat = SNMP_ERRORSTATUS_GENERR;

         goto Exit;
      }

   ErrStat = SNMP_ERRORSTATUS_NOERROR;

Exit:
   return ErrStat;
} // MIB_wsuses_copyfromtable

//-------------------------------- END --------------------------------------

