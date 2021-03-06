/****************************** Module Header ******************************\
* Module Name: wowexec.c
*
* Copyright (c) 2000, Microsoft Corporation
*
* WOWEXEC - 16 Bit Server Task - Does Exec Calls on Behalf of 32 bit CreateProcess
*
*
* History:
* 05-21-91 MattFe       Ported to Windows
* mar-20-92 MattFe      Added Error Message Boxes (from win 3.1 progman)
* apr-1-92 mattfe       added commandline exec and switch to path (from win 3.1 progman)
* jun-1-92 mattfe       changed wowgetnextvdmcommand
* 12-Nov-93 DaveHart    Multiple WOW support and remove captive
*                       GetNextVDMCommand thread from WOW32.
* 16-Nov-93 DaveHart    Reduce data segment size.
\***************************************************************************/
#include "wowexec.h"
#include "wowinfo.h"
#include "shellapi.h"
#ifndef PULONG
#define PULONG
#endif
#include "vint.h"
#include "dde.h"


/*
 * External Prototypes
 */
extern WORD FAR PASCAL WOWQueryDebug( void );
extern DWORD FAR PASCAL ExitWindowsExecContinue( void );
extern WORD FAR PASCAL WowWaitForMsgAndEvent( HWND);
extern void FAR PASCAL WowMsgBox(LPSTR szMsg, LPSTR szTitle, DWORD dwOptionalStyle);

/*
 * Global Variables
 */
HANDLE hAppInstance;
HWND ghwndMain = NULL;
HWND ghwndEdit = NULL;
char    szOOMExitTitle[32+1];
char    szOOMExitMsg[64+1];
char    szAppTitleBuffer[32];
LPSTR   lpszAppTitle = szAppTitleBuffer;
char    szWindowsDirectory[MAXITEMPATHLEN+1];
char    szOriginalDirectory[MAXITEMPATHLEN+1];
BOOL    gfSharedWOW = FALSE;
WORD    gwFirstCmdShow;



/*
 * Forward declarations.
 */
BOOL InitializeApp(LPSTR lpszCommandLine);
LONG FAR PASCAL WndProc(HWND hwnd, WORD message, WORD wParam, LONG lParam);
WORD NEAR PASCAL ExecProgram(LPSTR lpszPath, PWOWINFO pWowInfo);
BOOL NEAR PASCAL ExecApplication(LPSTR szCommand, PWOWINFO pWowInfo);
void NEAR PASCAL MyMessageBox(WORD idTitle, WORD idMessage, LPSTR psz);
PSTR FAR PASCAL GetFilenameFromPath( PSTR szPath );
void NEAR PASCAL GetPathInfo ( PSTR szPath, PSTR *pszFileName, PSTR *pszExt, WORD *pich, BOOL *pfUnc);
BOOL NEAR PASCAL StartRequestedApp(VOID);

#ifndef DBCS
#define AnsiNext(x) ((x)+1)
#define AnsiPrev(y,x) ((x)-1)
#define IsDBCSLeadByte(x) (FALSE)
#endif

typedef struct PARAMETERBLOCK {
    WORD    wEnvSeg;
    LPSTR   lpCmdLine;
    LPVOID  lpCmdShow;
    DWORD   dwReserved;
} PARAMETERBLOCK, *PPARAMETERBLOCK;

typedef struct CMDSHOW {
    WORD    two;
    WORD    nCmdShow;
} CMDSHOW, *PCMDSHOW;

#define CCHMAX 256+8

#define ERROR_ERROR         0
#define ERROR_FILENOTFOUND  2
#define ERROR_PATHNOTFOUND  3
#define ERROR_MANYOPEN      4
#define ERROR_DYNLINKSHARE  5
#define ERROR_LIBTASKDATA   6
#define ERROR_MEMORY        8
#define ERROR_VERSION       10
#define ERROR_BADEXE        11
#define ERROR_OTHEREXE      12
#define ERROR_DOS4EXE       13
#define ERROR_UNKNOWNEXE    14
#define ERROR_RMEXE         15
#define ERROR_MULTDATAINST  16
#define ERROR_PMODEONLY     18
#define ERROR_COMPRESSED    19
#define ERROR_DYNLINKBAD    20
#define ERROR_WIN32         21


/* FindPrevInstanceProc -
 * A silly little enumproc to find any window (EnumWindows) which has a
 * matching EXE file path.  The desired match EXE pathname is pointed to
 * by the lParam.  The found window's handle is stored in the
 * first word of this buffer.
 */

BOOL CALLBACK FindPrevInstanceProc(HWND hWnd, LPSTR lpszParam)
{
    char szT[260];
    HANDLE hInstance;

    // Filter out invisible and disabled windows
    //

    if (!IsWindowEnabled(hWnd) || !IsWindowVisible(hWnd))
        return TRUE;

    hInstance = GetWindowWord(hWnd, GWW_HINSTANCE);
    GetModuleFileName(hInstance, szT, sizeof (szT)-1);

    // Make sure that the hWnd belongs to the current VDM process
    //
    // GetWindowTask returns the wowexec htask16 if the window belongs
    // to a different process - thus we filter out windows in
    // 'separate VDM' processes.
    //                                                     - nanduri

    if (lstrcmpi(szT, lpszParam) == 0 &&
        GetWindowTask(hWnd) != GetWindowTask(ghwndMain)) {
        *(LPHANDLE)lpszParam = hWnd;
        return FALSE;
    }
    else {
        return TRUE;
    }
}

