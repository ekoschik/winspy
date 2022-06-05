//
//  WinSpyWindow.c
//
//  Copyright (c) 2002 by J Brown
//  Freeware
//
//  All the window related functionality for the
//  main window (i.e. sizing, window layout etc)
//

#include "WinSpy.h"
#include "resource.h"
#include "Utils.h"

#if (WINVER < 0x500)
#error "Please install latest Platform SDK or define WINVER >= 0x500"
#endif

//
//  Global variables, only used within this module.
//  It's just not worth putting them in a structure,
//  so I'll leave it like this for now
//
static SIZE duMinimized, szMinimized;
static SIZE duNormal, szNormal;
static SIZE duExpanded, szExpanded;

static SIZE szCurrent;          // current size of window!
static SIZE szLastMax;          // current NON-minimized (i.e. Normal or Expanded)
static SIZE szLastExp;          // the last expanded position

static int nLeftBorder;         // pixels between leftside + tab sheet
static int nBottomBorder;       // pixels between bottomside + tab

static BOOL fxMaxed, fyMaxed;   // Remember our sized state when a size/move starts


//
//  Added: Multimonitor support!!
//
typedef HMONITOR(WINAPI * MFR_PROC)(LPCRECT, DWORD);
static  MFR_PROC pMonitorFromRect = 0;

typedef BOOL(WINAPI * GMI_PROC)(HMONITOR, LPMONITORINFO);
static GMI_PROC pGetMonitorInfo = 0;

static BOOL fFindMultiMon = TRUE;

BOOL _GetMonitorInfo(HMONITOR hmon, MONITORINFO* pmi)
{
    static BOOL tried = FALSE;
    if (!tried)
    {
        HMODULE hUser32 = GetModuleHandle(L"USER32.DLL");
        pGetMonitorInfo = (GMI_PROC)GetProcAddress(hUser32, "GetMonitorInfoW");
    }
    if (pGetMonitorInfo)
    {
        return pGetMonitorInfo(hmon, pmi);
    }

    return FALSE;
}

HMONITOR _MonitorFromRect(PRECT prc, DWORD dwFlags)
{
    static BOOL tried = FALSE;
    if (!tried)
    {
        HMODULE hUser32 = GetModuleHandle(L"USER32.DLL");
        pMonitorFromRect = (MFR_PROC)GetProcAddress(hUser32, "MonitorFromRect");
    }
    if (pMonitorFromRect)
    {
        return pMonitorFromRect(prc, dwFlags);
    }

    return NULL;
}

//  Get the dimensions of the work area that the specified WinRect resides in
void GetWorkArea(RECT *prcWinRect, RECT *prcWorkArea)
{
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);

    HMONITOR hMonitor = _MonitorFromRect(prcWinRect, MONITOR_DEFAULTTONEAREST);
    if (hMonitor && _GetMonitorInfo(hMonitor, &mi))
    {
        *prcWorkArea = mi.rcWork;
        return;
    }

    SystemParametersInfo(SPI_GETWORKAREA, 0, prcWorkArea, FALSE);
}

// Called when the window is closed to store the work area and dpi at
// the time, used when reading the last position from the registry to
// position the initial window.
void SetLastWorkAreaAndDpi(HWND hwnd)
{
    RECT rect;
    RECT rcWorkArea;
    GetWindowRect(hwnd, &rect);
    GetWorkArea(&rect, &rcWorkArea);

    g_opts.ptWorkAreaOrigin.x = rcWorkArea.left;
    g_opts.ptWorkAreaOrigin.y = rcWorkArea.top;
    g_opts.lastWindowDpi = GetDpiForWindow(hwnd);
}

