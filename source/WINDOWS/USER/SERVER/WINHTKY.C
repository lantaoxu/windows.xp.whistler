/****************************** Module Header ******************************\
* Module Name: whotkeys.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains the core functions of 3.1 window hotkey processing.
*
* History:
* 04-16-92 JimA         Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * Hot key routines. Hot keys are stored in the gpHotKeyList as follows:
 */
typedef struct
{
    PWND spwnd;
    DWORD key;
} HOTKEYSTRUCT, *PHOTKEYSTRUCT;

PHOTKEYSTRUCT gpHotKeyList = NULL;
int gcHotKey = 0;

/***************************************************************************\
* HotKeyToWindow
*
* Scans the hotkey table and returns the pwnd corresponding to the  
* given hot key.  Returns NULL if no such hot key in the list.  Looks at the 
* current key state array.                                                   
*
* History:
* 04-15-92 JimA         Ported from Win3.1 sources.
\***************************************************************************/

PWND HotKeyToWindow(
    DWORD key)
{
    PHOTKEYSTRUCT phk;
    int ckeys = gcHotKey;
    
    if (ckeys == 0)
        return 0;
    
    phk = gpHotKeyList;
    
    while (ckeys) {
        if (phk->key == key)
            return TestWF(phk->spwnd, WFVISIBLE) ? phk->spwnd : NULL;
        phk++;
        ckeys--;
    }
    
    return 0;
}


/***************************************************************************\
* HotKeyHelper
*
* Scans the hot key list and returns a pointer to the entry for the 
* window.                                                                    
*
* History:
* 04-15-92 JimA         Ported from Win3.1 sources.
\***************************************************************************/

PHOTKEYSTRUCT HotKeyHelper(
    PWND pwnd)
{
    PHOTKEYSTRUCT phk;
    int count = gcHotKey;
    
    if (gpHotKeyList == NULL)
        return 0;
    
    phk = gpHotKeyList;
    
    while (count) {
        if (phk->spwnd == pwnd)
            return phk;
        phk++;
        count--;
    }
    
    return 0;
}


/***************************************************************************\
* DWP_SetHotKey
*
* Set the hot key for this window.  Replace existing hot key, or if new
* key is NULL, delete the entry.  Return 2 if key already existed and
* was replaced, 1 if key did not exist and was set, 0 for
* failure, and -1 for invalid hot key.
*
* History:
* 04-15-92 JimA         Ported from Win3.1 sources.
\***************************************************************************/

UINT DWP_SetHotKey(
    PWND pwnd,
    DWORD dwKey)
{
    PHOTKEYSTRUCT phk;
    BOOL fKeyExists = FALSE;
    PWND pwndTemp;
    
    /*
     * Filter out invalid hotkeys
     */
    if (LOBYTE(dwKey) == VK_ESCAPE || LOBYTE(dwKey) == VK_SPACE ||
            LOBYTE(dwKey) == VK_TAB)
        return (UINT)-1;

    /*
     * Don't allow hotkeys for children
     */
    if (TestWF(pwnd, WFCHILD))
        return 0;

    /*
     * If the list does not exist, create it
     */
    if (gpHotKeyList == NULL) {
        gpHotKeyList = (PHOTKEYSTRUCT)LocalAlloc(LPTR, sizeof(HOTKEYSTRUCT));
        if (gpHotKeyList == NULL)
            return 0;
    }
        
    /*
     * Check if the hot key exists and is assigned to a different pwnd
     */
    if (dwKey != 0) {
        pwndTemp = HotKeyToWindow(dwKey);
        if (pwndTemp != NULL && pwndTemp != pwnd)
            fKeyExists = TRUE;
    }
        
    /*
     * Get the hotkey assigned to the window, if any
     */
    phk = HotKeyHelper(pwnd);

    if (phk == NULL) {

        /*
         * Window doesn't exist in the hotkey list and key is being set
         * to zero, so just return.
         */
        if (dwKey == 0)
            return 1;

        /*
         * Allocate and point to a spot for the new hotkey
         */
        phk = (PHOTKEYSTRUCT)LocalReAlloc((HANDLE)gpHotKeyList,
                sizeof(HOTKEYSTRUCT) * (gcHotKey + 1), LPTR | LMEM_MOVEABLE);
        if (phk != NULL) {
            gpHotKeyList = phk;
            phk += gcHotKey++;
        } else
            return 0;
    }

    if (dwKey == 0) {

        /*
         * The hotkey for this window is being deleted. Copy the last item
         * on the list on top of the one being deleted.
         */
        --gcHotKey;
        Lock(&phk->spwnd, gpHotKeyList[gcHotKey].spwnd);
        Unlock(&gpHotKeyList[gcHotKey].spwnd);
        phk->key = gpHotKeyList[gcHotKey].key;
    } else {

        /*
         * Add the window and key to the list
         */
        Lock(&phk->spwnd, pwnd);
        phk->key = dwKey;
    }        
        
    return fKeyExists ? 2 : 1;
}
        

/***************************************************************************\
* DWP_GetHotKey
*
*
* History:
* 04-15-92 JimA         Created.
\***************************************************************************/

UINT DWP_GetHotKey(
    PWND pwnd)
{
    PHOTKEYSTRUCT phk;

    phk = HotKeyHelper(pwnd);
    if (phk == NULL)
        return 0;

    return phk->key;
}