HWND near pascal FindPopupFromExe(LPSTR lpExe)
{
    HWND hwnd = (HWND)0;
    BOOL b;

    b = EnumWindows(FindPrevInstanceProc, (LONG)(LPSTR)lpExe);
    if (!b && (hwnd = *(LPHANDLE)(LPSTR)lpExe))  {
        // Find a "main" window that is the ancestor of a given window
        //

        HWND hwndT;

        // First go up the parent chain to find the popup window.  Then go
        // up the owner chain to find the main window
        //

        while (hwndT = GetParent(hwnd))
             hwnd = hwndT;

        while (hwndT = GetWindow(hwnd, GW_OWNER))
             hwnd = hwndT;
    }

    return hwnd;
}

WORD ActivatePrevInstance(LPSTR lpszPath)
{
    HWND hwnd;
    HINSTANCE ret = IDS_MULTIPLEDSMSG;

    if (hwnd = FindPopupFromExe(lpszPath)) {
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd,SW_SHOWNORMAL);
        }
        else {
            HWND hwndT = GetLastActivePopup(hwnd);
            BringWindowToTop(hwnd);
            if (hwndT && hwnd != hwndT)
                BringWindowToTop(hwndT);
        }
        ret = 0;
    }

    return (ret);
}

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ExecProgram() -                                                         */
/*                                                                          */
/* Taken from Win 3.1 Progman -maf                                          */
/*--------------------------------------------------------------------------*/

/* Returns 0 for success.  Otherwise returns a IDS_ string code. */

WORD NEAR PASCAL ExecProgram(LPSTR lpszPath, PWOWINFO pWowInfo)
{
  WORD    ret;
  LPSTR   lpSI,lpDI;
  PARAMETERBLOCK ParmBlock;
  CMDSHOW CmdShow;
  char    module[CCHMAX];
  char    cmdline[CCHMAX];
  BOOL    bDotFound;

  ret = 0;

  // Don't mess with the mouse state; unless we're on a mouseless system.
  if (!GetSystemMetrics(SM_MOUSEPRESENT))
      ShowCursor(TRUE);

  // If this is the Guy Started with WOW then just use winexec, since there is no environment

  if (pWowInfo == NULL) {
    ret = WinExec(lpszPath, gwFirstCmdShow);
  } else {

  // We have a WOWINFO structure, then use it to pass the correct environment

    lpDI = module;
    bDotFound = FALSE;

    // Find Module name

    for ( lpSI = pWowInfo->lpCmdLine ; (*lpSI != NULL) && (*lpSI != ' ') ; lpSI++) {

        // See if file name has extension already by looking for final .

        if (*lpSI == '.') {
            bDotFound = TRUE;
        }

        // If . is found in path name then its not found in the app name

        if ((*lpSI == '\\') || (*lpSI == '/')) {
            bDotFound = FALSE;
        }

        *lpDI = *lpSI;
        lpDI++;
    }

    *lpDI = NULL;
    lpDI = module;

    if (!bDotFound) {
        lpDI = lstrcat(lpDI,".EXE");
    }

    cmdline[0] = lstrlen(lpSI);
    lstrcpy( &cmdline[1], lpSI );
    lstrcat( &cmdline[1], "\x0D" );

    ParmBlock.wEnvSeg = HIWORD(pWowInfo->lpEnv);
    ParmBlock.lpCmdLine = (LPSTR)cmdline;
    ParmBlock.lpCmdShow = &CmdShow;
    CmdShow.two = 2;
    CmdShow.nCmdShow = pWowInfo->wShowWindow;

    ParmBlock.dwReserved = NULL;

    ret = LoadModule(lpDI,(LPVOID)&ParmBlock) ;
  }

  switch (ret)
    {
      case ERROR_ERROR:
      case ERROR_MEMORY:
          ret = IDS_NOMEMORYMSG;
          break;

      case ERROR_FILENOTFOUND:
          ret = IDS_FILENOTFOUNDMSG;
          break;

      case ERROR_PATHNOTFOUND:
          ret = IDS_BADPATHMSG;
          break;

      case ERROR_MANYOPEN:
          ret = IDS_MANYOPENFILESMSG;
          break;

      case ERROR_DYNLINKSHARE:
          ret = IDS_ACCESSDENIED;
          break;

      case ERROR_VERSION:
          ret = IDS_NEWWINDOWSMSG;
          break;

      case ERROR_RMEXE:
          /* KERNEL has already put up a messagebox for this one. */
          ret = 0;
          break;

      case ERROR_MULTDATAINST:
          ret = ActivatePrevInstance(lpszPath);
          break;

      case ERROR_COMPRESSED:
          ret = IDS_COMPRESSEDEXE;
          break;

      case ERROR_DYNLINKBAD:
          ret = IDS_INVALIDDLL;
          break;

      case SE_ERR_SHARE:
          ret = IDS_SHAREERROR;
          break;

      //
      // We shouldn't get any of the following errors,
      // so the strings have been removed from the resource
      // file.  That's why there's the OutputDebugString
      // on checked builds only.
      //

#ifdef DEBUG
      case ERROR_OTHEREXE:
      case ERROR_PMODEONLY:
      case SE_ERR_ASSOCINCOMPLETE:
      case SE_ERR_DDETIMEOUT:
      case SE_ERR_DDEFAIL:
      case SE_ERR_DDEBUSY:
      case SE_ERR_NOASSOC:
          {
              char szTmp[64];
              wsprintf(szTmp, "WOWEXEC: Unexpected error %d executing app, fix that code!\n", (int)ret);
              OutputDebugString(szTmp);
          }
          //
          // fall through to default case, so the execution
          // is the same as on the free build.
          //
#endif

      default:
          if (ret < 32)
              goto EPExit;
          ret = 0;
    }

EPExit:

  if (!GetSystemMetrics(SM_MOUSEPRESENT)) {
      /*
       * We want to turn the mouse off here on mouseless systems, but
       * the mouse will already have been turned off by USER if the
       * app has GP'd so make sure everything's kosher.
       */
      if (ShowCursor(FALSE) != -1)
          ShowCursor(TRUE);
  }

  return(ret);
}