// Called by WinSpy_InitDlg to position and show the main window.
void SetInitialWindowPos(HWND hwnd)
{
    RECT rcStart;
    GetWindowRect(hwnd, &rcStart);

    // If the fSaveWinPos option is set (and the regkeys are also set) we
    // have a previous position to restore. Because we always start in
    // the 'minimized' state we only store a point and the position/dpi
    // of its monitor. If no stored position, use the window's current
    // position (the position the window received from its dialog layout).
    if (!g_opts.fSaveWinPos ||
        (g_opts.ptPinPos.x == CW_USEDEFAULT) ||
        (g_opts.ptPinPos.y == CW_USEDEFAULT))
    {
        g_opts.ptPinPos.x = rcStart.left;
        g_opts.ptPinPos.y = rcStart.top;

        // Update the work area and dpi to match the ptPinPos set above.
        SetLastWorkAreaAndDpi(hwnd);
    }


    // todo... ptPinPos might not be top-left of window,
    //         if uPinnedCorner not PINNED_TOPLEFT...
    //         Should this be checking uPinnedCorner+ptPinPos and
    //         picking a top-left here??
    //         see GetPinnedPosition


    POINT ptWorkAreaStored = g_opts.ptWorkAreaOrigin;
    UINT dpiStored = g_opts.lastWindowDpi;

    // Read the StartupInfo (flags provided by caller of CreateProcess).
    STARTUPINFO si;
    GetStartupInfo(&si);

    // If provided a 'monitor hint' in the StartupInfo, for example if
    // launched from the taskbar, use that monitor. Otherwise us the
    // window's current monitor.
    RECT rcWork;
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (_GetMonitorInfo(si.hStdOutput, &mi))
    {
        rcWork = mi.rcWork;
    }
    else
    {
        GetWorkArea(&rcStart, &rcWork);
    }

    // Move the window onto the target monitor (pre-emptively).
    // This avoids a 'jump' if scaling per-monitor and changing DPIs,
    // and if 'unuaware' of DPI this makes sure we're on the monitor
    // we're asking about (so we don't get virtualized coordinates
    // in cases where the system has multiple DPIs).
    SetWindowPos(hwnd, NULL, rcWork.left, rcWork.top, 0, 0,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);

    // Refresh metrics (including szMinimized which is used below).
    WinSpyDlg_SizeContents(hwnd);

    // Refresh the work area and get the DPI of the window.
    // These may have changed if the window moved between monitors with
    // different DPIs.
    UINT dpi = GetDpiForWindow(hwnd);
    GetWindowRect(hwnd, &rcStart);
    GetWorkArea(&rcStart, &rcWork);

    // Adjust the starting position from the previous monitor to the new
    // monitor. This retains the offset the position had relative to its
    // previous monitor. For example if near the top-left of a monitor
    // when closed we should launch near the top-left of whichever monitor
    // we launch to.
    g_opts.ptPinPos.x = rcWork.left + MulDiv(g_opts.ptPinPos.x - ptWorkAreaStored.x, dpi, dpiStored);
    g_opts.ptPinPos.y = rcWork.top + MulDiv(g_opts.ptPinPos.y - ptWorkAreaStored.y, dpi, dpiStored);

    // Require that the entire window (in 'minimized' size) is within the work area
    // at the chosen position, moving the position up/left as needed.
    if (g_opts.ptPinPos.x + szMinimized.cx > rcWork.right)
    {
        g_opts.ptPinPos.x -= ((g_opts.ptPinPos.x + szMinimized.cx) - rcWork.right);
    }
    if (g_opts.ptPinPos.y + szMinimized.cy > rcWork.bottom)
    {
        g_opts.ptPinPos.y -= ((g_opts.ptPinPos.y + szMinimized.cy) - rcWork.bottom);
    }

    // Honor requests in the StartupInfo to launch minimized.
    // Ignore requests to launch maximized (or normal).
    BOOL startMinimized = ((si.dwFlags & STARTF_USESHOWWINDOW) &&
                           ((si.wShowWindow == SW_SHOWMINNOACTIVE) ||
                            (si.wShowWindow == SW_SHOWMINIMIZED) ||
                            (si.wShowWindow == SW_MINIMIZE)));

    // Move the window to the initial position.
    // If not starting minimized also show and activate it.
    SetWindowPos(hwnd,
                 NULL,
                 g_opts.ptPinPos.x,
                 g_opts.ptPinPos.y,
                 szMinimized.cx,
                 szMinimized.cy,
                 startMinimized ? SWP_NOACTIVATE | SWP_NOZORDER : SWP_SHOWWINDOW);

    if (startMinimized)
    {
        // Minimize the window using the command from the StartupInfo.
        ShowWindow(hwnd, si.wShowWindow);
    }
}

