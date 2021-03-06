/* $Header: "%n;%v  %f  LastEdit=%w  Locker=%l" */
/* "DDESHARE.C;1  2-Apr-93,17:34:18  LastEdit=IGOR  Locker=IGOR" */
/************************************************************************
* Copyright (c) Wonderware Software Development Corp. 2000-1993.        *
*               All Rights Reserved.                                    *
*************************************************************************/
/* $History: Begin

    DDESHARE.C

    DDE Share Access Applettee. Allows shares and trusted shares to be
    viewed, created, or modified.

    Revisions:
    12-92   PhilH.  Wonderware port from WFW'd DDEShare.
     3-93   IgorM.  Wonderware overhaul. Add trust share access.
                    Access all share types. New Security convictions.

   $History: End */

#define UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nddeapi.h"
#include "nddeapip.h"
#include "dialogs.h"
#include "debug.h"
#include "hexdump.h"
#include "tmpbuf.h"
#include "rc.h"
#include "nddeagnt.h"

#define DDESHARE_VER    TEXT("Version 1.00.12 NT")

// Flags and typedef for the NT LanMan computer browser dialog.
// The actual function is I_SystemFocusDialog, in NTLANMAN.DLL.
#define FOCUSDLG_DOMAINS_ONLY        (1)
#define FOCUSDLG_SERVERS_ONLY        (2)
#define FOCUSDLG_SERVERS_AND_DOMAINS (3)
typedef UINT (APIENTRY *LPFNSYSFOCUS)(HWND, UINT, LPWSTR, UINT, PBOOL,
      LPWSTR, DWORD);
// Typedef for the ShellAbout function
typedef void (WINAPI *LPFNSHELLABOUT)(HWND, LPTSTR, LPTSTR, HICON);

HWND            hWndParent;
BOOL            bNetDDEdbg  = FALSE;
HICON           hIcon1, hIcon2;
HINSTANCE       hInst;
LPTSTR          lpszServer;
TCHAR           szTargetComputer[MAX_COMPUTERNAME_LENGTH+3];
TCHAR           szClassName[] = TEXT("NetDDEShareClass");
TCHAR           szAppName[]   = TEXT("DDE Share");

int WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, int );
BOOL FAR PASCAL InitializeApplication( void );
BOOL FAR PASCAL InitializeInstance(LPSTR lpCmdLine, int nCmdShow);
LRESULT CALLBACK
DdeShareWindowProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK About( HWND hDlg, UINT message, WPARAM wParam,
                        LPARAM lParam);