/***************************************************************************\
* ExecApplication
*
* Code Taken From Win 3.1 ExecItem()
*
\***************************************************************************/

BOOL NEAR PASCAL ExecApplication(LPSTR szCommand, PWOWINFO pWowInfo)
{

    WORD    ret;
    LPSTR   szEnv;
    LPSTR   szEnd;
    BYTE    bDrive;
    WORD    wSegEnvSave;
    HANDLE  hTask;
    LPSTR   lpTask;
    HANDLE  hPDB;
    LPSTR   lpPDB;
    HANDLE  hNewEnv;

    // Seup the environment if we have a WOWINFO record from getvdmcommand

    if (pWowInfo != NULL) {
        int     nLength;
        int     nNewEnvLength;
        LPSTR   lpstrEnv;
        LPSTR   lpstr;
        LPSTR   lpOriginalEnv;

        // Figure out who we are (so we can edit our PDB/PSP)

        hTask = GetCurrentTask();

        lpTask = GlobalLock( hTask );
        if ( lpTask == NULL ) {
            ret = IDS_NOMEMORYMSG;
            goto punt;
        }

#define TDB_PDB_OFFSET  0x60

        hPDB = *((LPWORD)(lpTask + TDB_PDB_OFFSET));

        lpPDB = GlobalLock( hPDB );

        // Save our environment block

#define PDB_ENV_OFFSET  0x2C

        wSegEnvSave = *((LPWORD)(lpPDB + PDB_ENV_OFFSET));

        // Now determine the length of the original exe name

        lpOriginalEnv = (LPSTR)MAKELONG(0,wSegEnvSave);

        do {
            nLength = lstrlen(lpOriginalEnv);
            lpOriginalEnv += nLength + 1;
        } while ( nLength != 0 );

        lpOriginalEnv += 2;         // Skip over magic word, see comment below

        nNewEnvLength = 4 + lstrlen(lpOriginalEnv); // See magic comments below!

        // WOW Apps cannot deal with an invalid TEMP=c:\bugusdir directory
        // Usually on Win 3.1 the TEMP= is checked in ldboot.asm check_temp
        // routine.   However on NT since we get a new environment with each
        // WOW app it means that we have to check it here.   If it is not
        // valid then it is edited in the environment.
        //      - mattfe june 11 93


        szEnv = pWowInfo->lpEnv;
        szEnd = szEnv + pWowInfo->EnvSize;
        szEnd--;

        while ( szEnv < szEnd ) {

           nLength = lstrlen(szEnv) + 1;

           if (  (*szEnv == 'T') &&
             (*(szEnv+1) == 'E') &&
             (*(szEnv+2) == 'M') &&
             (*(szEnv+3) == 'P') &&
             (*(szEnv+4) == '=') )  {

                // Try to set the current directory to the TEMP= dir
                // If it fails then nuke the TEMP= part of the environment
                // in the same way check_TEMP does in ldboot.asm

                if (SetCurrentDirectory(szEnv+5) ) {
                    while (*szEnv != 0) {
                        *szEnv = 'x';
                        szEnv++;
                    }
                }
           break;
           }
           szEnv += nLength;
        }

        // WOW Apps only have a Single Current Directory
        // Find =d:=D:\path where d is the active drive letter
        // Note that the drive info doesn have to be at the start
        // of the environment.

        bDrive = pWowInfo->CurDrive + 'A';
        szEnv = pWowInfo->lpEnv;
        szEnd = szEnv + pWowInfo->EnvSize;
        szEnd--;

        while ( szEnv < szEnd ) {

           nLength = lstrlen(szEnv) + 1;
           if ( *szEnv == '=' ) {
                if ( (bDrive == (*(szEnv+1) & 0xdf)) &&
                     (*(szEnv+2) == ':') && (*(szEnv+3) == '=') ) {
                    SetCurrentDirectory(szEnv+4);
                }
           } else {
                nNewEnvLength += nLength;
           }
           szEnv += nLength;
        }

        // Now allocate and make a personal copy of the Env

        hNewEnv = GlobalAlloc( GMEM_MOVEABLE, (DWORD)nNewEnvLength );
        if ( hNewEnv == NULL ) {
            ret = IDS_NOMEMORYMSG;
            goto punt;
        }
        lpstrEnv = GlobalLock( hNewEnv );
        if ( lpstrEnv == NULL ) {
            GlobalFree( hNewEnv );
            ret = IDS_NOMEMORYMSG;
            goto punt;
        }

        // Copy only the non-current directory env variables

        szEnv = pWowInfo->lpEnv;
        lpstr = lpstrEnv;

        while ( szEnv < szEnd ) {
            nLength = lstrlen(szEnv) + 1;

            // Copy everything except the drive letters

            if ( *szEnv != '=' ) {
                lstrcpy( lpstr, szEnv );
                lpstr += nLength;
            }
            szEnv += nLength;
        }
        *lpstr++ = '\0';          // Extra '\0' on the end

        // Magic environment goop!
        //
        // Windows only supports the passing of environment information
        // using the LoadModule API.  The WinExec API just causes
        // the application to inherit the current DOS PDB's environment.
        //
        // Also, the environment block has a little gotcha at the end.  Just
        // after the double-nuls there is a magic WORD value 0x0001 (DOS 3.0
        // and later).  After the value is a nul terminated string indicating
        // the applications executable file name (including PATH).
        //
        // We copy the value from WOWEXEC's original environment because
        // that is what WinExec appears to do.
        //
        // -BobDay

        *lpstr++ = '\1';
        *lpstr++ = '\0';        // Magic 0x0001 from DOS

        lstrcpy( lpstr, lpOriginalEnv );    // More Magic (see comment above)

        // Temporarily update our environment block

        *((LPWORD)(lpPDB + PDB_ENV_OFFSET)) = (WORD)hNewEnv | 1;

        pWowInfo->lpEnv = lpstrEnv;

    }

    //
    // Set our current drive & directory as requested.
    //

    if (pWowInfo) {
        SetCurrentDirectory(pWowInfo->lpCurDir);
    }

    ret = ExecProgram(szCommand, pWowInfo);

    if ( pWowInfo != NULL ) {

        // Restore our environment block

        *((LPWORD)(lpPDB + PDB_ENV_OFFSET)) = wSegEnvSave;

        GlobalUnlock( hPDB );
        GlobalUnlock( hTask );
        GlobalUnlock( hNewEnv );
        GlobalFree( hNewEnv );

    }

punt:

    // Change back to the Windows Directory
    // So that if we are execing from a NET Drive its
    // Not kept Active

    SetCurrentDirectory(szWindowsDirectory);

    // Check for errors.
    if (ret)
      {
        MyMessageBox(IDS_EXECERRTITLE, ret, szCommand);
      }

    //  Always call this when we are done try to start an app.
    //  It will do nothing if we were successful in starting an
    //  app, otherwise if we were unsucessful it will signal that
    //  the app has completed.
    WowFailedExec();

    return(ret);
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  MyMessageBox() -                                                        */
/*  Taken From Win 3.1 Progman - maf                                        */
/*--------------------------------------------------------------------------*/

void NEAR PASCAL MyMessageBox(WORD idTitle, WORD idMessage, LPSTR psz)
{
  char szTitle[MAXTITLELEN+1];
  char szMessage[MAXMESSAGELEN+1];
  char szTempField[MAXMESSAGELEN+1];


  if (!LoadString(hAppInstance, idTitle, szTitle, sizeof(szTitle)))
      goto MessageBoxOOM;

  if (idMessage > IDS_LAST)
    {
      if (!LoadString(hAppInstance, IDS_UNKNOWNMSG, szTempField, sizeof(szTempField)))
          goto MessageBoxOOM;
      wsprintf(szMessage, szTempField, idMessage);
    }
  else
    {
      if (!LoadString(hAppInstance, idMessage, szTempField, sizeof(szTempField)))
          goto MessageBoxOOM;

      if (psz)
          wsprintf(szMessage, szTempField, (LPSTR)psz);
      else
          lstrcpy(szMessage, szTempField);
    }

  WowMsgBox(szMessage, szTitle, MB_ICONEXCLAMATION);
  return;


MessageBoxOOM:
  WowMsgBox(szOOMExitMsg, szOOMExitTitle, MB_ICONHAND);

  return ;
}



/***************************************************************************\
* main
*
*
* History:
* 04-13-91 ScottLu      Created - from 32 bit exec app
* 21-mar-92 mattfe      significant alterations for WOW
\***************************************************************************/

int PASCAL WinMain(HANDLE hInstance,
                   HANDLE hPrevInstance, LPSTR lpszCmdLine, int iCmd)

{
    int i;
    MSG msg;
    LPSTR pch,pch1;
    WORD    ret;
    WOWINFO wowinfo;
    char aszWOWDEB[CCHMAX];
    LPSTR pchWOWDEB;
    HANDLE hMMDriver;


    char        szBuffer[150];
    BOOL        bFinished;
    int         iStart;
    int         iEnd;

    hAppInstance = hInstance ;

    // Only Want One WOWExec
    if (hPrevInstance != NULL) {
        return(FALSE);
    }

    if (!InitializeApp(lpszCmdLine)) {
        OutputDebugString("WOWEXEC: InitializeApp failure!\n");
        return 0;
    }

/*
 * Look for a drivers= line in the [boot] section of SYSTEM.INI
 * If present it is the 16 bit MultiMedia interface, so load it
 */

#ifdef OLD_MMSYSTEM_LOAD
    if (GetPrivateProfileString((LPSTR)"boot", (LPSTR)"drivers",(LPSTR)"", aszMMDriver, sizeof(aszMMDriver), (LPSTR)"SYSTEM.INI")) {
/*
 * We have now discovered an app that rewrites the "drivers" entry with
 * multiple drivers - so the existing load method fails. As a temporary fix
 * while we decide what the "proper" fix is I will always load MMSYSTEM and
 * ONLY MMSYSTEM.
 *
 *       aszMMDriver[sizeof(aszMMDriver)-1] = '\0';
 *       hMMDriver = LoadLibrary((LPSTR)aszMMDriver);
 * #ifdef DEBUG
 *       if (hMMDriver < 32) {
 *           OutputDebugString("WOWEXEC: Load of MultiMedia driver failed\n");
 *       }
 * #endif
 */
        LoadLibrary("MMSYSTEM.DLL");
    }
#else
    /* Load DDL's from DRIVERS section in system.ini
     */
    GetPrivateProfileString( (LPSTR)"boot",      /* [Boot] section */
                            (LPSTR)"drivers",   /* Drivers= */
                            (LPSTR)"",          /* Default if no match */
                            szBuffer,    /* Return buffer */
                            sizeof(szBuffer),
                            (LPSTR)"system.ini" );

    if (!*szBuffer) {
        goto Done;
    }

    bFinished = FALSE;
    iStart    = 0;

    while (!bFinished) {
        iEnd = iStart;

        while (szBuffer[iEnd] && (szBuffer[iEnd] != ' ') &&
               (szBuffer[iEnd] != ',')) {
            iEnd++;
        }

        if (szBuffer[iEnd] == NULL) {
            bFinished = TRUE;
        }
        else {
            szBuffer[iEnd] = NULL;
        }

        /* Load and enable the driver.
         */
        OpenDriver( &(szBuffer[iStart]), NULL, NULL );
        iStart = iEnd + 1;
    }

Done:

#endif

/*
 * Look for a debug= line in the [boot] section of SYSTEM.INI
 * If present it is the 16 bit MultiMedia interface, so load it
 */

    if ( WOWQueryDebug() & 0x0001 ) {
        pchWOWDEB = "WOWDEB.EXE";
    } else {
        pchWOWDEB = "";
    }

    GetPrivateProfileString((LPSTR)"boot", (LPSTR)"debug",pchWOWDEB, aszWOWDEB, sizeof(aszWOWDEB), (LPSTR)"SYSTEM.INI");
    aszWOWDEB[sizeof(aszWOWDEB)-1] = '\0';

    if ( lstrlen(pchWOWDEB) != 0 ) {
        WinExec((LPSTR)aszWOWDEB,SW_SHOW);
    }

#if 0
/*  Preload winspool.exe.   Apps will keep loading and freeing it
 *  which is slow.   We might as well load it now so the reference
 *  count is 1 so it will never be loaded or freed
 */
    //
    // Disabled load of winspool.exe to save 8k.  Size vs. speed,
    // which one do we care about?  Right now, size!
    //
    LoadLibrary("WINSPOOL.EXE");
#endif

    // command line

    if (*lpszCmdLine) {
#ifdef DEBUG
        OutputDebugString("WOWEXEC: - Initial Commandline ");
        OutputDebugString(lpszCmdLine);
        OutputDebugString("\n");
#endif
        // win foo.bar is done relative to the original directory.
        SetCurrentDirectory(szOriginalDirectory);
        ret = ExecApplication(lpszCmdLine, NULL);
        if (ret) {
#ifdef DEBUG
            OutputDebugString("WOWEXEC: - Failed to Load Command Line App, closing WOW\n");
#endif
            ExitKernel();
        }
        SetCurrentDirectory(szWindowsDirectory);
    }

    //
    // Start any apps pending in basesrv queue
    //

    if (gfSharedWOW) {
        while (StartRequestedApp()) {
            /* null stmt */ ;
        }
    }

    while (1)  {
        if (!WowWaitForMsgAndEvent(ghwndMain) &&
            PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) &&
            msg.message != WM_WOWEXECHEARTBEAT )
           {
            if (msg.message != WM_QUIT) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            } else {
                return 1;
            }
        }
    }

    return 1;
}