void GetPinnedPosition(HWND hwnd, POINT *pt)
{
    RECT rect;
    RECT rcDisplay;

    //
    GetWindowRect(hwnd, &rect);

    // get
    GetWorkArea(&rect, &rcDisplay);

    UINT uPinnedCorner = PINNED_NONE;

    if (rect.left + szLastExp.cx >= rcDisplay.right)
        uPinnedCorner |= PINNED_RIGHT;
    else
        uPinnedCorner |= PINNED_LEFT;

    if (rect.top + szLastExp.cy >= rcDisplay.bottom)
        uPinnedCorner |= PINNED_BOTTOM;
    else
        uPinnedCorner |= PINNED_TOP;

    if (g_opts.fPinWindow == FALSE)
        uPinnedCorner = PINNED_TOPLEFT;

    switch (uPinnedCorner)
    {
    case PINNED_TOPLEFT:
        pt->x = rect.left;
        pt->y = rect.top;
        break;

    case PINNED_TOPRIGHT:
        pt->x = rect.right;
        pt->y = rect.top;
        break;

    case PINNED_BOTTOMRIGHT:
        pt->x = rect.right;
        pt->y = rect.bottom;
        break;

    case PINNED_BOTTOMLEFT:
        pt->x = rect.left;
        pt->y = rect.bottom;
        break;

    }

    //
    // Sanity check!!!
    //
    // If the window is in an expanded state, and it is
    // moved so that its lower-right edge extends off the screen,
    // then when it is minimized, it will disappear (i.e. position
    // itself off-screen!). This check stops that
    //
    if (pt->x - szLastExp.cx < rcDisplay.left || pt->x >= rcDisplay.right)
    {
        pt->x = rect.left;
        uPinnedCorner &= ~PINNED_RIGHT;
    }

    if (pt->y - szLastExp.cy < rcDisplay.top || pt->y >= rcDisplay.bottom)
    {
        pt->y = rect.top;
        uPinnedCorner &= ~PINNED_BOTTOM;
    }

    g_opts.uPinnedCorner = uPinnedCorner;
}

//
//  Return TRUE if the specified window is minimized to the taskbar.
//
BOOL IsWindowMinimized(HWND hwnd)
{
    return IsIconic(hwnd);
}

//
//  hwnd       - window to calc
//  szDlgUnits - (input)  size in dialog units
//  szClient   - (output) size of client area in pixels
//  szWindow   - (output) total size of based on current settings
//
void CalcDlgWindowSize(HWND hwnd, SIZE *szDlgUnits, SIZE *szClient, SIZE *szWindow)
{
    RECT rect;
    DWORD dwStyle;
    DWORD dwStyleEx;

    // work out the size in pixels of our main window, by converting
    // from dialog units
    SetRect(&rect, 0, 0, szDlgUnits->cx, szDlgUnits->cy);
    MapDialogRect(hwnd, &rect);

    if (szClient)
    {
        szClient->cx = GetRectWidth(&rect);
        szClient->cy = GetRectHeight(&rect);
    }

    dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    dwStyleEx = GetWindowLong(hwnd, GWL_EXSTYLE);

    AdjustWindowRectEx(&rect, dwStyle, FALSE, dwStyleEx);

    if (szWindow)
    {
        szWindow->cx = GetRectWidth(&rect);
        szWindow->cy = GetRectHeight(&rect);
    }
}