LRESULT CALLBACK TrustSharesDlg(HWND hDlg, UINT message,
                        WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DdeSharesDlg(HWND hDlg, UINT message,
                        WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK AddShareDlg( HWND hDlg, UINT message,
                        WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TrustedShareDlg( HWND hDlg, UINT message,
                        WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ServerNameDlg( HWND hDlg, UINT message,
                        WPARAM wParam, LPARAM lParam);
BOOL    RefreshShareWindow ( HWND );
BOOL    RefreshTrustedShareWindow ( HWND );
int     GetNDDEdbg(PSTR);
VOID    NDDEdbgDump(void);
VOID    ReverseNDDEdbg(int, PSTR);
extern VOID HandleError ( HWND hwnd, TCHAR * s, UINT code );

int
WINAPI
WinMain (
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine,
    int nCmdShow)
{
    MSG         msg;
    DWORD       len;

    hInst = hInstance;
    msg.wParam = 0;

    /*  Setup the configuration file path in a permenent string (in DS).
        Strip trailing blanks and tack on a \ if needed.
    */

    DebugInit( "DDESHARE" );

    if (*lpszCmdLine == '+') {
        bNetDDEdbg = TRUE;
        lpszCmdLine++;
    }
    if( *lpszCmdLine == '\0' )  {
        len = MAX_COMPUTERNAME_LENGTH + 1;
        lstrcpy( szTargetComputer, TEXT("\\\\") );
        if( GetComputerName( &szTargetComputer[2], &len ) ) {
            lpszServer = szTargetComputer;
        } else {
            lpszServer = NULL;
        }
#ifdef UNICODE
        DPRINTF(( "%ws Targetting local computer", DDESHARE_VER ));
#else
        DPRINTF(( "%s Targetting local computer", DDESHARE_VER ));
#endif
    } else {
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, lpszCmdLine, -1,
                        szTargetComputer, MAX_COMPUTERNAME_LENGTH+1 );
        lpszServer = szTargetComputer;
#ifdef UNICODE
        DPRINTF(( "%ws Targetting \"%ws\"", DDESHARE_VER, lpszServer ));
#else
        DPRINTF(( "%s Targetting \"%s\"", DDESHARE_VER, lpszServer ));
#endif
    }
    if( !InitializeApplication() ) {
        DPRINTF(("Could not initialize application"));
        return 0;
    }

    if( !InitializeInstance(lpszCmdLine, nCmdShow) ) {
        DPRINTF(("Could not initialize instance"));
        return 0;
    }

    while (GetMessage ((LPMSG)&msg, (HWND)NULL, 0, 0)) {
        TranslateMessage ( &msg);
        DispatchMessage (&msg);
    }

    return( (int) msg.wParam );
}

BOOL FAR PASCAL InitializeApplication( void )
{
    WNDCLASS  wc;

    // Register the frame class
    wc.style         = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS
                        | CS_BYTEALIGNWINDOW;
    wc.lpfnWndProc   = DdeShareWindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon( hInst, TEXT("NetDDE") );
    wc.hCursor       = LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_APPWORKSPACE+1);
    wc.lpszMenuName  = TEXT("DdeShareMenu");
    wc.lpszClassName = szClassName;

    if (!RegisterClass (&wc))  {
        return( FALSE );
    }

    return TRUE;
}


/*----------------------  InitializeInstance  --------------------------*/


BOOL
FAR PASCAL
InitializeInstance(
    LPSTR   lpCmdLine,
    int     nCmdShow)
{
    TCHAR   szBuf[100];
    HMENU   hDebugMenu;

    // Create the parent window

    hWndParent = CreateWindow (szClassName,
            szAppName,
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT,
            0,
            300,
            150,
            NULL,
            NULL,
            hInst,
            NULL);

    if (hWndParent ) {
        lstrcpy(szBuf, szAppName);
        lstrcat(szBuf, TEXT(" on "));
        lstrcat(szBuf, lpszServer);
        SetWindowText(hWndParent, szBuf);
        if( bNetDDEdbg )  {
            hDebugMenu = GetSystemMenu( hWndParent, FALSE );
            ChangeMenu(hDebugMenu, 0,
                TEXT("Log &DDE Traffic"), IDM_DEBUG_DDE,
                MF_APPEND | MF_STRING | MF_MENUBARBREAK );
            ChangeMenu(hDebugMenu, 0,
                TEXT("Log &Info"), IDM_LOG_INFO,
                MF_APPEND | MF_STRING );
            ChangeMenu(hDebugMenu, 0,
                TEXT("Log &Errors"), IDM_LOG_ERRORS,
                MF_APPEND | MF_STRING );
            ChangeMenu(hDebugMenu, 0,
                TEXT("Log DDE &Packets"), IDM_LOG_DDE_PKTS,
                MF_APPEND | MF_STRING );
            ChangeMenu(hDebugMenu, 0,
                TEXT("Dump &NetDDE State"), IDM_DEBUG_NETDDE,
                MF_APPEND | MF_STRING );
            CheckMenuItem( hDebugMenu, IDM_LOG_INFO,
                GetNDDEdbg("DebugInfo") ? MF_CHECKED : MF_UNCHECKED );
            CheckMenuItem( hDebugMenu, IDM_LOG_DDE_PKTS,
                GetNDDEdbg("DebugDdePkts") ? MF_CHECKED : MF_UNCHECKED );
            CheckMenuItem( hDebugMenu, IDM_LOG_ERRORS,
                GetNDDEdbg("DebugErrors") ? MF_CHECKED : MF_UNCHECKED );
            CheckMenuItem( hDebugMenu, IDM_DEBUG_DDE,
                GetNDDEdbg("DebugDDEMessages") ? MF_CHECKED : MF_UNCHECKED );
        }

        ShowWindow (hWndParent, nCmdShow);
        UpdateWindow (hWndParent);
        return TRUE;
    }

    return FALSE;
}

VOID
CenterDlg(HWND hDlg)
{
    int             screenHeight;
    int             screenWidth;
    RECT            rect;

    GetWindowRect(hDlg, &rect);

    screenHeight = GetSystemMetrics(SM_CYSCREEN);
    screenWidth  = GetSystemMetrics(SM_CXSCREEN);

    MoveWindow(hDlg,
           (screenWidth - (rect.right - rect.left)) / 2,
           (screenHeight - (rect.bottom - rect.top)) / 2,
           rect.right - rect.left,
           rect.bottom - rect.top,
           FALSE);
    SetFocus(GetDlgItem(hDlg, IDOK));
}

LRESULT
CALLBACK
About(
    HWND        hDlg,
    UINT        message,
    WPARAM      wParam,
    LPARAM      lParam)
{
    switch (message) {

    case WM_INITDIALOG:
        CenterDlg(hDlg);
        return FALSE;

    case WM_COMMAND:
        EndDialog(hDlg, TRUE);
        return TRUE;

    default:
        return FALSE;
    }
}

LRESULT CALLBACK
DdeShareWindowProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    int         x, y;
    PAINTSTRUCT ps;
    HDC         hDC;

    switch (msg)  {

    case WM_CREATE:
        START_NETDDE_SERVICES(hWnd);
        hIcon1 = LoadIcon(hInst, TEXT("DdeShare"));
        hIcon2 = LoadIcon(hInst, TEXT("TrustShare"));
        break;

    case WM_PAINT:
        hDC = BeginPaint(hWnd, &ps);
        DrawIcon(hDC, 10, 10, hIcon1);
        DrawIcon(hDC, 62, 10, hIcon2);
        EndPaint(hWnd, &ps);
        break;

    case WM_LBUTTONDBLCLK:
        x = LOWORD(lParam);
        y = HIWORD(lParam);
        if (y > 10 && y < 42) {
            if (x > 10 && x < 42) {
                PostMessage(hWnd, WM_COMMAND, IDM_DDESHARES, 0L);
            } else if (x > 62 && x < 94) {
                PostMessage(hWnd, WM_COMMAND, IDM_TRUSTSHARES, 0L);
            }
        }
        break;

    case WM_COMMAND:
        switch( LOWORD( wParam ) ) {
        case IDM_DDESHARES:
            DialogBoxParam(hInst, TEXT("DDESHARES_DLG"), hWnd,
                                (DLGPROC) DdeSharesDlg, 0L );
            break;

        case IDM_TRUSTSHARES:
            DialogBoxParam(hInst, TEXT("TRUSTSHARES_DLG"), hWnd,
                                (DLGPROC) TrustSharesDlg, 0L );
            break;

        case IDM_SERVERNAME:
             {
             WCHAR rgwch[MAX_COMPUTERNAME_LENGTH + 5];
             TCHAR szBuf[MAX_COMPUTERNAME_LENGTH + 32];
             BOOL  bOK = FALSE;
             BOOL  fFoundLMDlg = FALSE;
             HMODULE hMod;
             LPFNSYSFOCUS lpfn;

             rgwch[0] = L'\0';

             if (hMod = LoadLibraryW(L"NTLANMAN.DLL"))
                {
                if (lpfn = (LPFNSYSFOCUS)GetProcAddress(hMod, "I_SystemFocusDialog"))
                   {
                   fFoundLMDlg = TRUE;
                   (*lpfn)(hWnd, FOCUSDLG_SERVERS_ONLY, rgwch,
                        MAX_COMPUTERNAME_LENGTH, &bOK, NULL, 0);

                   if (IDOK == bOK && rgwch[0])
                      {
                      #ifndef UNICODE
                      WideCharToMultiByte(CP_ACP,
                          WC_COMPOSITECHECK | WC_DISCARDNS, rgwch,
                          -1, szTargetComputer, MAX_COMPUTERNAME_LENGTH + 2, NULL, &bOK);
                      #else
                      lstrcpy(szTargetComputer, rgwch);
                      #endif

                      lpszServer = szTargetComputer;
                      }
                   // else User hit Cancel or entered an empty c-name
                   }
                // Else couldn't get the proc
                FreeLibrary(hMod);
                }
             // Else couldn't find the DLL

             // If we didn't find the fancy LanMan dialog, we still can get
             // by with our own cheesy version-- 'course, ours comes up faster, too.
             if (!fFoundLMDlg)
                {
                bOK = DialogBoxParam(hInst, TEXT("SERVERNAME_DLG"), hWnd,
                                (DLGPROC) ServerNameDlg, 0L );
                }

             lstrcat(lstrcat(lstrcpy(szBuf, szAppName), TEXT(" on ")),
                        szTargetComputer);
             SetWindowText(hWnd, szBuf);
             }
            break;

        case IDM_EXIT:
            PostMessage(hWnd, WM_CLOSE, 0, 0L);
            break;

        case MENU_HELP_ABOUT:
            {
            HMODULE hMod;
            LPFNSHELLABOUT lpfn;

            if (hMod = LoadLibrary(TEXT("SHELL32")))
               {
               if (lpfn = (LPFNSHELLABOUT)GetProcAddress(hMod,
                  #ifdef UNICODE
                    "ShellAboutW"
                  #else
                    "ShellAboutA"
                  #endif
                  ))
                  {
                  (*lpfn)(hWnd, szAppName, TEXT(""),
                       LoadIconW(hInst, L"NetDDE"));
                  }
               FreeLibrary(hMod);
               }
            // Else couldn't load lib
            }
            break;

         default:
            return DefWindowProc( hWnd, msg, wParam, lParam );
            break;
        }
        break;

    case WM_SYSCOMMAND:
        switch( LOWORD(wParam) ) {
        case IDM_DEBUG_DDE:
            ReverseNDDEdbg(wParam, "DebugDDEMessages");
            break;
        case IDM_LOG_INFO:
            ReverseNDDEdbg(wParam, "DebugInfo");
            break;
        case IDM_LOG_ERRORS:
            ReverseNDDEdbg(wParam, "DebugErrors");
            break;
        case IDM_LOG_DDE_PKTS:
            ReverseNDDEdbg(wParam, "DebugDdePkts");
            break;
        case IDM_DEBUG_NETDDE:
            NDDEdbgDump();
            break;
        default:
            return (DefWindowProc(hWnd, msg, wParam, lParam));
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage (0);
        return DefWindowProc( hWnd, msg, wParam, lParam );
        break;

    default:
        return DefWindowProc( hWnd, msg, wParam, lParam );
    }

    return 0;
}

BOOL
RefreshShareWindow ( HWND hDlg )
{
    UINT    RetCode;
    DWORD   entries;
    DWORD   avail;
    HWND    hCtl;
    TCHAR * s;
    LPBYTE  lpBuf;
    BOOL    OK;

    /* probe for lenght */
    RetCode = NDdeShareEnum ( lpszServer, 0, (LPBYTE)NULL, 0, &entries, &avail );
    if (RetCode != NDDE_BUF_TOO_SMALL) {
        HandleError ( hWndParent, TEXT("Probing for Number of Shares"), RetCode );
        return FALSE;
    }
    lpBuf = LocalAlloc(LPTR, avail);
    if (lpBuf == NULL) {
        MessageBox ( hWndParent, TEXT("Unable to allocate sufficient memory."),
            TEXT("Enumerating Shares"), MB_ICONEXCLAMATION | MB_OK );
        return FALSE;
    }
    RetCode = NDdeShareEnum ( lpszServer, 0, lpBuf, avail, &entries, &avail );
    HandleError ( hWndParent, TEXT("Enumerating Shares"), RetCode );
    hCtl = GetDlgItem(hDlg, IDC_SHARE_LIST);
    SendMessage ( hCtl, LB_RESETCONTENT, 0, 0L );
    if (RetCode == NDDE_NO_ERROR) {
        for ( s = (TCHAR *)lpBuf; *s; s += lstrlen(s) + 1 ) {
            SendMessage(hCtl, LB_ADDSTRING, 0, (LPARAM)s );
        }
        OK = TRUE;
    } else {
        SendMessage(hCtl, LB_ADDSTRING, 0, (LPARAM)TEXT("DDE Shares inaccessible."));
        OK = FALSE;
    }
    LocalFree(lpBuf);
    return(OK);
}

BOOL
RefreshTrustedShareWindow ( HWND hDlg )
{
    UINT    RetCode;
    DWORD   entries;
    DWORD   avail;
    TCHAR * s;
    HWND    hCtl;
    LPBYTE  lpBuf;

    /* probe for lenght */
    RetCode = NDdeTrustedShareEnum ( lpszServer, 0, (LPBYTE)NULL, 0, &entries, &avail );
    if (RetCode != NDDE_BUF_TOO_SMALL) {
        HandleError ( hWndParent, TEXT("Probing for Number of Trusted Shares"), RetCode );
        return FALSE;
    }
    lpBuf = LocalAlloc(LPTR, avail);
    if (lpBuf == NULL) {
        MessageBox ( hWndParent, TEXT("Unable to allocate sufficient memory."),
            TEXT("Enumerating Trusted Shares"), MB_ICONEXCLAMATION | MB_OK );
        return FALSE;
    }
    RetCode = NDdeTrustedShareEnum ( lpszServer, 0, lpBuf, avail,
            &entries, &avail );
    HandleError ( hWndParent, TEXT("Enumerating Trusted Shares"), RetCode );
    hCtl = GetDlgItem(hDlg, IDC_SHARE_LIST);
    SendMessage ( hCtl, LB_RESETCONTENT, 0, 0L );
    if (RetCode == NDDE_NO_ERROR) {
        for ( s = (TCHAR *)lpBuf; *s; s += lstrlen(s) + 1 ) {
            SendMessage(hCtl, LB_ADDSTRING, 0, (LPARAM)s );
        }
        LocalFree(lpBuf);
        return(TRUE);
    } else {
        LocalFree(lpBuf);
        return(FALSE);
    }
}


VOID HandleError (
    HWND    hwnd,
    TCHAR * s,
    UINT    code )
{
    TCHAR           szBuf[128];

    if ( code == NDDE_NO_ERROR )
            return;
    NDdeGetErrorString ( code, szBuf, 128 );
    MessageBox ( hwnd, szBuf, s, MB_ICONEXCLAMATION | MB_OK );
}

LRESULT
CALLBACK
DdeSharesDlg(
    HWND        hDlg,
    UINT        message,
    WPARAM      wParam,
    LPARAM      lParam)
{
    int             idx;
    HWND            hCtl;
    UINT            RetCode;
    TCHAR           szBuf[128];

    switch (message) {

    case WM_INITDIALOG:
        CenterDlg(hDlg);
        if (RefreshShareWindow(hDlg) == FALSE) {
            PostMessage(hDlg, IDCANCEL, 0, 0L);
            return(FALSE);
        }
        break;

    case WM_COMMAND:
        switch ( LOWORD(wParam) ) {
        case IDC_ADD_SHARE:
            if ( DialogBoxParam(hInst, TEXT("DDESHARE_DLG"), hDlg,
                                (DLGPROC) AddShareDlg, 0L ) )
                    RefreshShareWindow(hDlg);
            break;
        case IDC_DELETE_SHARE:
            hCtl = GetDlgItem(hDlg, IDC_SHARE_LIST);
            idx = (int)SendMessage( hCtl, LB_GETCURSEL, 0, 0L );
            if ( idx == LB_ERR ) {
                MessageBox ( hDlg,
                    TEXT("No share selected.\nNeed to select a DDE share to delete."),
                    TEXT("DDE Share Access"),
                    MB_ICONEXCLAMATION | MB_OK );
                break;
            }
            SendMessage( hCtl, LB_GETTEXT, idx, (LPARAM)szBuf );
            RetCode = NDdeShareDel ( lpszServer, szBuf, 0 );
            if (RetCode == NDDE_NO_ERROR) {
                RefreshShareWindow(hDlg);
                RetCode = NDdeSetTrustedShare(lpszServer, szBuf, 0);
                if (RetCode != NDDE_NO_ERROR) {
                    HandleError ( hDlg, TEXT("Deleting Trusted DDE Share"), RetCode);
                }
            } else {
                HandleError ( hDlg, TEXT("Deleting DDE Share"), RetCode);
            }
            break;
        case IDC_SHARE_LIST:
            if (HIWORD(wParam) != LBN_DBLCLK) {
                break;
            }
            /*  fall through */
        case IDC_PROPERTIES:
            hCtl = GetDlgItem(hDlg, IDC_SHARE_LIST);
            idx = (int)SendMessage( hCtl, LB_GETCURSEL, 0, 0L );
            if ( idx == LB_ERR ) {
                MessageBox ( hDlg,
                    TEXT("No share selected.\nNeed to select a DDE share to view its properties."),
                    TEXT("DDE Share Access"),
                    MB_ICONEXCLAMATION | MB_OK );
                break;
            }
            SendMessage( hCtl, LB_GETTEXT, idx, (LPARAM)szBuf );
            if (DialogBoxParam( hInst, TEXT("DDESHARE_DLG"), hDlg,
                    (DLGPROC) AddShareDlg, (LPARAM)(LPTSTR)szBuf))
                RefreshShareWindow(hDlg);
            break;
        case IDC_TRUST_SHARE:
            hCtl = GetDlgItem(hDlg, IDC_SHARE_LIST);
            idx = (int)SendMessage( hCtl, LB_GETCURSEL, 0, 0L );
            if ( idx == LB_ERR ) {
                MessageBox ( hDlg,
                    TEXT("No share selected.\nNeed to select a DDE share to trust."),
                    TEXT("DDE Share Access"),
                    MB_ICONEXCLAMATION | MB_OK );
                break;
            }
            SendMessage( hCtl, LB_GETTEXT, idx, (LPARAM)szBuf );
            DialogBoxParam( hInst, TEXT("TRUSTEDSHARE_DLG"), hDlg,
                (DLGPROC) TrustedShareDlg, (LPARAM)(LPTSTR)szBuf);
            break;
        case IDOK:
        case IDCANCEL:
            EndDialog(hDlg, TRUE);
            break;
        default:
            return(FALSE);
        }
        break;
    default:
        return FALSE;
    }
    return(TRUE);
}

LRESULT
CALLBACK
TrustSharesDlg(
    HWND        hDlg,
    UINT        message,
    WPARAM      wParam,
    LPARAM      lParam)
{
    int             idx;
    HWND            hCtl;
    TCHAR           szBuf[128];

    switch (message) {

    case WM_INITDIALOG:
        CenterDlg(hDlg);
        if (RefreshTrustedShareWindow(hDlg) == FALSE) {
            PostMessage(hDlg, IDCANCEL, 0, 0L);
            return FALSE;
        }
        break;

    case WM_COMMAND:
        switch ( LOWORD(wParam) ) {
        case IDC_SHARE_LIST:
            if (HIWORD(wParam) != LBN_DBLCLK) {
                break;
            }
            /*  fall through */
        case IDC_PROPERTIES:
            hCtl = GetDlgItem(hDlg, IDC_SHARE_LIST);
            idx = (int)SendMessage( hCtl, LB_GETCURSEL, 0, 0L );
            if ( idx == LB_ERR ) {
                MessageBox ( hDlg,
                    TEXT("No share selected.\nNeed to select a trusted share to view."),
                    TEXT("Trusted Shares Access"),
                    MB_ICONEXCLAMATION | MB_OK );
                break;
            }
            SendMessage( hCtl, LB_GETTEXT, idx, (LPARAM)szBuf );
            DialogBoxParam( hInst, TEXT("TRUSTEDSHARE_DLG"), hDlg,
                (DLGPROC) TrustedShareDlg, (LPARAM)(LPTSTR)szBuf);
            break;
        case IDC_DELETE_SHARE:
            hCtl = GetDlgItem(hDlg, IDC_SHARE_LIST);
            idx = (int)SendMessage( hCtl, LB_GETCURSEL, 0, 0L );
            if ( idx == LB_ERR ) {
                MessageBox ( hDlg,
                    TEXT("No share selected.\nNeed to select a trusted share to delete."),
                    TEXT("Trusted Shares Access"),
                    MB_ICONEXCLAMATION | MB_OK );
                break;
            }
            SendMessage( hCtl, LB_GETTEXT, idx, (LPARAM)szBuf );
            HandleError ( hDlg, TEXT("Deleting Trusted Share"),
                NDdeSetTrustedShare ( lpszServer, szBuf, NDDE_TRUST_SHARE_DEL ) );
            RefreshTrustedShareWindow(hDlg);
            break;

        case IDOK:
        case IDCANCEL:
            EndDialog(hDlg, TRUE);
            break;
        default:
            return(FALSE);
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

LRESULT
CALLBACK
TrustedShareDlg(
    HWND        hDlg,
    UINT        message,
    WPARAM      wParam,
    LPARAM      lParam)
{
    BOOL            fError;
    TCHAR           szBuf[128];
    DWORD           dwOptions = 0;
    DWORD           dwSerial0, dwSerial1;
    UINT            RetCode;

    switch (message) {

    case WM_INITDIALOG:
        CenterDlg(hDlg);
        SetDlgItemText( hDlg, IDC_SHARE_NAME, (LPCTSTR) lParam );
        RetCode = NDdeGetTrustedShare(lpszServer, (LPTSTR)lParam, &dwOptions,
            &dwSerial0, &dwSerial1);
        if (RetCode == NDDE_NO_ERROR) {
            CheckDlgButton( hDlg, IDC_START_APP, dwOptions & NDDE_TRUST_SHARE_START );
            CheckDlgButton( hDlg, IDC_INIT_ENABLE, dwOptions & NDDE_TRUST_SHARE_INIT );
            CheckDlgButton( hDlg, IDC_CMD_OVERRIDE, dwOptions & NDDE_TRUST_CMD_SHOW );
            SetDlgItemInt( hDlg, IDC_CMD_SHOW, dwOptions & NDDE_CMD_SHOW_MASK, FALSE );
        }
        break;

    case WM_COMMAND:
        switch ( LOWORD(wParam) ) {
        case IDC_MODIFY:
        case IDOK:
            dwOptions = 0;
            GetDlgItemText(hDlg, IDC_SHARE_NAME, szBuf, sizeof(szBuf));
            dwOptions = GetDlgItemInt(hDlg, IDC_CMD_SHOW, &fError, FALSE);
            if (IsDlgButtonChecked(hDlg, IDC_START_APP)) {
                dwOptions |= NDDE_TRUST_SHARE_START;
            }
            if (IsDlgButtonChecked(hDlg, IDC_INIT_ENABLE)) {
                dwOptions |= NDDE_TRUST_SHARE_INIT;
            }
            if (IsDlgButtonChecked(hDlg, IDC_CMD_OVERRIDE)) {
                dwOptions |= NDDE_TRUST_CMD_SHOW;
            }
            RetCode = NDdeSetTrustedShare(lpszServer, szBuf, dwOptions);
            HandleError ( hWndParent, TEXT("Create/Modify Trusted Shares"), RetCode );
            if (LOWORD(wParam) == IDOK) {
                EndDialog(hDlg, TRUE);
            }
            break;

        case IDCANCEL:
            EndDialog(hDlg, TRUE);
            break;
        default:
            return(FALSE);
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

LRESULT
CALLBACK
ServerNameDlg(
    HWND        hDlg,
    UINT        message,
    WPARAM      wParam,
    LPARAM      lParam)
{
    TCHAR           szBuf[MAX_COMPUTERNAME_LENGTH + 100];
    DWORD           dwOptions = 0;

    switch (message) {

    case WM_INITDIALOG:
        CenterDlg(hDlg);
        break;

    case WM_COMMAND:
        switch ( LOWORD(wParam) ) {
        case IDOK:
            dwOptions = 0;
            GetDlgItemText(hDlg, IDC_SERVER_NAME, szBuf, sizeof(szBuf));
            if (lstrlen(szBuf) > 0) {
                lstrcpy( szTargetComputer, TEXT("\\\\") );
                lstrcat( szTargetComputer, szBuf);
                lpszServer = szTargetComputer;
                lstrcpy(szBuf, szAppName);
                lstrcat(szBuf, TEXT(" on "));
                lstrcat(szBuf, lpszServer);
                SetWindowText(hWndParent, szBuf);
            }
            EndDialog(hDlg, TRUE);
            break;

        case IDCANCEL:
            EndDialog(hDlg, TRUE);
            break;
        default:
            return(FALSE);
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

/*
    Special commands
*/
#define NDDE_SC_TEST        0
#define NDDE_SC_REFRESH     1
#define NDDE_SC_GET_PARAM   2
#define NDDE_SC_SET_PARAM   3
#define NDDE_SC_DUMP_NETDDE 4

struct sc_param {
    LONG    pType;
    LONG    offSection;
    LONG    offKey;
    LONG    offszValue;
    UINT    pData;
};

typedef struct sc_param SC_PARAM;
typedef struct sc_param * PSC_PARAM;

#define SC_PARAM_INT    0
#define SC_PARAM_STRING 1

int
GetNDDEdbg(LPSTR pszName)
{
    BYTE        szBuf[1024];
    PSC_PARAM   pParam;
    LPSTR       lpVal;
    UINT        Value;
    UINT        Count;
    UINT        nCount;
    UINT        RetStat;

    pParam = (PSC_PARAM)szBuf;
    pParam->pType = SC_PARAM_INT;
    pParam->offSection = sizeof(SC_PARAM);
    lpVal = (LPSTR)pParam + pParam->offSection;
    Count = sizeof(SC_PARAM);
    strcpy(lpVal, "General");
    pParam->offKey = pParam->offSection + strlen("General") + 1;
    Count += pParam->offKey;
    lpVal = (LPSTR)pParam + pParam->offKey;
    strcpy(lpVal, pszName);
    Count += strlen(pszName);
    nCount = sizeof(UINT);

    RetStat = NDdeSpecialCommand(lpszServer, NDDE_SC_GET_PARAM,
            (LPBYTE)pParam, Count, (LPBYTE)&Value, &nCount);
    if (RetStat != NDDE_NO_ERROR) {
        DPRINTF(("Bad get special command: %d", RetStat));
    }
    return(Value);
}

void
SetNDDEdbg(
    LPSTR   pszName,
    UINT    inValue)
{
    BYTE        szBuf[1024];
    PSC_PARAM   pParam;
    LPSTR       lpVal;
    UINT        Size;
    UINT        Count   = 0;
    UINT        RetStat;
    UINT        Dummy = 0;


    pParam = (PSC_PARAM)szBuf;
    pParam->pType = SC_PARAM_INT;
    pParam->pData = inValue;
    pParam->offSection = sizeof(SC_PARAM);
    pParam->offszValue = 0;
    lpVal = (LPSTR)pParam + pParam->offSection;
    strcpy(lpVal, "General");
    Size = strlen("General") + 1;
    pParam->offKey = pParam->offSection + Size;
    Count += Size;
    lpVal = (LPSTR)pParam + pParam->offKey;
    strcpy(lpVal, pszName);
    Size =  strlen(pszName) + 1;
    Count += Size + sizeof(SC_PARAM);

    RetStat = NDdeSpecialCommand(lpszServer, NDDE_SC_SET_PARAM,
            (LPBYTE)pParam, Count, (LPBYTE)&Dummy, &Dummy);
    if (RetStat != NDDE_NO_ERROR) {
        DPRINTF(("Bad set special command: %d", RetStat));
    }
    return;
}

VOID
NDDEdbgDump(void)
{
    UINT    RetStat;
    UINT    Dummy = 0;

    RetStat = NDdeSpecialCommand(lpszServer, NDDE_SC_DUMP_NETDDE,
            (LPBYTE)&Dummy, Dummy, (LPBYTE)&Dummy, &Dummy);
    if (RetStat != NDDE_NO_ERROR) {
        DPRINTF(("Bad dump netdde special command: %d", RetStat));
    }
}

VOID
ReverseNDDEdbg(
    int     idMenu,
    LPSTR   pszIniName )
{
    HMENU       hMenu;
    int         DbgFlag;
    UINT    RetStat;
    UINT    Dummy = 0;


    hMenu = GetSystemMenu( hWndParent, FALSE );
    DbgFlag = GetNDDEdbg(pszIniName);
    if (DbgFlag) {
        DbgFlag = 0;
    } else {
        DbgFlag = 1;
    }
    CheckMenuItem( hMenu, idMenu, DbgFlag ? MF_CHECKED : MF_UNCHECKED );
    SetNDDEdbg(pszIniName, DbgFlag);
    RetStat = NDdeSpecialCommand(lpszServer, NDDE_SC_REFRESH,
        (LPBYTE)&Dummy, Dummy,
        (LPBYTE)&Dummy, &Dummy);
    InvalidateRect( hWndParent, NULL, TRUE );
    UpdateWindow( hWndParent );
}