/***************************************************************************\
* InitializeApp
*
* History:
* 04-13-91 ScottLu      Created.
\***************************************************************************/

BOOL InitializeApp(LPSTR lpszCommandLine)
{
    WNDCLASS wc;
    int cyExecStart, cxExecStart;
    USHORT TitleLen, cbCopy;


    // Remove Real Mode Segment Address

    wc.style            = 0;
    wc.lpfnWndProc      = WndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = hAppInstance;
    wc.hIcon            = LoadIcon(hAppInstance, MAKEINTRESOURCE(ID_WOWEXEC_ICON));
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName     = "MainMenu";
    wc.lpszClassName    = "WOWExecClass";

    if (!RegisterClass(&wc)) {
        OutputDebugString("WOWEXEC: RegisterClass failured\n");
        return FALSE;
    }

    /*
     * Guess size for now.
     */
    cyExecStart = GetSystemMetrics(SM_CYMENU) * 6;
    cxExecStart = GetSystemMetrics(SM_CXSCREEN) / 2;

    /* Load these strings now.  If we need them later, we won't be able to load
     * them at that time.
     */
    LoadString(hAppInstance, IDS_OOMEXITTITLE, szOOMExitTitle, sizeof(szOOMExitTitle));
    LoadString(hAppInstance, IDS_OOMEXITMSG, szOOMExitMsg, sizeof(szOOMExitMsg));
    LoadString(hAppInstance, IDS_APPTITLE, szAppTitleBuffer, sizeof(szAppTitleBuffer));

    ghwndMain = CreateWindow("WOWExecClass", lpszAppTitle,
            WS_OVERLAPPED | WS_CAPTION | WS_BORDER | WS_THICKFRAME |
            WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN |
            WS_SYSMENU,
            30, 30, cxExecStart, cyExecStart,
            NULL, NULL, hAppInstance, NULL);

    if (ghwndMain == NULL ) {
#ifdef DEBUG
        OutputDebugString("WOWEXEC: ghwndMain Null\n");
#endif
        return FALSE;
    }

    //
    // Give our window handle to BaseSrv, which will post WM_WOWEXECSTARTAPP
    // messages when we have commands to pick up.  The return value tells
    // us if we are the shared WOW VDM or not (a seperate WOW VDM).
    // We also pick up the ShowWindow parameter (SW_SHOW, SW_MINIMIZED, etc)
    // for the first WOW app here.  Subsequent ones we get from BaseSrv.
    //

    gfSharedWOW = WOWRegisterShellWindowHandle(ghwndMain, &gwFirstCmdShow);

    //
    // If this isn't the shared WOW, tell the kernel to exit when the
    // last app (except WowExec) exits.
    //

    if (!gfSharedWOW) {
        WowSetExitOnLastApp(TRUE);
    }

      /* Remember the original directory. */
    GetCurrentDirectory(NULL, szOriginalDirectory);
    GetWindowsDirectory(szWindowsDirectory, MAXITEMPATHLEN+1);

#ifdef DEBUG

    ShowWindow(ghwndMain, SW_MINIMIZE);

    //
    // If this is the shared WOW, change the app title string to
    // reflect this and change the window title.  If this is a
    // separate WOW, change the app title to append " - " and the
    // initial command line, so you can tell which WOWExec it is.
    //

    if (gfSharedWOW) {

        LoadString(hAppInstance, IDS_SHAREDAPPTITLE, szAppTitleBuffer, sizeof(szAppTitleBuffer));

    } else {

        //
        // For checked builds only, change the window title to be
        // "WOWExec - CommandLine".  No need to worry about freeing
        // this memory, since when we go away the kernel goes away.
        //

        lpszAppTitle = GlobalLock(
            GlobalAlloc(GMEM_FIXED,
                        lstrlen(szAppTitleBuffer) +
                        lstrlen(lpszCommandLine) +
                        3 +                        // for " - "
                        1                          // for null terminator
                        ));

        lstrcpy(lpszAppTitle, szAppTitleBuffer);
        lstrcat(lpszAppTitle, " - ");
        lstrcat(lpszAppTitle, lpszCommandLine);
    }

    SetWindowText(ghwndMain, lpszAppTitle);
    UpdateWindow(ghwndMain);

#endif

    return TRUE;
}


