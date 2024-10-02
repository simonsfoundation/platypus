#include <windows.h>

static HHOOK s_msgBoxHook;
static const char *s_msgBoxOK;
static const char *s_msgBoxCancel;

enum
{
	kMsgBox_YES = IDYES,
	kMsgBox_NO = IDNO
};

static LRESULT CALLBACK MessageBoxProc(INT nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HCBT_ACTIVATE)
	{
		HWND hChild = (HWND)wParam;
		RECT windowRect;
		::GetWindowRect(hChild, &windowRect);

		if (s_msgBoxOK)
		{
			HWND item = ::GetDlgItem(hChild, IDYES);
			::SetDlgItemTextA(hChild, IDYES, s_msgBoxOK);
		}

		if (s_msgBoxCancel)
		{
			HWND item = ::GetDlgItem(hChild, IDNO);
			::SetDlgItemTextA(hChild, IDNO, s_msgBoxCancel);
		}

		::UnhookWindowsHookEx(s_msgBoxHook);
	}
	else
		CallNextHookEx(s_msgBoxHook, nCode, wParam, lParam);
	return 0;
}

extern "C" int messageBox(void *parentPtr, const char *product, const char *msg, const char *okText, const char *cancelText)
{
	HWND parent = (HWND)parentPtr;

	s_msgBoxOK = okText;
	s_msgBoxCancel = cancelText;

	if (okText || cancelText)
	{
		s_msgBoxHook = ::SetWindowsHookEx(WH_CBT, MessageBoxProc, 0, GetCurrentThreadId());

		return ::MessageBoxA(parent, msg, product, MB_YESNO | MB_ICONINFORMATION);
	}

	return ::MessageBoxA(parent, msg, product, MB_OK | MB_ICONEXCLAMATION);
}


