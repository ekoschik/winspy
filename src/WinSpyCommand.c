//
//  WinSpyCommand.c
//
//  Copyright (c) 2002 by J Brown
//  Freeware
//
//  Menu / Control Command handler
//

#include "WinSpy.h"

#include "resource.h"
#include "Utils.h"
#include "FindTool.h"
#include "CaptureWindow.h"

void SetPinState(BOOL fPinned)
{
    g_opts.fPinWindow = fPinned;

    SendMessage(g_hwndPin, TB_CHANGEBITMAP, IDM_WINSPY_PIN,
        MAKELPARAM(fPinned, 0));

    SendMessage(g_hwndPin, TB_CHECKBUTTON, IDM_WINSPY_PIN,
        MAKELPARAM(fPinned, 0));

}

UINT WinSpyDlg_CommandHandler(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    NMHDR       hdr;
    UINT        uLayout;

    HWND hwndGeneral;
    HWND hwndFocus;
    HWND hwndCtrl;

    switch (LOWORD(wParam))
    {
    case IDM_WINSPY_ONTOP:

        if (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST)
        {
            // Not top-most any more
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_ZONLY);
        }
        else
        {
            // Make top-most (float above all other windows)
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_ZONLY);
        }

        return TRUE;

    case IDM_WINSPY_TOGGLE:
        ToggleWindowLayout(hwnd);
        return TRUE;

    case IDM_WINSPY_TOGGLEEXP:

        uLayout = GetWindowLayout(hwnd);

        if (uLayout == WINSPY_EXPANDED)
            SetWindowLayout(hwnd, WINSPY_MINIMIZED);

        else if (uLayout == WINSPY_NORMAL)
            SetWindowLayout(hwnd, WINSPY_EXPANDED);

        else if (uLayout == WINSPY_MINIMIZED)
            SetWindowLayout(hwnd, WINSPY_NORMAL);

        return TRUE;

    case IDM_WINSPY_ZOOMTL:
        WinSpy_ZoomTo(hwnd, PINNED_TOPLEFT);
        return TRUE;

    case IDM_WINSPY_ZOOMTR:
        WinSpy_ZoomTo(hwnd, PINNED_TOPRIGHT);
        return TRUE;

    case IDM_WINSPY_ZOOMBR:
        WinSpy_ZoomTo(hwnd, PINNED_BOTTOMRIGHT);
        return TRUE;

    case IDM_WINSPY_ZOOMBL:
        WinSpy_ZoomTo(hwnd, PINNED_BOTTOMLEFT);
        return TRUE;

    case IDM_WINSPY_REFRESH:
        DisplayWindowInfo(g_hCurWnd);
        return TRUE;

    case IDM_WINSPY_OPTIONS:
        ShowOptionsDlg(hwnd);
        return TRUE;

    case IDM_WINSPY_PIN:

        g_opts.fPinWindow = !g_opts.fPinWindow;

        SendMessage(g_hwndPin, TB_CHANGEBITMAP, IDM_WINSPY_PIN,
            MAKELPARAM(g_opts.fPinWindow, 0));

        // if from an accelerator, then we have to manually check the
        if (HIWORD(wParam) == 1)
        {
            SendMessage(g_hwndPin, TB_CHECKBUTTON, IDM_WINSPY_PIN,
                MAKELPARAM(g_opts.fPinWindow, 0));
        }

        GetPinnedPosition(hwnd, &g_opts.ptPinPos);
        return TRUE;

    case IDC_HIDDEN:
        g_opts.fShowHidden = IsDlgButtonChecked(hwnd, IDC_HIDDEN);
        return TRUE;

    case IDC_MINIMIZE:
        g_opts.fMinimizeWinSpy = IsDlgButtonChecked(hwnd, IDC_MINIMIZE);
        return TRUE;

    case IDM_GOTO_TAB_GENERAL:
    case IDM_GOTO_TAB_STYLES:
    case IDM_GOTO_TAB_PROPERTIES:
    case IDM_GOTO_TAB_CLASS:
    case IDM_GOTO_TAB_WINDOWS:
    case IDM_GOTO_TAB_PROCESS:
    case IDM_GOTO_TAB_DPI:

        // Simulate the tab-control being clicked
        hdr.hwndFrom = GetDlgItem(hwnd, IDC_TAB1);
        hdr.idFrom = IDC_TAB1;
        hdr.code = TCN_SELCHANGE;

        TabCtrl_SetCurSel(hdr.hwndFrom, LOWORD(wParam) - IDM_GOTO_TAB_GENERAL);

        SendMessage(hwnd, WM_NOTIFY, 0, (LPARAM)&hdr);

        return TRUE;

    case IDC_FLASH:

        HWND hwndSelected = WindowTree_GetSelectedWindow();

        if (hwndSelected)
        {
            FlashWindowBorder(hwndSelected);
        }

        return TRUE;

    case IDC_EXPAND:

        if (GetWindowLayout(hwnd) == WINSPY_NORMAL)
        {
            WindowTree_Refresh(g_hCurWnd, FALSE);
            SetWindowLayout(hwnd, WINSPY_EXPANDED);
        }
        else
        {
            SetWindowLayout(hwnd, WINSPY_NORMAL);
        }

        return TRUE;

    case IDC_CAPTURE:
        CaptureWindow(hwnd, g_hCurWnd);
        MessageBox(hwnd, L"Window contents captured to clipboard", szAppName, MB_ICONINFORMATION);
        return TRUE;

    case IDC_AUTOUPDATE:
        if (IsDlgButtonChecked(hwnd, IDC_AUTOUPDATE))
            SetTimer(hwnd, 0, 1000, NULL);
        else
            KillTimer(hwnd, 0);
        return TRUE;

    case IDOK:

        hwndGeneral = WinSpyTab[GENERAL_TAB].hwnd;
        hwndFocus = GetFocus();

        if (hwndFocus == GetDlgItem(hwndGeneral, IDC_HANDLE))
        {
            hwndCtrl = (HWND)GetDlgItemBaseInt(hwndGeneral, IDC_HANDLE, 16);

            if (IsWindow(hwndCtrl))
            {
                g_hCurWnd = hwndCtrl;
                DisplayWindowInfo(g_hCurWnd);
            }

            return 0;
        }
        else if (hwndFocus == GetDlgItem(hwndGeneral, IDC_CAPTION1) ||
            hwndFocus == GetWindow(GetDlgItem(hwndGeneral, IDC_CAPTION2), GW_CHILD))
        {
            PostMessage(hwndGeneral, WM_COMMAND, MAKEWPARAM(IDC_SETCAPTION, BN_CLICKED), 0);
            return FALSE;
        }


        if (GetFocus() != (HWND)lParam)
            return FALSE;

        ExitWinSpy(hwnd, 0);

        return TRUE;

    case IDC_LOCATE:

        if (g_hCurWnd)
        {
            WindowTree_Locate(g_hCurWnd);
        }

        return TRUE;

    case IDC_REFRESH:

        WindowTree_Refresh(g_hCurWnd, TRUE);

        return TRUE;
    }

    return FALSE;
}

