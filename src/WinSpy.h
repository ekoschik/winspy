#ifndef WINSPY_INCLUDED
#define WINSPY_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <WindowsX.h>
#include <commctrl.h>
#include <stdlib.h>
#include <Strsafe.h>
#include <dwmapi.h>

#ifndef DWM_CLOAKED_APP
#define DWMWA_CLOAKED           14
#define DWM_CLOAKED_APP         0x00000001
#define DWM_CLOAKED_SHELL       0x00000002
#define DWM_CLOAKED_INHERITED   0x00000004
#endif

#define QUOTE(arg)          #arg
#define STRINGIZE(arg)      QUOTE(arg)

#define UNREFERENCED_PARAMETER(P)          (P)
#define IDM_WINSPY_ABOUT    100

//
//  Define a structure for each property page in
//  the main window
//
typedef struct
{
    HWND    hwnd;
    PCWSTR  szText;
    UINT    id;
    DLGPROC dlgproc;
} DialogTab;

extern DialogTab WinSpyTab[];

#define GENERAL_TAB        0
#define STYLE_TAB          1
#define PROPERTY_TAB       2
#define CLASS_TAB          3
#define WINDOW_TAB         4
#define PROCESS_TAB        5
#define DPI_TAB            6
#define NUMTABCONTROLITEMS 7

#define MAX_STYLE_NAME_CCH 60

//
//  Simple const value lookup.
//  Used for class styles and predefined color and brush values.
//
typedef struct
{
    PCWSTR szName;
    UINT value;
} ConstLookupType;

//
//  Handle-lookup
//
typedef struct
{
    PCWSTR szName;
    HANDLE handle;
} HandleLookupType;

//
//  Extended Style table. 1 per window class
//

//
// There are 2 sorts of "style constants":
// 1. Single style. This is a logical "property" of the window (e.g.: WS_OVERLAPPED).
// The presence of that property in a window's styles value is determined by a set of bits and a mask containing those bits.
// Essentially, by masking the styles value with the mask, we obtain an "enum" value, and this style represents one value for that enum.
// In most cases (e.g.: WS_MINIMIZE), that mask contains just one bit whose value of "1" indicates presence of this style
// and "0" - absence of this style. But sometimes, the "0" value has its own style constant (e.g.: SBS_HORZ).
// And sometimes, the mask contains more than 1 bit (e.g.: WS_OVERLAPPED).
// 2. Combination style. This is a constant that represents a set of several single styles with non-overlapping masks
// (e.g.: WS_OVERLAPPEDWINDOW). For such style to be present in the styles value, each of its components must be present.
// Therefore, its presence can be checked using the value that is the "|" of all contained styles' values
// and the mask that is the "|" of all contained styles' masks.
//
// Thus, the presence of any style in the styles value can be defined by 2 DWORD numbers: the mask and the matching value.
//
// Sometimes, applicability of a style depends on presence of another style.
// For instance, the same bit would mean WS_TABSTOP if WS_CHILD is present or WS_MAXIMIZEBOX if WS_SYSMENU is present
// (and, as experiment showed, even mean both if both those dependencies are present, which is obviously unintended and confusing).
// Hopefully we can find a dependency style (not necessarily one of the predefined constants)
// with no nesting dependencies for every style that we want to be able to display.
// This way, we can implement a smart "set the style" functionality: not only set the actual value of the style's bits,
// but also set its dependency's bits so that we make the style applicable and present at once.
// This way, a comprehensive definition of a style will contain:
// - style's value
// - style's mask
// - [optional] dependency style's value
// - [optional] dependency style's mask
//
// To simplify style definitions, the "extraMask" fields will contain the bits that need to be "|"'ed with the value
// in order to get the actual mask. This allows this field to default to 0 for the vast majority of typical cases
// where the mask is equal to the value.

typedef struct
{
    PCWSTR name;       // Textual name of style
    DWORD  value;      // The value of the style
    DWORD  extraMask;  // The extra bits determining the mask for testing presence of the style

    // This style is only applicable if the following style is present:
    DWORD dependencyValue;
    DWORD dependencyExtraMask;
} StyleLookupEx;

