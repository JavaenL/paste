#include <Windows.h>
#ifdef CopyMemory
#undef CopyMemory
#endif

#define USE_LOCAL_ERROR_MESSAGE 1
#define RDATA_SEG ".text"

enum class DataType : char
{
  UTF8 = 0,
  ANSII,
  UCS2,
};

HANDLE gHandle;
#define BufferedChars (4) // : at least ~4M UTF-8 chars
char gBuffer[1024 * 1024 * 3 * BufferedChars - 0x4000];
#define MAX_LOCAL_ERROR_MESSAGE_BYTES (64 * 1024)

#pragma section(RDATA_SEG)
__declspec(allocate(RDATA_SEG))
const char s_help[] = "[-a|--ansi[i] | -u|--ucs2|--unicode] <<< DATA\r\n\t[--] string to copy"
  , s_sys_err[] = "System Error: "
  , s_too_much[] = "Too much input!"
  , s_cleared[] = "clear."
  , s_xclip[] = { '\r', '\n', 'x', '-', 'c', 'l', 'i', 'p', ':', '\t' };

static __forceinline void _Write(DWORD handleId, const wchar_t *text, long long length)
{
  HANDLE handle = GetStdHandle(handleId);
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
  {
    return;
  }
  gHandle = handle;
  DWORD tmp = -1;
  if (GetConsoleMode(handle, &tmp) != 0)
  {
    WriteConsoleW(gHandle, text, (DWORD)length, &tmp, nullptr);
  }
  else
  {
    //WSL fakes the console, and requires UTF8 output
    char *const buffer2 = gBuffer;
    DWORD utf8ByteCount;
#if 1 || !USE_LOCAL_ERROR_MESSAGE
    {
      char *p2 = buffer2;
      for (int i = 0; i < length; i++) {
        unsigned short w = text[i];
#if USE_LOCAL_ERROR_MESSAGE
        if (w <= 0x7f) {
          *p2++ = (char)w;
        }
        else if (w <= 0x7ff) {
          *p2++ = 0xc0 | ((w >> 6) & 0x1f);
          *p2++ = 0x80 | (w & 0x3f);
        }
        else {
          if (0 && w >= 0xd800 && w < 0xdc00 && i < length - 1) {
            unsigned int point = ((w - 0xd800) << 10) | ((text[++i] - 0xdc00) & 0x3ff) + 0x10000;
            *p2++ = 0xf0 | ((point >> 18) & 0x08);
            *p2++ = 0x80 | ((point >> 12) & 0x3f);
            w = (unsigned short)point;
          }
          else
          {
            *p2++ = 0xe0 | ((w >> 12) & 0x0f);
          }
          *p2++ = 0x80 | ((w >> 6) & 0x3f);
          *p2++ = 0x80 | (w & 0x3f);
        }
#else
        *p2++ = (char)w;
#endif
      }
      utf8ByteCount = (int)(p2 - buffer2);
    }
#else 
    utf8ByteCount = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, (int)length, buffer2, MAX_LOCAL_ERROR_MESSAGE_BYTES / 2 * 3, nullptr, nullptr);
#endif
    WriteFile(gHandle, buffer2, utf8ByteCount, &tmp, nullptr);
  }
}

/*
wchar_t *str2wstr(wchar_t *dest, const char *src) {
  for (const unsigned char *p = (const unsigned char *)src; *p; ) {
    *dest++ = *p++;
  }
  return dest;
} //*/

static __declspec(noinline) void WriteOutput(const char *p0, DWORD handleId, int base_offset = 2, int tchar_len = 0) {
  const int i = sizeof(DWORD);
  wchar_t *dest = (wchar_t *)gBuffer, *start = dest;
  for (int i = base_offset; i < sizeof(s_xclip); ) {
    *dest++ = s_xclip[i++];
  }
  //#if USE_LOCAL_ERROR_MESSAGE
  // this line may make msvc generate smaller code
  //dest = (wchar_t *)((unsigned long long)dest & ~(unsigned long long)3);
  //#endif
  if (tchar_len > 0) {
    int i = 0;
    for (auto p = (const wchar_t *)p0; i < tchar_len; i++) {
      *dest++ = p[i];
    }
  }
  else {
    for (const unsigned char *p = (const unsigned char *)p0; *p; ) {
      *dest++ = *p++;
    }
    *dest++ = L'\r';
    *dest++ = L'\n';
  }
  _Write(handleId, start, dest - start);
}