UINT WinSpyDlg_NCDoubleClick(HWND hwnd, WPARAM wParam)
{
    // Double clicking on the title bar toggles the layout ('min'/'max' mode).
    if (wParam == HTCAPTION)
    {
        ToggleWindowLayout(hwnd);
    }

    // Respond non-zero (handled).
    // We do not want default processing, which includes double clicking
    // top and bottom resize borders to 'vertical maximize' (extends to
    // top and bottom of work area).
    return 1;
}

// todo, win shift up, win left, are still arranging the window.
// this is happening bc window has WS_THICKFRAME...

UINT WinSpyDlg_SysMenuHandler(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    switch (wParam & 0xFFF0)
    {
    case SC_RESTORE:

        if (IsWindowMinimized(hwnd))
        {
            break;
        }
        else
        {
            ToggleWindowLayout(hwnd);
            return 1;
        }

    case SC_MAXIMIZE:
        ToggleWindowLayout(hwnd);
        return 1;
    }

    switch (wParam)
    {
    case IDM_WINSPY_ABOUT:
        ShowAboutDlg(hwnd);
        return TRUE;

    case IDM_WINSPY_OPTIONS:
        ShowOptionsDlg(hwnd);
        return TRUE;

    case IDM_WINSPY_BROADCASTER:
        ShowBroadcasterDlg(hwnd);
        return TRUE;

    case IDM_WINSPY_ONTOP:
        PostMessage(hwnd, WM_COMMAND, wParam, lParam);
        return TRUE;

    case IDM_WINSPY_TOGGLEEXP:
        WinSpyDlg_CommandHandler(hwnd, wParam, lParam);
        break;
    }
    return FALSE;

}

UINT WinSpyDlg_TimerHandler(UINT_PTR uTimerId)
{
    if (uTimerId == 0)
    {
        DisplayWindowInfo(g_hCurWnd);
        return TRUE;
    }

    return FALSE;
}

void ShowAboutDlg(HWND hwndParent)
{
    CHAR  szText[400];
    CHAR  szTitle[60];
    WCHAR szVersion[40];
    WCHAR szCurExe[MAX_PATH];

    GetModuleFileName(0, szCurExe, MAX_PATH);
    GetVersionString(szCurExe, TEXT("FileVersion"), szVersion, 40);

    sprintf_s(szText, ARRAYSIZE(szText),
        "%S v%S\n"
        "\n"
        "Original version:\n"
        "    Copyright(c) 2002 - 2012 by Catch22 Productions\n"
        "    Written by J Brown\n"
        "    www.catch22.net | github.com/strobejb/winspy\n"
        "\n"
        "Forked and improved by various contributors:\n"
        "    github.com/m417z/winspy\n"
#ifdef WINSPY_GITHUB_FORK
        "\n"
        "This binary was built from this fork:\n"
        "    " STRINGIZE(WINSPY_GITHUB_FORK) " (on " __DATE__ ")"
#endif
        "",
        szAppName, szVersion);

    sprintf_s(szTitle, ARRAYSIZE(szTitle), "About %S", szAppName);

    MessageBoxA(hwndParent, szText, szTitle, MB_OK | MB_ICONINFORMATION);
}