/***************************************************************************\
* WndProc
*
* History:
* 04-07-91 DarrinM      Created.
\***************************************************************************/

LONG FAR PASCAL WndProc(
    HWND hwnd,
    WORD message,
    WORD wParam,
    LONG lParam)
{
    TEXTMETRIC tm;
    PAINTSTRUCT ps;
    HDC hdc;
    char chbuf[50];
    HICON hIcon;

    switch (message) {
        case WM_CREATE:
            break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
           case MM_ABOUT:
                LoadString(hAppInstance, errTitle, (LPSTR)chbuf, sizeof(chbuf));
                hIcon = LoadIcon(hAppInstance, MAKEINTRESOURCE(ID_WOWEXEC_ICON));
                ShellAbout(ghwndMain, (LPSTR)chbuf, (LPSTR)lpszAppTitle, hIcon);
           break;

           case MM_BREAK:
                _asm int 3
           break;

           case MM_FAULT:
                _asm mov cs:0,ax
           break;

           case MM_EXIT:
                ExitKernel();
           break;

           case MM_WATSON:
                WinExec("drwatson", SW_MINIMIZE );
           break;

        }
        break;

    case WM_WOWEXECSTARTAPP:

#ifdef DEBUG
        OutputDebugString("WOWEXEC - got WM_WOWEXECSTARTAPP\n");
#endif

        //
        // Either BaseSrv or Wow32 asked us to go looking for
        // commands to run.
        //

        if (!gfSharedWOW) {

            //
            // We shouldn't get this message unless we are the shared
            // WOW VDM!
            //

#ifdef DEBUG
            OutputDebugString("WOWEXEC - seperate WOW VDM got WM_WOWEXECSTARTAPP!\n");
#endif
            break;
        }

        //
        // Start requested apps until there are no more to start.
        // This handles the case where several Win16 apps are launched
        // in a row, before BaseSrv has the window handle for WowExec.
        //

        while (StartRequestedApp()) {
            /* Null stmt */ ;
        }
        break;

    case WM_WOWEXECHEARTBEAT:
        // Probably will never get here...
        break;

    case WM_WOWEXECEXITEXEC:
        ExitWindowsExecContinue();      // Kernel Thunk
        break;

    case WM_TIMECHANGE:
        *((DWORD *)(((DWORD)40 << 16) | FIXED_NTVDMSTATE_REL40))
         |= VDM_TIMECHANGE;
        break;

    case WM_DDE_INITIATE:
        {
            // In win31, the Program Manager WindowProc calls peekmessage to filterout
            // otherwindowcreated and otherwindowdestroyed messages (which are atoms in win31)
            // whenever it receives WM_DDE_INITIATE message.
            //
            // This has a side effect - basically when peekmessage is called the app ie program
            // manager effectively yields allowing scheduling of other apps.
            //
            // So we do the side effect thing (simulate win31 behaviour) -
            //
            // The bug: 20221, rumba as/400 can't connect the firsttime to sna server.
            // Scenario: Rumbawsf execs snasrv and blocks on yield, snasrv execs wnap and blocks
            //           on yield. Eventually wnap yields and rumbawsf is scheduled which
            //           broadcasts a ddeinitiate message. When WOWEXEC receives this message
            //           it will peek for nonexistent msg, which allows snasrv to get scheduled

            MSG msg;
            while (PeekMessage(&msg, NULL, 0xFFFF, 0xFFFF, PM_REMOVE))
                DispatchMessage(&msg);
        }
        break;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 1;
}