extern "C" void cmain(void)
{
  const void *real_input = nullptr;
  int input_len = 0, input_utf8_len = 0;
  DataType dataType = DataType::UTF8;
  bool hasInput, succeed;
  {
    LPCWSTR p = GetCommandLineW();
    WCHAR chEnd = *p == L'"' ? L'"' : L' ';
    while (*++p != chEnd && *p != L'\0') {}
    if (*p == L'"') {
      p++;
    }
    for (; *p == L' '; p++) {}
    HANDLE stdin = GetStdHandle(STD_INPUT_HANDLE);
    gHandle = stdin;
    hasInput = stdin != nullptr && stdin != INVALID_HANDLE_VALUE;
    const wchar_t *start = p;
    if (*p == L'-') {
      p++;
      if (*p == L'-') { p++; }
      switch (*p) {
      case L'a': dataType = DataType::ANSII; break;
      case L'u': dataType = DataType::UCS2; break;
      case L' ': case L'\0':
        if (p - start == 2) { // 'xclip -- ...'
          start += *p == L' ' ? 3 : 2;
          p--;
          goto l_use_args_as_input;
        }
        // no break;
      default: goto l_print_help;
      }
    }
    else if (*p == L'/' && p[1] == L'?') {
      goto l_print_help;
    }
    else if (*p != L'\0') {
    l_use_args_as_input:
      dataType = DataType::UCS2;
      real_input = start;
      while (*++p) {}
      input_len = (int)((p - start) * 2);
      goto l_arg_parsed;
    }
    DWORD consoleMode;
    if (GetConsoleMode(stdin, &consoleMode) != FALSE) {
    l_print_help:
      WriteOutput(s_help, STD_OUTPUT_HANDLE);
      ExitProcess(0);
      return;
    }
  }
l_arg_parsed:
  if (real_input == nullptr && hasInput) {
    DWORD new_len = 0;
    int bin_max = (int)sizeof(gBuffer);
    char *input = gBuffer;
    real_input = input;
    while (bin_max > 0 && ReadFile(gHandle, input + input_len, bin_max, &new_len, NULL) != FALSE && new_len > 0) {
      input_len += new_len;
      bin_max -= new_len;
      new_len = 0;
    }
    if (bin_max - new_len <= 0) {
      WriteOutput((const char*)s_too_much, STD_ERROR_HANDLE, 0);
      ExitProcess(-1);
      return;
    }
    input_len += new_len;
    if (dataType == DataType::UTF8 && input_len > 0) {
      input_utf8_len = input_len;
      input_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char*)real_input, input_utf8_len, nullptr, 0) * 2;
      if (input_len <= 0) {
        goto l_print_sys_error;
      }
    }
  }

  succeed = OpenClipboard(nullptr) != FALSE;
  if (succeed)
  {
    EmptyClipboard();
  }
  if (!succeed) {
  }
  else if (input_len > 0)
  {
    const HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, input_len + 2);
    const bool isDataHandleValid = succeed = hData != INVALID_HANDLE_VALUE && hData != NULL;
    if (succeed)
    {
      char *output;
      succeed = (output = (char *)GlobalLock(hData)) != nullptr;
      if (succeed) {
        if (dataType == DataType::UTF8) {
          MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char*)real_input, input_utf8_len, (wchar_t *)output, input_len / 2);
        }
        else {
          for (int i = 0; i < input_len; i++) {
            output[i] = ((const char*)real_input)[i];
          }
        }
        char *p = output + input_len;
        // here strips tail /\r\n/
        if (dataType == DataType::ANSII) {
          while (*--p == '\n' || *p == '\r') {}
          p[1] = '\0';
        }
        else {
          do {
            p = (char*)((wchar_t *)p - 1);
          } while (*(wchar_t *)p == L'\n' || *(wchar_t *)p == L'\r');
          ((wchar_t *)p)[1] = L'\0';
        }
        //output[input_len] = '\0'; output[input_len + 1] = '\0';
        GlobalUnlock(hData);
        succeed = SetClipboardData(dataType == DataType::ANSII ? CF_TEXT : CF_UNICODETEXT, hData) != NULL;
      }
      if (!succeed) {
        GlobalFree(hData);
      }
    }
    CloseClipboard();
  }
  else {
    CloseClipboard();
    WriteOutput(s_cleared, STD_OUTPUT_HANDLE);
  }
  if (succeed)
  {
    ExitProcess(0);
    return;
  }
l_print_sys_error:
  int err = GetLastError();
#if USE_LOCAL_ERROR_MESSAGE
  wchar_t *msg_buffer = (wchar_t *)(gBuffer + MAX_LOCAL_ERROR_MESSAGE_BYTES / 2 * 3);
  int len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0, msg_buffer, MAX_LOCAL_ERROR_MESSAGE_BYTES / 2, nullptr);
  if (len > 0) {
    WriteOutput((const char*)msg_buffer, STD_ERROR_HANDLE, 0, len);
  }
#else
  unsigned int e = (unsigned)err;
  // format: "err: %d" ( len(%d) <= 10 )
  char msg[sizeof(s_sys_err) + 10], *const end = msg + sizeof(msg) / sizeof(msg[0]), *p = end;
  *--p = '\0';
  while (e > 0) {
    *--p = (unsigned char)((char)(e % 10) + '0');
    e /= 10;
  }
  char *start = p - sizeof(s_sys_err) + 1;
  for (int i = 0; i < sizeof(s_sys_err) - 1; i++) {
    start[i] = s_sys_err[i];
  }
  WriteOutput(start, STD_ERROR_HANDLE);
#endif
  ExitProcess(err);
}
