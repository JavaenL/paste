#include <Windows.h>

enum class ExitReason : int
{
	Success,
	ClipboardError,
	NoTextualData,
	SystemError
};

enum class LineEnding : int
{
	AsIs,
	CrLf,
	Lf
};

template<typename T>
T* _malloc(DWORD count)
{
	DWORD bytes = sizeof(T) * count;
	return (T*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
}

template<typename T>
void _free(T *obj)
{
	HeapFree(GetProcessHeap(), 0, obj);
}

void Write(const wchar_t *text, DWORD outputHandle = STD_OUTPUT_HANDLE, DWORD length = -1)
{
	length = length != -1 ? length : lstrlen(text);

	HANDLE hOut = GetStdHandle(outputHandle);
	if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr)
	{
		ExitProcess((UINT)-1);
	}
	DWORD consoleMode;
	bool isConsole = GetConsoleMode(hOut, &consoleMode) != 0;

	DWORD result = 0;
	DWORD charsWritten = -1;
	if (isConsole)
	{
		result = WriteConsoleW(hOut, text, length, &charsWritten, nullptr);
	}
	else
	{
		//WSL fakes the console, and requires UTF8 output
		DWORD utf8ByteCount = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, length + 1, nullptr, 0, nullptr, nullptr); //include null
		auto utf8Bytes = _malloc<char>(utf8ByteCount);
		WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, utf8Bytes, utf8ByteCount, nullptr, nullptr);
		result = WriteFile(hOut, utf8Bytes, utf8ByteCount - 1 /* remove null */, &charsWritten, nullptr);
		if (charsWritten != utf8ByteCount - 1)
		{
			ExitProcess(GetLastError());
		}
		_free(utf8Bytes);
	}

	if (result == 0)
	{
		ExitProcess((UINT)GetLastError());
	}
}

void WriteError(const wchar_t *text)
{
	return Write(text, STD_ERROR_HANDLE);
}

bool ClipboardContainsFormat(UINT format)
{
	bool firstTime = true;
	for (UINT f = 0; firstTime || f != 0; f = EnumClipboardFormats(f))
	{
		firstTime = false;
		if (f == format)
		{
			return true;
		}
	}
	return false;
}

void print(const WCHAR *text, LineEnding lineEnding)
{
	if (text == nullptr || !*text) {
		return;
	}
	const WCHAR *ending = L"\n";
	switch (lineEnding)
	{
	case LineEnding::AsIs:
		Write(text);
		for (; *text; text++) {
			if (*text == L'\n' || *text == L'\r') {
				ending = *text == L'\n' ? L"\n" : text[1] == L'\n' ? L"\r\n" : L"\r";
				text++;
				break;
			}
		}
		break;
	case LineEnding::Lf:
		while (*text)
		{
			auto end = text;
			while (*end && *end != L'\r') { end++; }
			if (end > text) {
				Write(text, STD_OUTPUT_HANDLE, (int)(end - text));
			}
			if (*end) {
				end += end[1] == L'\n' ? 2 : 1;
				Write(L"\n", STD_OUTPUT_HANDLE, 1);
			}
			text = end;
		}
		break;
	case LineEnding::CrLf:
		ending = L"\r\n";
		while (*text)
		{
			auto end = text;
			while (*end && (*end == L'\n' ? end != text && end[-1] == L'\r' : *end != L'\r' || end[1] == L'\n')) { end++; }
			if (end > text) {
				Write(text, STD_OUTPUT_HANDLE, (int)(end - text));
			}
			if (*end) {
				end++;
				Write(L"\r\n", STD_OUTPUT_HANDLE, 2);
			}
			text = end;
		}
		break;
	}
	if (text[-1] != L'\n' && text[-1] != L'\r') {
		Write(ending, STD_OUTPUT_HANDLE);
	}
}

int wmain(void)
{
	int argc;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLine(), &argc);
	LineEnding lineEnding = LineEnding::AsIs;

	if (argc == 2)
	{
		if (lstrcmpi(argv[1], L"--lf") == 0)
		{
			lineEnding = LineEnding::Lf;
		}
		else if (lstrcmpi(argv[1], L"--crlf") == 0)
		{
			lineEnding = LineEnding::CrLf;
		}
	}

	if (!OpenClipboard(nullptr))
	{
		WriteError(L"Failed to open system clipboard!\n");
		ExitProcess((UINT)ExitReason::ClipboardError);
	}

	if (!ClipboardContainsFormat(CF_UNICODETEXT))
	{
		CloseClipboard();
		WriteError(L"Clipboard contains non-text data!\n");
		ExitProcess((UINT)ExitReason::NoTextualData);
	}

	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData == INVALID_HANDLE_VALUE || hData == nullptr)
	{
		CloseClipboard();
		WriteError(L"Unable to get clipboard data!\n");
		ExitProcess((UINT)ExitReason::ClipboardError);
	}

	const wchar_t *text = (const wchar_t *) GlobalLock(hData);
	if (text == nullptr)
	{
		CloseClipboard();
		WriteError(L"Unable to get clipboard data!\n");
		ExitProcess((UINT)ExitReason::ClipboardError);
	}

	print(text, lineEnding);

	GlobalUnlock(hData);
	CloseClipboard();

	ExitProcess((UINT)ExitReason::Success);
}