//
// Position / size controls and main window based
// on current system metrics
//
void WinSpyDlg_SizeContents(HWND hwnd)
{
    int x, y, cx, cy;
    int i;
    RECT rect, rect1;
    HWND hwndTab;
    HWND hwndCtrl;

    int nPaneWidth;     // width of each dialog-pane
    int nPaneHeight;    // height of each dialog-pane
    int nActualPaneWidth; // what the tab-control is set to.

    int nTabWidth;
    int nTabHeight;

    int nDesiredTabWidth;

    // HARD-CODED sizes for each window layout.
    // These are DIALOG UNITS, so it's not too bad.
    duMinimized.cx = 254;
    duMinimized.cy = 25;

    duNormal.cx = duMinimized.cx;
    duNormal.cy = 251;

    duExpanded.cx = 500;
    duExpanded.cy = duNormal.cy;

    // work out the size (in pixels) of each window layout
    CalcDlgWindowSize(hwnd, &duMinimized, 0, &szMinimized);
    CalcDlgWindowSize(hwnd, &duNormal, 0, &szNormal);
    CalcDlgWindowSize(hwnd, &duExpanded, 0, &szExpanded);

    // resize to NORMAL layout (temporarily)
    SetWindowPos(hwnd, 0, 0, 0, szNormal.cx, szNormal.cy, SWP_SIZEONLY | SWP_NOREDRAW);

    // Locate main Property sheet control
    hwndTab = GetDlgItem(hwnd, IDC_TAB1);

    // Get SCREEN coords of tab control
    GetWindowRect(hwndTab, &rect);

    // Get SCREEN coords of dialog's CLIENT area
    if (IsIconic(hwnd))
    {
        // If the window is minimized, calc the client rect manually

        rect1.left = 0;
        rect1.top = 0;
        rect1.right = szNormal.cx - 2 * GetSystemMetrics(SM_CXFRAME);
        rect1.bottom = szNormal.cy - 2 * GetSystemMetrics(SM_CYFRAME) - GetSystemMetrics(SM_CYCAPTION);
    }
    else
        GetClientRect(hwnd, &rect1);

    MapWindowPoints(hwnd, 0, (POINT *)&rect1, 2);

    // Now we know what the border is between TAB and left-side
    nLeftBorder = rect.left - rect1.left;
    nBottomBorder = rect1.bottom - rect.bottom;

    nDesiredTabWidth = (rect1.right - rect1.left) - nLeftBorder * 2;

    //
    // Find out the size of the biggest dialog-tab-pane
    //
    SetRect(&rect, 0, 0, 0, 0);

    for (i = 0; i < NUMTABCONTROLITEMS; i++)
    {
        // Get tab-pane relative to parent (main) window
        GetClientRect(WinSpyTab[i].hwnd, &rect1);
        MapWindowPoints(WinSpyTab[i].hwnd, hwnd, (POINT *)&rect1, 2);

        // find biggest
        UnionRect(&rect, &rect, &rect1);
    }

    nPaneWidth = GetRectWidth(&rect);
    nPaneHeight = GetRectHeight(&rect);

    // Resize the tab control based on this biggest rect
    SendMessage(hwndTab, TCM_ADJUSTRECT, TRUE, (LPARAM)&rect);

    nTabWidth = GetRectWidth(&rect);
    nTabHeight = GetRectHeight(&rect);

    // Resize the tab control now we know how big it needs to be
    SetWindowPos(hwndTab, hwnd, 0, 0, nDesiredTabWidth, nTabHeight, SWP_SIZEONLY);

    //
    // Tab control is now in place.
    // Now find out exactly where to position every
    // tab-pane. (We know how big they are, but we need
    // to find where to move them to).
    //
    GetWindowRect(hwndTab, &rect);
    ScreenToClient(hwnd, (POINT *)&rect.left);
    ScreenToClient(hwnd, (POINT *)&rect.right);

    SendMessage(hwndTab, TCM_ADJUSTRECT, FALSE, (LPARAM)&rect);

    x = rect.left;
    y = rect.top;
    cx = nPaneWidth;
    cy = nPaneHeight;

    nActualPaneWidth = GetRectWidth(&rect);

    // Center each dialog-tab in the tab control
    x += (nActualPaneWidth - nPaneWidth) / 2;

    // position each dialog in the right place
    for (i = 0; i < NUMTABCONTROLITEMS; i++)
    {
        SetWindowPos(WinSpyTab[i].hwnd, hwndTab, x, y, cx, cy, SWP_NOACTIVATE);
    }


    SetWindowPos(hwnd, 0, 0, 0, szMinimized.cx, szMinimized.cy, SWP_NOMOVE | SWP_NOZORDER);

    // Even though we are initially minimized, we want to
    // automatically expand to normal view the first time a
    // window is selected.
    szCurrent = szMinimized;
    szLastMax = szNormal;
    szLastExp = szExpanded;

    SetWindowPos(hwndTab, //GetDlgItem(hwnd, IDC_MINIMIZE)
        HWND_BOTTOM, 0, 0, 0, 0, SWP_ZONLY);

    // Finally, move the little expand / shrink button
    // so it is right-aligned with the edge of the tab.
    hwndCtrl = GetDlgItem(hwnd, IDC_EXPAND);
    GetWindowRect(hwndCtrl, &rect);
    MapWindowPoints(0, hwnd, (POINT *)&rect, 2);

    x = nDesiredTabWidth + nLeftBorder - GetRectWidth(&rect);
    y = rect.top;

    SetWindowPos(hwndCtrl, 0, x, y, 0, 0, SWP_MOVEONLY);
}