// Misc File Routines - taken from progman (pmdlg.c) mattfe apr-1 92

//-------------------------------------------------------------------------
PSTR FAR PASCAL GetFilenameFromPath
    // Given a full path returns a ptr to the filename bit. Unless it's a UNC style
    // path in which case
    (
    PSTR szPath
    )
    {
    DWORD dummy;
    PSTR pFileName;
    BOOL fUNC;


    GetPathInfo(szPath, &pFileName, (PSTR*) &dummy, (WORD*) &dummy,
        &fUNC);

    // If it's a UNC then the 'filename' part is the whole thing.
    if (fUNC)
        pFileName = szPath;

    return pFileName;
    }


//-------------------------------------------------------------------------
void NEAR PASCAL GetPathInfo
    // Get pointers and an index to specific bits of a path.
    // Stops scaning at first space.
    (
                        // Uses:
    PSTR szPath,        // The path.

                        // Returns:
    PSTR *pszFileName,  // The start of the filename in the path.
    PSTR *pszExt,       // Extension part of path (starting with the dot).
    WORD *pich,         // Index (in DBCS characters) of filename part starting at 0.
    BOOL *pfUnc         // Contents set to true if it's a UNC style path.
    )
    {
    char *pch;          // Temp variable.
    WORD ich = 0;       // Temp.

    *pszExt = NULL;             // If no extension, return NULL.
    *pszFileName = szPath;      // If no seperate filename component, return path.
    *pich = 0;
    *pfUnc = FALSE;             // Default to not UNC style.

    // Check for UNC style paths.
    if (*szPath == '\\' && *(szPath+1) == '\\')
        *pfUnc = TRUE;

    // Search forward to find the last backslash or colon in the path.
    // While we're at it, look for the last dot.
    for (pch = szPath; *pch; pch = AnsiNext(pch))
        {
        if (*pch == ' ')
            {
            // Found a space - stop here.
            break;
            }
        if (*pch == '\\' || *pch == ':')
            {
            // Found it, record ptr to it and it's index.
            *pszFileName = pch+1;
            *pich = ich+1;
            }
        if (*pch == '.')
            {
            // Found a dot.
            *pszExt = pch;
            }
        ich++;
        }

    // Check that the last dot is part of the last filename.
    if (*pszExt < *pszFileName)
        *pszExt = NULL;

    }


