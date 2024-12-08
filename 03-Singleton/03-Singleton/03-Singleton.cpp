// 03-Singleton.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "03-Singleton.h"
#include <windowsx.h>
#include <tchar.h>
#include <stddef.h>
#include <sddl.h>
#include <strsafe.h>
#include <WinUser.h>
#define MAX_LOADSTRING 100

inline void chSETDLGICONS(HWND hWnd, int idi) {
	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)
		LoadIcon((HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
			MAKEINTRESOURCE(idi)));
	SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)
		LoadIcon((HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
			MAKEINTRESOURCE(idi)));
}

#define chHANDLE_DLGMSG(hWnd, message, fn)                 \
   case (message): return (SetDlgMsgResult(hWnd, uMsg,     \
      HANDLE_##message((hWnd), (wParam), (lParam), (fn))))

HWND g_hDlg;

HANDLE g_hSingleton = NULL;
HANDLE g_hBoundary = NULL;
HANDLE g_hNameSpace = NULL;

BOOL g_bNamespaceOpened = FALSE;

PCTSTR g_szBoundary = TEXT("3-Boundary");
PCTSTR g_szNamespace = TEXT("3-NameSpace");

#define DETAILS_CTRL GetDlgItem(g_hDlg, IDC_EDIT_DETAILS)


void AddText(PCTSTR pszFormat, ...) {

	va_list argList;
	va_start(argList, pszFormat);

	TCHAR sz[20 * 1024];

	Edit_GetText(DETAILS_CTRL, sz, _countof(sz));
	_vstprintf_s(
		_tcschr(sz, TEXT('\0')), _countof(sz) - _tcslen(sz),
		pszFormat, argList);
	Edit_SetText(DETAILS_CTRL, sz);
	va_end(argList);
}

void Dlg_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	switch (id)
	{
	case IDOK:
	case IDCANCEL:
		EndDialog(hwnd, id);
		break;
	}
}


void CheckInstances()
{
	g_hBoundary = CreateBoundaryDescriptor(g_szBoundary, 0);

	BYTE localAdminSID[SECURITY_MAX_SID_SIZE];
	PSID pLocalAdminSID = &localAdminSID;
	DWORD cbSID = sizeof(localAdminSID);

	if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, pLocalAdminSID, &cbSID)) {
		AddText(TEXT("AddSIDToBoundaryDescriptor failed: %u\r\n"), GetLastError());
		return;
	}	

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = FALSE;

	if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
		TEXT("D:(A;;GA;;;BA)"),
		SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL
	))
	{
		AddText(TEXT("Security Descriptor creation failed: %u\r\n"), GetLastError());
		return;
	}

	g_hNameSpace = CreatePrivateNamespace(&sa, g_hBoundary, g_szNamespace);

	LocalFree(sa.lpSecurityDescriptor);

	DWORD dwLastError = GetLastError();

	if (g_hNameSpace == NULL)
	{
		if (dwLastError == ERROR_ACCESS_DENIED) {
			AddText(TEXT("Access denied when creating the namespace.\r\n"));
			AddText(TEXT("   You must be running as Administrator.\r\n\r\n"));
			return;
		}
		else {
			if (dwLastError == ERROR_ALREADY_EXISTS) {
				// If another instance has already created the namespace, 
				// we need to open it instead. 
				AddText(TEXT("CreatePrivateNamespace failed: %u\r\n"), dwLastError);
				g_hNameSpace = OpenPrivateNamespace(g_hBoundary, g_szNamespace);
				if (g_hNameSpace == NULL) {
					AddText(TEXT("   and OpenPrivateNamespace failed: %u\r\n"),
						dwLastError);
					return;
				}
				else {
					g_bNamespaceOpened = TRUE;
					AddText(TEXT("   but OpenPrivateNamespace succeeded\r\n\r\n"));
				}
			}
			else {
				AddText(TEXT("Unexpected error occured: %u\r\n\r\n"),
					dwLastError);
				return;
			}
		}
	}

	TCHAR szMutexName[64];
	StringCchPrintf(szMutexName, _countof(szMutexName), TEXT("%s\\%s"),
		g_szNamespace, TEXT("Singleton"));

	g_hSingleton = CreateMutex(NULL, FALSE, szMutexName);
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		// There is already an instance of this Singleton object
		AddText(TEXT("Another instance of Singleton is running:\r\n"));
		AddText(TEXT("--> Impossible to access application features.\r\n"));
	}
	else {
		// First time the Singleton object is created
		AddText(TEXT("First instance of Singleton:\r\n"));
		AddText(TEXT("--> Access application features now.\r\n"));
	}
}

BOOL Dlg_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam) {

	chSETDLGICONS(hwnd, IDI_SINGLETON);

	// Keep track of the main dialog window handle
	g_hDlg = hwnd;

	// Check whether another instance is already running
	CheckInstances();

	return(TRUE);
}

INT_PTR WINAPI Dlg_Proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	switch (uMsg) {
		chHANDLE_DLGMSG(hwnd, WM_COMMAND, Dlg_OnCommand);
		chHANDLE_DLGMSG(hwnd, WM_INITDIALOG, Dlg_OnInitDialog);
	}

	return(FALSE);
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPTSTR    lpCmdLine,
	int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Show main window 
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_SINGLETON), NULL, Dlg_Proc);

	// Don't forget to clean up and release kernel resources
	if (g_hSingleton != NULL) {
		CloseHandle(g_hSingleton);
	}

	if (g_hNameSpace != NULL) {
		if (g_bNamespaceOpened) {  // Open namespace
			ClosePrivateNamespace(g_hNameSpace, 0);
		}
		else { // Created namespace
			ClosePrivateNamespace(g_hNameSpace, PRIVATE_NAMESPACE_FLAG_DESTROY);
		}
	}

	if (g_hBoundary != NULL) {
		DeleteBoundaryDescriptor(g_hBoundary);
	}

	return(0);
}