//
//  Retrieve current layout for main window
//
UINT GetWindowLayout(HWND hwnd)
{
    RECT rect;
    BOOL xMaxed, yMaxed;

    GetWindowRect(hwnd, &rect);

    yMaxed = GetRectHeight(&rect) > szMinimized.cy;
    xMaxed = GetRectWidth(&rect) >= szExpanded.cx;

    if (yMaxed == FALSE)
    {
        return WINSPY_MINIMIZED;
    }
    else
    {
        if (xMaxed)
            return WINSPY_EXPANDED;
        else
            return WINSPY_NORMAL;
    }
}

//
//  Switch between minimized and non-minimized layouts
//
void ToggleWindowLayout(HWND hwnd)
{
    UINT layout = GetWindowLayout(hwnd);

    if (layout == WINSPY_MINIMIZED)
    {
        SetWindowLayout(hwnd, WINSPY_LASTMAX);
    }
    else
    {
        SetWindowLayout(hwnd, WINSPY_MINIMIZED);
    }
}

//
//  Switch to a specific layout.
//  Intelligently reposition the window if the new
//  layout won't fit on-screen.
//
void SetWindowLayout(HWND hwnd, UINT uLayout)
{
    DWORD dwSWPflags = SWP_NOZORDER | SWP_NOACTIVATE;

    SIZE   *psz;
    POINT  ptPos;
    POINT  ptPinPos = g_opts.ptPinPos;

    // Decide which layout we are going to use
    switch (uLayout)
    {
    case WINSPY_MINIMIZED:
        psz = &szMinimized;
        break;

    case WINSPY_NORMAL:
        psz = &szNormal;
        break;

    case WINSPY_EXPANDED:
        psz = &szLastExp;
        break;

    default:
    case WINSPY_LASTMAX:
        psz = &szLastMax;
    }

    // Now work out where the top-left corner needs to
    // be, taking into account where the pinned-corner is
    switch (g_opts.uPinnedCorner)
    {
    default:
    case PINNED_TOPLEFT:
        ptPos = ptPinPos;
        break;

    case PINNED_TOPRIGHT:
        ptPos.x = ptPinPos.x - psz->cx;
        ptPos.y = ptPinPos.y;
        break;

    case PINNED_BOTTOMRIGHT:
        ptPos.x = ptPinPos.x - psz->cx;
        ptPos.y = ptPinPos.y - psz->cy;
        break;

    case PINNED_BOTTOMLEFT:
        ptPos.x = ptPinPos.x;
        ptPos.y = ptPinPos.y - psz->cy;
        break;

    }

    // Switch into the new layout!
    SetWindowPos(hwnd, 0, ptPos.x, ptPos.y, psz->cx, psz->cy, dwSWPflags);
}


UINT WinSpyDlg_Size(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    int  cx, cy;
    HWND hwndCtrl;
    RECT rect;
    RECT rect2;

    if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
    {
        cx = LOWORD(lParam);
        cy = HIWORD(lParam);

        // Resize the right-hand tab control so that
        // it fills the window
        hwndCtrl = GetDlgItem(hwnd, IDC_TAB2);
        GetWindowRect(hwndCtrl, &rect);
        ScreenToClient(hwnd, (POINT *)&rect.left);
        ScreenToClient(hwnd, (POINT *)&rect.right);

        MoveWindow(hwndCtrl, rect.left, rect.top, cx - rect.left - nLeftBorder, cy - rect.top - nBottomBorder, TRUE);

        GetWindowRect(hwndCtrl, &rect);
        ScreenToClient(hwnd, (POINT *)&rect.left);
        ScreenToClient(hwnd, (POINT *)&rect.right);
        rect.top++;

        // Work out the coords of the tab contents
        SendMessage(hwndCtrl, TCM_ADJUSTRECT, FALSE, (LPARAM)&rect);

        // Resize the tree control so that it fills the tab control.
        hwndCtrl = GetDlgItem(hwnd, IDC_TREE1);
        InflateRect(&rect, 1, 1);
        MoveWindow(hwndCtrl, rect.left, rect.top, GetRectWidth(&rect), GetRectHeight(&rect), TRUE);

        // Position the size-grip
        {
            int width = GetSystemMetrics(SM_CXVSCROLL);
            int height = GetSystemMetrics(SM_CYHSCROLL);

            GetClientRect(hwnd, &rect);

            MoveWindow(g_hwndSizer, rect.right - width, rect.bottom - height, width, height, TRUE);

        }

        GetWindowRect(g_hwndPin, &rect2);
        OffsetRect(&rect2, -rect2.left, -rect2.top);

        // Position the pin toolbar
        //SetWindowPos(g_hwndPin,
        //  HWND_TOP, rect.right-rect2.right, 1, rect2.right, rect2.bottom, 0);
        MoveWindow(g_hwndPin, rect.right - rect2.right, 1, rect2.right, rect2.bottom, TRUE);
    }

    return 0;
}