//-----------------------------------------------------------------------------
//  StartRequestedApp
//      Calls WIN32 Base GetNextVDMCommand
//      and then starts the application
//-----------------------------------------------------------------------------
BOOL NEAR PASCAL StartRequestedApp(VOID)
{
    int i;
    LPSTR pch,pch1;
    char ach[CCHMAX];
    char achCurDir[CCHMAX];
    WOWINFO wowinfo;
    LPSTR pchWOWDEB;
    BOOL    b;
    HANDLE  hmemEnvironment = 0 ;

    for (i=0;i<CCHMAX;i++) {
        ach[i] = NULL;
    }

    // Always Fails the First GetNextVdmCommand so we can get the
    // right size to GlobalAlloc.

    wowinfo.lpEnv = 0;
    wowinfo.lpCmdLine = ach;
    wowinfo.iTask = 0;
    wowinfo.CmdLineSize = 0;
    wowinfo.EnvSize = 0;
    wowinfo.CurDrive = 0;
    wowinfo.lpCurDir = achCurDir;
    wowinfo.CurDirSize = sizeof(achCurDir);

    // We call GetNextVDMCommand 2 times
    // First time to get the correct EnvironmentSize
    // Second time to get the record
    // For GetNextVDMCommand to fail we have to set both CmdLineSize
    // and EnvSize to ZERO.

    if (!WowGetNextVdmCommand(&wowinfo)) {

        hmemEnvironment = GlobalAlloc(GMEM_MOVEABLE, wowinfo.EnvSize);

        if ( (hmemEnvironment == NULL) || (wowinfo.EnvSize == 0) ) {
#ifdef DEBUG
            OutputDebugString("WOWEXEC - failed to allocate Environment Memory 1\n");
#endif
            MyMessageBox(IDS_EXECERRTITLE, IDS_NOMEMORYMSG, NULL);
            return FALSE;
        }

        wowinfo.lpEnv    = GlobalLock(hmemEnvironment);

#ifdef DEBUG
        if (wowinfo.lpEnv == NULL) {
            OutputDebugString("WOWEXEC ASSERT - GlobalLock failed, fix this!\n");
            _asm { int 3 };
        }

        if (wowinfo.EnvSize > GlobalSize(hmemEnvironment)) {
            OutputDebugString("WOWEXEC ASSERT - alloced memory too small, fix this!\n");
            _asm { int 3 };
        }
#endif

        wowinfo.CmdLineSize = CCHMAX;

        // At this point we have enough memory and its locked

        b = WowGetNextVdmCommand( &wowinfo );
        if ( !b ) {
#ifdef DEBUG
            OutputDebugString("WOWEXEC - GetNextVdmCommand failed.\n");
#endif
            MyMessageBox(IDS_EXECERRTITLE, IDS_NOMEMORYMSG, ach);
            GlobalUnlock( hmemEnvironment );
            GlobalFree( hmemEnvironment );
            return FALSE;
        }

    } else {

        //
        // Our initial call to GetNextVDMCommand succeeded, even though
        // our buffers were too small.  No problem, it means that there
        // aren't any commands waiting for us.
        //

        if (wowinfo.CmdLineSize) {
#ifdef DEBUG
            OutputDebugString("WOWEXEC - initial GetNextVDMCommand succeeded, CmdLineSize != 0\n");
#endif
        }

        return FALSE;   // We didn't find an app to run.
    }


#ifdef DEBUG
    OutputDebugString("WOWEXEC: CommandLine = <");
    OutputDebugString((LPSTR)ach);
    OutputDebugString(">\n");
#endif

    pch1 = ach ;

    // strip any leading blanks

    while(*pch1) {
        if ( *pch1 == ' ' || *pch1 == '\t' ) {
            pch1++;
        }
        else {
            break;
        }
    }

    pch = pch1;

    // Remove wowexec from commandline if it is there

    while (*pch1 && *pch1 != ' ' && *pch1 != '\t') {
        pch1++;
    }

    if (*pch1 == ' ' || *pch1 == '\t'){

        *pch1 = 0;

        if ( lstrcmp(pch,"WOWEXEC") == 0 || lstrcmp(pch, "wowexec") == 0 ) {

            // Strip of WOWEXEC.xxx
            pch = ++pch1;

            // Move down command line till we get to the app name

            while(*pch == ' ' || *pch == '\t') {
                pch++;
            }
        }
        else
            *pch1 = ' ';
    }
    wowinfo.lpCmdLine = pch;

#ifdef DEBUG
    SetWindowText(ghwndMain, pch);
    UpdateWindow(ghwndMain);
#endif

    ExecApplication(pch, &wowinfo);

#ifdef DEBUG
    SetWindowText(ghwndMain, lpszAppTitle);
    UpdateWindow(ghwndMain);
#endif

    GlobalUnlock(hmemEnvironment);
    GlobalFree(hmemEnvironment);

    return TRUE;  // We ran an app.
}