inline BOOL StyleApplicableAndPresent(DWORD value, StyleLookupEx *pStyle)
{
    if (((pStyle->dependencyValue | pStyle->dependencyExtraMask) & value) != pStyle->dependencyValue)
        return FALSE;
    return ((pStyle->value | pStyle->extraMask) & value) == pStyle->value;
}

// Because static_assert is a statement which is not an expression, it cannot be used where an expression is expected.
// Therefore, we define an expression loosely equivalent to a static_assert here
// which would cause a compilation error if the condition is false.
#define value_with_static_assert(value, condition) 1 ? (value) : (sizeof(int[condition]), (value))

#define HANDLE_(handle) L###handle, (HANDLE)handle

//
//  Use these helper macros to fill in the style structures.
//
#define CHECKEDNAMEANDVALUE_(name, value) L##name, value_with_static_assert((UINT)(value), ARRAYSIZE(name) < MAX_STYLE_NAME_CCH)
#define NAMEANDVALUE_(value) CHECKEDNAMEANDVALUE_(#value, value)

#define STYLE_MASK_DEPENDS(style, extraMask, dependencyStyle, dependencyExtraMask) CHECKEDNAMEANDVALUE_(#style, style), extraMask, dependencyStyle, dependencyExtraMask
#define STYLE_SIMPLE_DEPENDS(style, dependencyStyle) CHECKEDNAMEANDVALUE_(#style, style), 0, dependencyStyle, 0
#define STYLE_MASK(style, extraMask) CHECKEDNAMEANDVALUE_(#style, style), extraMask, 0, 0
#define STYLE_SIMPLE(style) CHECKEDNAMEANDVALUE_(#style, style), 0, 0, 0
#define STYLE_COMBINATION(style) CHECKEDNAMEANDVALUE_(#style, style), 0, 0, 0
#define STYLE_COMBINATION_MASK(style, extraMask) CHECKEDNAMEANDVALUE_(#style, style), extraMask, 0, 0

//
// Define some masks that are not defined in the Windows headers
//
#define WS_OVERLAPPED_MASK WS_OVERLAPPED | WS_POPUP | WS_CHILD // 0xC0000000
#define BS_TEXT_MASK BS_TEXT | BS_ICON | BS_BITMAP // 0x00C0
#define CBS_TYPE_MASK CBS_SIMPLE | CBS_DROPDOWN | CBS_DROPDOWNLIST //0x0003
#define SBS_DIR_MASK SBS_HORZ | SBS_VERT //0x0001
#define CCS_TOP_MASK 0x0003
#define DTS_FORMAT_MASK 0x000C

//
//  Use this structure to list each window class with its
//  associated style table and, optionally, a message to send to the window
//  to retrieve this set of control-specific extended styles
//
typedef struct
{
    PCWSTR          szClassName;
    StyleLookupEx  *stylelist;
    DWORD           dwMessage;
} ClassStyleLookup;

//
//  Useful functions!
//
BOOL FunkyList_MeasureItem(HWND hwnd, MEASUREITEMSTRUCT *mis);
BOOL FunkyList_DrawItem(HWND hwnd, UINT uCtrlId, DRAWITEMSTRUCT *dis);

//
//  WinSpy layout functions
//
void ToggleWindowLayout(HWND hwnd);
void SetWindowLayout(HWND hwnd, UINT uLayout);
UINT GetWindowLayout(HWND hwnd);
void SetInitialWindowPos(HWND hwnd, HWND hwndLaunchAbove);
void UpdateMainWindowText();
void GetWorkArea(RECT* prcWinRect, RECT* prcWorkArea);
void SetLastWorkAreaAndDpi(HWND hwnd);

#define WINSPY_LAYOUT_NO 0
#define WINSPY_MINIMIZED 1
#define WINSPY_NORMAL    2
#define WINSPY_EXPANDED  3
#define WINSPY_LASTMAX   4  // Only use with SetWindowLayout
// (chooses between normal/expanded)