//
//  Make sure that only the controls that
//  are visible through the current layout are actually enabled.
//  This prevents the user from Tabbing to controls that
//  are not visible
//
typedef struct
{
    UINT uCtrlId;
    BOOL fEnabled;
} CtrlEnable;

void EnableLayoutCtrls(HWND hwnd, UINT layout)
{
    int i;
    const int nNumCtrls = 9;

    CtrlEnable ctrl0[] =
    {
        IDC_TAB1,       FALSE,
        IDC_AUTOUPDATE, FALSE,
        IDC_CAPTURE,    FALSE,
        IDC_EXPAND,     FALSE,
        IDC_TAB2,       FALSE,
        IDC_TREE1,      FALSE,
        IDC_REFRESH,    FALSE,
        IDC_LOCATE,     FALSE,
        IDC_FLASH,      FALSE,
    };

    CtrlEnable ctrl1[] =
    {
        IDC_TAB1,       TRUE,
        IDC_AUTOUPDATE, TRUE,
        IDC_CAPTURE,    TRUE,
        IDC_EXPAND,     TRUE,
        IDC_TAB2,       FALSE,
        IDC_TREE1,      FALSE,
        IDC_REFRESH,    FALSE,
        IDC_LOCATE,     FALSE,
        IDC_FLASH,      FALSE,
    };

    CtrlEnable ctrl2[] =
    {
        IDC_TAB1,       TRUE,
        IDC_AUTOUPDATE, TRUE,
        IDC_CAPTURE,    TRUE,
        IDC_EXPAND,     TRUE,
        IDC_TAB2,       TRUE,
        IDC_TREE1,      TRUE,
        IDC_REFRESH,    TRUE,
        IDC_LOCATE,     TRUE,
        IDC_FLASH,      TRUE,
    };

    switch (layout)
    {
    case WINSPY_MINIMIZED:

        for (i = 0; i < NUMTABCONTROLITEMS; i++)
            EnableWindow(WinSpyTab[i].hwnd, FALSE);

        for (i = 0; i < nNumCtrls; i++)
            EnableDlgItem(hwnd, ctrl0[i].uCtrlId, ctrl0[i].fEnabled);

        break;

    case WINSPY_NORMAL:

        for (i = 0; i < NUMTABCONTROLITEMS; i++)
            EnableWindow(WinSpyTab[i].hwnd, TRUE);

        for (i = 0; i < nNumCtrls; i++)
            EnableDlgItem(hwnd, ctrl1[i].uCtrlId, ctrl1[i].fEnabled);

        break;

    case WINSPY_EXPANDED:

        for (i = 0; i < NUMTABCONTROLITEMS; i++)
            EnableWindow(WinSpyTab[i].hwnd, TRUE);

        for (i = 0; i < nNumCtrls; i++)
            EnableDlgItem(hwnd, ctrl2[i].uCtrlId, ctrl2[i].fEnabled);

        break;

    }

}

