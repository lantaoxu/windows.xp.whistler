/******************************* MODULE HEADER ******************************
 * dxval.c
 *      Validate a DRIVEREXTRA structure.  Basically means determining
 *      that the data is consistent with the model information.
 *
 * Copyright (C) 2000  Microsoft Corporation
 *
 ****************************************************************************/


#include        <stddef.h>
#include        <windows.h>

#include        <winres.h>
#include        <libproto.h>

#include        <win30def.h>
#include        <udmindrv.h>
#include        <udresrc.h>
#include        <memory.h>

#include        <udproto.h>





/******************************** Function Header ****************************
 * bValidateDX
 *      Given the model index, verify that the model index in DRIVEREXTRA
 *      is the same,  and that all other fields are consistent with this
 *      model.  Returns FALSE for any failure.  OF COURSE,  THIS IS NO
 *      GUARANTEE THAT THE DATA IS CORRECT FOR THIS PRINTER.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE meaning that data is consistent.
 *
 * HISTORY:
 *  13:33 on Thu 26 Mar 2000    -by-    Lindsay Harris   [lindsayh]
 *      First version, needed now that registry is becoming operational.
 *
 *****************************************************************************/

BOOL
bValidateDX( pdx, pDH, iModel )
DRIVEREXTRA   *pdx;             /* Area to be filled int */
DATAHDR       *pDH;             /* The GPC data */
int            iModel;          /* The model number index */
{

    int     iI;                 /* Loop index */

    short  *ps;                 /* Pounding through the heap */

    MODELDATA  *pMD;            /* The MODELDATA structure contains defaults */


    if( pdx->sVer != DXF_VER || pdx->rgindex[ HE_MODELDATA ] != (short)iModel )
        return   FALSE;

    /*
     *   Set the model specific stuff in the extension part of the
     * devmode structure.  This is based on information contained in the
     * MODELDATA structure.
     */

    if( !(pMD = GetTableInfoIndex( pDH, HE_MODELDATA, iModel )) )
        return   FALSE;

    /*
     *    Verify that the rgindex data is valid.  Simply loop througn
     *  the array checking each entry against the count of them in
     *  the MODELDATA information.
     */

    for( iI = 0; iI < (int)pDH->sMaxHE; ++iI )
    {

         /*
          *    If sCount is 0, there is no corresponding data, so we
          *  ignore the EXTDEVMODE data for that entry.  An index value
          *  of -1 means that there is no corresponding default value,
          *  so we ignore those fields too.
          */

         if( pDH->rghe[ iI ].sCount > 0 &&
             (pdx->rgindex[ iI ] < -1 ||
              pdx->rgindex[ iI ] >= pDH->rghe[ iI ].sCount) )
         {
             return   FALSE;
         }
    }

    /*
     *   Onto the miscellaneous values.
     */
    


    if( pdx->dmNumCarts < 0 || pdx->dmNumCarts > pMD->sCartSlots )
        return   FALSE;

    /*
     *    Also verify that the cartridge indices are within range.
     */
    
    if( pDH->rghe[ HE_FONTCART ].sCount > 0 )
    {
        for( iI = 0; iI < pdx->dmNumCarts; ++iI )
        {
            short   sVal;

            /*
             *  NOTE:  the + 1 following is to adjust the values.  The
             *  values stored in the EXTDEVMODE are indices into the
             *  font cartridge structures in the GPC data.  However,
             *  the array data referenced by the MODELDATA structure
             *  is 1 based,  using 0 as the terminating value.  Hence,
             *  it is easier to add 1 here rather than subtract one
             *  for every loop iteration.
             */

            sVal = pdx->rgFontCarts[ iI ] + 1;
    
            ps = (short *)((BYTE *)pDH + pDH->loHeap + pMD->rgoi[ MD_OI_FONTCART ]);
    
            while( *ps && *ps != sVal )
                    ++ps;
    
            if( *ps == 0 )
            {
                return  FALSE;          /* Didn't find this index */
            }
        }
    }

    return  TRUE;
}
