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

HANDLE hOut;
bool isConsole;
#define BufferedChars (1) // 1M chars
char utf8Bytes[1024 * 1024 * 3 * BufferedChars + 1024];

static const char *err_msgs = "Failed to open systemClipboard contains non-textDUnable to get clipboard data";
constexpr int ERR_LEN_1 = 21;
constexpr int ERR_LEN_2 = 27;
constexpr int ERR_LEN_3 = 13;
constexpr int ERR_LEN_4 = 10;
constexpr int ERR_LEN_5 = 5;
constexpr int ERR_OFFSET_1 = 0;
constexpr int ERR_OFFSET_2 = ERR_OFFSET_1 + ERR_LEN_1 + 0;
constexpr int ERR_OFFSET_3 = ERR_OFFSET_2 + ERR_LEN_2 + 1;
constexpr int ERR_OFFSET_4 = ERR_OFFSET_3 + ERR_LEN_3 + 0;
constexpr int ERR_OFFSET_5 = ERR_OFFSET_4 + ERR_LEN_4 + 0;

void setupOutput(DWORD outputHandle) {
	hOut = GetStdHandle(outputHandle);
	if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr)
	{
		ExitProcess((UINT)-1);
	}
	DWORD consoleMode;
	isConsole = GetConsoleMode(hOut, &consoleMode) != 0;
}
	

void Write(const wchar_t *text, int length)
{
	DWORD result = 0;
	DWORD charsWritten = -1;
	if (isConsole)
	{
		result = WriteConsoleW(hOut, text, (DWORD)length, &charsWritten, nullptr);
	  if (result == 0)
	  {
		  ExitProcess((UINT)-2); // GetLastError()
	  }
	}
	else
	{
		//WSL fakes the console, and requires UTF8 output
		const int step = 1024 * 1024 * BufferedChars;
		for (; length > 0; length -= step) {
			const auto part = length < step ? length : step;
			DWORD utf8ByteCount = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, part, utf8Bytes, step * 3, nullptr, nullptr);
			result = WriteFile(hOut, utf8Bytes, utf8ByteCount, &charsWritten, nullptr);
			text += part;
			if (charsWritten != utf8ByteCount)
			{
				ExitProcess((UINT)-2); // GetLastError()
			}
		}
	}
}

void ExitWithError(ExitReason err, const char *text, int size)
{
	setupOutput(STD_ERROR_HANDLE);
	wchar_t *dest = (wchar_t *)(utf8Bytes + 1024 * 1024 * 3 * BufferedChars), *p2 = dest;
	for (int i = 0; i < size; i++) {
		*p2++ = (unsigned short)(unsigned char)*text++;
	}
	size = *text == 'C' ? ERR_LEN_4 : *text == 'D' ? ERR_LEN_5 : 0;
	text = err_msgs + (*text == 'D' ? ERR_OFFSET_5 : ERR_OFFSET_4);
	for (int i = 0; i < size; i++) {
		*p2++ = (unsigned short)(unsigned char)*text++;
	}
	*p2++ = L'!';
	*p2++ = L'\n';
	size += 2;
	Write(dest, (DWORD)(p2 - dest));
	ExitProcess((UINT)err);
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
	setupOutput(STD_OUTPUT_HANDLE);
	WCHAR ending[2] = { L'\n', L'\0' };
	const auto start = text;
	switch (lineEnding)
	{
	case LineEnding::AsIs:
		for (; *text; text++) {
			if (*text == L'\n' || *text == L'\r') {
				if (*text == L'\r') {
					ending[0] = L'\r';
					if (text[1] == L'\n') {
						ending[1] = L'\n';
					}
				}
				break;
			}
		}
		for (; *text; text++) {}
		Write(start, (DWORD)(text - start));
		break;
	case LineEnding::CrLf:
		ending[0] = L'\r'; ending[1] = L'\n';
	case LineEnding::Lf:
		while (*text)
		{
			auto end = text;
			while (*end && (lineEnding == LineEnding::Lf ? *end != L'\r' : *end == L'\n' ? end != text && end[-1] == L'\r' : *end != L'\r' || end[1] == L'\n')) { end++; }
			if (end > text) {
				Write(text, (int)(end - text));
			}
			if (*end) {
				end += lineEnding == LineEnding::Lf && end[1] == L'\n' ? 2 : 1;
				Write(ending, ending[1] ? 2 : 1);
			}
			text = end;
		}
		break;
	}
	if (text[-1] != L'\n' && text[-1] != L'\r') {
		Write(ending, ending[1] ? 2 : 1);
	}
}

int wmain(void)
{
	LineEnding lineEnding = LineEnding::AsIs;
	{
		LPWSTR p = GetCommandLineW();
		if (*p == L'"') {
			while (*++p != L'"') {}
			p++;
		}
		else {
			while (*++p != L' ' && *p != L'\0') {}
		}
		for (; *p == L' '; p++) {}
		if (p[0] == L'-' && p[1] == L'-') {
			if ((p[2] | (short)32) == L'l' && (p[3] | (short)32) == L'f' && !p[4])
			{
				lineEnding = LineEnding::Lf;
			}
			else if ((p[2] | (short)32) == L'c' && (p[3] | (short)32) == L'r' && (p[4] | (short)32) == L'l' && (p[5] | (short)32) == L'f' && !p[6])
			{
				lineEnding = LineEnding::CrLf;
			}
		}
	}

	if (!OpenClipboard(nullptr))
	{
		ExitWithError(ExitReason::ClipboardError, err_msgs + ERR_OFFSET_1, ERR_LEN_1);
	}

	if (!ClipboardContainsFormat(CF_UNICODETEXT))
	{
		CloseClipboard();
		ExitWithError(ExitReason::NoTextualData,  err_msgs + ERR_OFFSET_2, ERR_LEN_2);
	}

	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	const wchar_t *text;
	if (hData == INVALID_HANDLE_VALUE || hData == nullptr || (text = (const wchar_t *)GlobalLock(hData)) == nullptr)
	{
		CloseClipboard();
		ExitWithError(ExitReason::ClipboardError, err_msgs + ERR_OFFSET_3, ERR_LEN_3 + ERR_LEN_4 + ERR_LEN_5);
	}

	print(text, lineEnding);

	GlobalUnlock(hData);
	CloseClipboard();

	ExitProcess((UINT)ExitReason::Success);
}