UINT WinSpyDlg_WindowPosChanged(HWND hwnd, WINDOWPOS *wp)
{
    UINT layout;
    HICON hIcon, hOld;

    static UINT oldlayout = WINSPY_LAYOUT_NO;

    if (wp == 0)
        return 0;

    layout = GetWindowLayout(hwnd);

    // Detect if our size has changed
    if (!(wp->flags & SWP_NOSIZE))
    {
        if (layout == WINSPY_EXPANDED)
        {
            szLastExp.cx = wp->cx;
            szLastExp.cy = wp->cy;
        }

        szCurrent.cx = wp->cx;
        szCurrent.cy = wp->cy;

        if (layout != WINSPY_MINIMIZED)
        {
            szLastMax = szCurrent;
        }

        // Has the layout changed as a result?
        if (oldlayout != layout)
        {
            HWND  hwndExpand = GetDlgItem(hwnd, IDC_EXPAND);
            DWORD dwStyle = GetWindowLong(hwndExpand, GWL_STYLE);
            int   cxIcon = DPIScale(hwnd, 16);

            if (layout == WINSPY_NORMAL)
            {
                hIcon = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_MORE), IMAGE_ICON, cxIcon, cxIcon, 0);
                hOld = (HICON)SendDlgItemMessage(hwnd, IDC_EXPAND, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);

                DestroyIcon(hOld);

                SetWindowLong(hwndExpand, GWL_STYLE, dwStyle | BS_RIGHT);
                SetWindowText(hwndExpand, L"&More");
            }
            else
            {
                hIcon = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_LESS), IMAGE_ICON, cxIcon, cxIcon, 0);
                hOld = (HICON)SendDlgItemMessage(hwnd, IDC_EXPAND, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);

                DestroyIcon(hOld);

                SetWindowLong(hwndExpand, GWL_STYLE, dwStyle & ~BS_RIGHT);
                SetWindowText(hwndExpand, L"L&ess");
            }

            SetSysMenuIconFromLayout(hwnd, layout);

            EnableLayoutCtrls(hwnd, layout);
        }

        oldlayout = layout;
    }

    // Has our Z-order changed?
    if (wp && !(wp->flags & SWP_NOZORDER))
    {
        DWORD dwStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

        // Set the global flag (just so we can remember it in the registry)
        if (dwStyle & WS_EX_TOPMOST)
            g_opts.fAlwaysOnTop = TRUE;
        else
            g_opts.fAlwaysOnTop = FALSE;

        CheckSysMenu(hwnd, IDM_WINSPY_ONTOP, g_opts.fAlwaysOnTop);
    }


    return 0;
}


// monitor the sizing rectangle so that the main window
// "snaps" to each of the 3 layouts

UINT WinSpyDlg_Sizing(UINT nSide, RECT *prc)
{
    int minx;
    int miny;
    int maxy;
    int nWidthNew;
    int nHeightNew;

    minx = szMinimized.cx;
    miny = szMinimized.cy;
    maxy = szNormal.cy;

    nWidthNew = prc->right - prc->left;
    nHeightNew = prc->bottom - prc->top;

    if (fxMaxed == FALSE)
    {
        if (nWidthNew <= minx)
            nWidthNew = minx;

        if (nWidthNew > minx && nWidthNew < szExpanded.cx)
            nWidthNew = szExpanded.cx;
    }
    else
    {
        if (nWidthNew < szExpanded.cx)
            nWidthNew = minx;

    }

    if (fyMaxed == FALSE)
    {
        if (nHeightNew > miny)
        {
            nHeightNew = maxy;
        }

        if (nHeightNew <= miny)
        {
            nHeightNew = miny;
            nWidthNew = minx;
        }
    }
    else
    {
        if (nHeightNew < maxy)
        {
            nHeightNew = miny;
            nWidthNew = minx;
        }
        else
            nHeightNew = maxy;
    }

    // Adjust the rectangle's dimensions
    switch (nSide)
    {
    case WMSZ_LEFT:
        prc->left = prc->right - nWidthNew;
        break;

    case WMSZ_TOP:
        prc->top = prc->bottom - nHeightNew;
        //>
        prc->right = prc->left + nWidthNew;
        break;

    case WMSZ_RIGHT:
        prc->right = prc->left + nWidthNew;
        break;

    case WMSZ_BOTTOM:
        prc->bottom = prc->top + nHeightNew;
        //>
        prc->right = prc->left + nWidthNew;
        break;

    case WMSZ_BOTTOMLEFT:
        prc->bottom = prc->top + nHeightNew;
        prc->left = prc->right - nWidthNew;
        break;

    case WMSZ_BOTTOMRIGHT:
        prc->bottom = prc->top + nHeightNew;
        prc->right = prc->left + nWidthNew;
        break;

    case WMSZ_TOPLEFT:
        prc->left = prc->right - nWidthNew;
        prc->top = prc->bottom - nHeightNew;
        break;

    case WMSZ_TOPRIGHT:
        prc->top = prc->bottom - nHeightNew;
        prc->right = prc->left + nWidthNew;
        break;
    }

    return TRUE;
}

