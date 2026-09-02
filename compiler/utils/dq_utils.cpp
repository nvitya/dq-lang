/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dq_utils.cpp
 * authors: nvitya
 * created: 2026-09-02
 * brief:   Generic utilities used by the DQ compiler and tools
 */

#include "dq_utils.h"

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

string JsonEscape(string_view text)
{
  string result;
  for (unsigned char c : text)
  {
    switch (c)
    {
      case '"': result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\b': result += "\\b"; break;
      case '\f': result += "\\f"; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default:
        if (c < 0x20) result += format("\\u{:04x}", unsigned(c));
        else result.push_back(char(c));
        break;
    }
  }
  return result;
}

filesystem::path AbsNormPath(const filesystem::path & path)
{
  error_code ec;
  filesystem::path result = filesystem::absolute(path, ec);
  if (ec) result = path;
  return result.lexically_normal();
}

int64_t FileTimeTicks(filesystem::file_time_type filetime)
{
  return int64_t(chrono::duration_cast<chrono::nanoseconds>(filetime.time_since_epoch()).count());
}

#if defined(_WIN32)
string WindowsErrorMessage(uint32_t error)
{
  LPSTR msg = nullptr;
  DWORD len = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                             | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, DWORD(error), 0, reinterpret_cast<LPSTR>(&msg), 0, nullptr);

  string result = (len && msg ? string(msg, len) : format("Windows error {}", error));
  if (msg) LocalFree(msg);
  while (!result.empty() && ((result.back() == '\n') || (result.back() == '\r')))
  {
    result.pop_back();
  }
  return result;
}
#endif