//
//  WinSpy message handler functions
//

UINT WinSpyDlg_Size(HWND hwnd, WPARAM wParam, LPARAM lParam);
UINT WinSpyDlg_Sizing(UINT nSide, RECT *prc);
UINT WinSpyDlg_WindowPosChanged(HWND hwnd, WINDOWPOS *wp);
UINT WinSpyDlg_EnterSizeMove(HWND hwnd);
UINT WinSpyDlg_ExitSizeMove(HWND hwnd);
UINT_PTR WinSpyDlg_NCHitTest(HWND hwnd, WPARAM wParam, LPARAM lParam);

UINT WinSpyDlg_CommandHandler(HWND hwnd, WPARAM wParam, LPARAM lParam);
UINT WinSpyDlg_SysMenuHandler(HWND hwnd, WPARAM wParam, LPARAM lParam);
UINT WinSpyDlg_NCDoubleClick(HWND hwnd, WPARAM wParam);
UINT WinSpyDlg_TimerHandler(UINT_PTR uTimerId);

void WinSpyDlg_SizeContents(HWND hwnd);

UINT WinSpy_PopupCommandHandler(HWND hwndDlg, UINT uCmdId, HWND hwndTarget);
void WinSpy_SetupPopupMenu(HMENU hMenu, HWND hwndTarget);


int WINAPI GetRectWidth(RECT *rect);
int WINAPI GetRectHeight(RECT *rect);

BOOL IsWindowMinimized(HWND hwnd);

//
//  Dialog box procedures for each dialog tab.
//
INT_PTR CALLBACK GeneralDlgProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK StyleDlgProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK WindowDlgProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK PropertyDlgProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ProcessDlgProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ClassDlgProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DpiDlgProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

// Top-level
void DisplayWindowInfo(HWND hwnd);

void UpdateActiveTab();
void UpdateWindowTab(HWND hwnd);
void UpdateClassTab(HWND hwnd);
void UpdateStyleTab(HWND hwnd);
void UpdateGeneralTab(HWND hwnd);
void UpdatePropertyTab(HWND hwnd);
void UpdateProcessTab(HWND hwnd, DWORD dwOverridePID);
void UpdateDpiTab(HWND hwnd);

void UpdateScrollbarInfo(HWND hwnd);
void UpdateWndProcControls(HWND hwnd, HWND hwndDlg, PVOID clsproc);

void GetRemoteInfo();

void ExitWinSpy(HWND hwnd, UINT uCode);

//
//  Menu and System Menu functions
//
//
void CheckSysMenu(HWND hwnd, UINT uItemId, BOOL fChecked);
void SetSysMenuIconFromLayout(HWND hwnd, UINT layout);

void ShowEditSizeDlg(HWND hwndParent, HWND hwndTarget);
void ShowPosterDlg(HWND hwndParent, HWND hwndTarget);
void ShowBroadcasterDlg(HWND hwndParent);
void ShowWindowStyleEditor(HWND hwndParent, HWND hwndTarget, BOOL fExtended);
void ShowWindowPropertyEditor(HWND hwndParent, HWND hwndTarget, BOOL bAddNew);
void ShowOptionsDlg(HWND hwndParent);
void ShowAboutDlg(HWND hwndParent);

void LoadSettings(void);
void SaveSettings(void);

BOOL GetRemoteWindowInfo(HWND hwnd, WNDCLASSEX *pClass,
    WNDPROC *pProc, WCHAR *pszText, int nTextLen);

BOOL RemoveTabCtrlFlicker(HWND hwndTab);

void VerboseClassName(WCHAR ach[], size_t cch, WORD atom);

void InitStockStyleLists();
BOOL GetProcessNameByPid(DWORD dwProcessId, WCHAR szName[], DWORD nNameSize, WCHAR szPath[], DWORD nPathSize);

void ShowProcessContextMenu(HWND hwndParent, INT x, INT y, BOOL fForButton, HWND hwnd, DWORD dwProcessId);