UINT WinSpyDlg_EnterSizeMove(HWND hwnd)
{
    RECT rect;
    GetWindowRect(hwnd, &rect);

    fyMaxed = (GetRectHeight(&rect) > szMinimized.cy);
    fxMaxed = (GetRectWidth(&rect) >= szExpanded.cx);

    return 0;
}

UINT WinSpyDlg_ExitSizeMove(HWND hwnd)
{
    RECT rect;
    UINT uLayout;

    static UINT uOldLayout = WINSPY_MINIMIZED;


    GetWindowRect(hwnd, &rect);

    szCurrent.cx = GetRectWidth(&rect);
    szCurrent.cy = GetRectHeight(&rect);

    fyMaxed = (szCurrent.cy > szMinimized.cy);
    fxMaxed = (szCurrent.cx >= szExpanded.cx);

    if (fyMaxed == FALSE)
    {
        uLayout = WINSPY_MINIMIZED;
    }
    else
    {
        if (fxMaxed)
            uLayout = WINSPY_EXPANDED;
        else
            uLayout = WINSPY_NORMAL;

        szLastMax = szCurrent;
    }

    SetSysMenuIconFromLayout(hwnd, uLayout);

    if (uLayout == WINSPY_EXPANDED && uOldLayout != WINSPY_EXPANDED)
    {
        WindowTree_Refresh(g_hCurWnd, FALSE);
    }

    GetPinnedPosition(hwnd, &g_opts.ptPinPos);

    uOldLayout = uLayout;

    return 0;
}

UINT_PTR WinSpyDlg_NCHitTest(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    UINT_PTR uHitTest;

    uHitTest = DefWindowProc(hwnd, WM_NCHITTEST, wParam, lParam);

    // Allow full-window dragging
    if (g_opts.fFullDragging &&    uHitTest == HTCLIENT)
        uHitTest = HTCAPTION;

    SetWindowLongPtr(hwnd, DWLP_MSGRESULT, uHitTest);
    return TRUE;
}

#define X_ZOOM_BORDER 8
#define Y_ZOOM_BORDER 8

BOOL WinSpy_ZoomTo(HWND hwnd, UINT uCorner)
{
    RECT rcDisplay;
    RECT rect;
    POINT ptPinPos;

    //SystemParametersInfo(SPI_GETWORKAREA, 0, &rcDisplay, FALSE);
    GetWindowRect(hwnd, &rect);
    GetWorkArea(&rect, &rcDisplay);

    switch (uCorner)
    {
    case PINNED_TOPLEFT:
        ptPinPos.x = rcDisplay.left + X_ZOOM_BORDER;
        ptPinPos.y = rcDisplay.top + Y_ZOOM_BORDER;
        break;

    case PINNED_TOPRIGHT:
        ptPinPos.x = rcDisplay.right - X_ZOOM_BORDER;
        ptPinPos.y = rcDisplay.top + Y_ZOOM_BORDER;
        break;

    case PINNED_BOTTOMRIGHT:
        ptPinPos.x = rcDisplay.right - X_ZOOM_BORDER;
        ptPinPos.y = rcDisplay.bottom - Y_ZOOM_BORDER;
        break;

    case PINNED_BOTTOMLEFT:
        ptPinPos.x = rcDisplay.left + X_ZOOM_BORDER;
        ptPinPos.y = rcDisplay.bottom - Y_ZOOM_BORDER;
        break;

    default:
        return FALSE;
    }

    SetPinState(TRUE);

    g_opts.ptPinPos = ptPinPos;
    g_opts.uPinnedCorner = uCorner;
    SetWindowLayout(hwnd, WINSPY_MINIMIZED);

    return TRUE;
}