//
//  Pinned-window support
//
void GetPinnedPosition(HWND, POINT *);
BOOL WinSpy_ZoomTo(HWND hwnd, UINT uCorner);
void SetPinState(BOOL fPinned);

//
//  Pinned-window constants
//
#define PINNED_TOPLEFT      0       // notice the bit-positions:
#define PINNED_TOPRIGHT     1       // (bit 0 -> 0 = left, 1 = right)
#define PINNED_BOTTOMRIGHT  3       // (bit 1 -> 0 = top,  1 = bottom)
#define PINNED_BOTTOMLEFT   2

#define PINNED_NONE         PINNED_TOPLEFT
#define PINNED_LEFT         0
#define PINNED_RIGHT        1
#define PINNED_TOP          0
#define PINNED_BOTTOM       2

//
//  Global variables!! These just control WinSpy behavior
//
typedef struct
{
    BOOL  fAlwaysOnTop;
    BOOL  fClassThenText;
    BOOL  fEnableToolTips;
    BOOL  fFullDragging;
    BOOL  fMinimizeWinSpy;
    BOOL  fPinWindow;
    BOOL  fShowDimmed;
    BOOL  fShowHidden;
    BOOL  fShowInCaption;
    BOOL  fSaveWinPos;
    BOOL  fShowDesktopRoot;
    UINT  uTreeInclude;
    BOOL  fShowHiddenInList;

    // todo...
    UINT  uPinnedCorner;
    POINT ptPinPos;

    // When the window is closed we store the work area and window DPI
    // at the time. This is used later if using the ptPinPos stored in
    // the registry to position the initial window (so that it launches
    // where it was last closed).
    POINT ptLastClose;
    POINT ptWorkAreaOrigin;
    UINT  lastWindowDpi;

} Options;

extern Options g_opts;

//
//  Application global variables
//
extern HINSTANCE g_hInst;

#define szAppName L"WinSpy++"

#define szInvalidWindow L"(invalid window)"

extern HWND  g_hwndPin;       // Toolbar with pin bitmap
extern HWND  g_hwndSizer;     // Sizing grip for bottom-right corner
extern HWND  g_hwndToolTip;   // tooltip for main window controls only

//
//  Spy-window globals
//
//
extern HWND       g_hCurWnd;
extern WNDPROC    g_WndProc;
extern BOOL       g_fPassword;
extern BOOL       g_fTriedRemote;
extern WCHAR      g_szPassword[];
extern WCHAR      g_szClassName[];

extern DWORD      g_dwSelectedPID;
extern BOOL       g_fShowClientRectAsMargins;

//
//  Useful SetWindowPos constants (saves space!)
//
#define SWP_SIZEONLY  (SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)
#define SWP_MOVEONLY  (SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)
#define SWP_ZONLY     (SWP_NOSIZE | SWP_NOMOVE   | SWP_NOACTIVATE)
#define SWP_SHOWONLY  (SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE | SWP_SHOWWINDOW)
#define SWP_HIDEONLY  (SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE | SWP_HIDEWINDOW)

//
//  Window tree
//

#define WINLIST_INCLUDE_HANDLE  1       // handle, classname
#define WINLIST_INCLUDE_CLASS   2       // classname, caption
#define WINLIST_INCLUDE_ALL     3       // handle, caption, classname

void WindowTree_Initialize(HWND hwndTree);
void WindowTree_Destroy();
void WindowTree_Refresh(HWND hwndToSelect, BOOL fSetFocus);
void WindowTree_OnRightClick(NMHDR *pnm);
void WindowTree_OnSelectionChanged(NMHDR *pnm);
void WindowTree_Locate(HWND hwnd);
HWND WindowTree_GetSelectedWindow();
void WindowTree_RefreshWindowNode(HWND hwnd);


//
// DPI related helpers.
//

int DPIScale(HWND hwnd, int value);
void MarkProcessAsPerMonitorDpiAware();
void MarkProcessAsSystemDpiAware();
UINT GetDpiForWindow(HWND hwnd);

#ifdef __cplusplus
}
#endif

#endif