/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    executable_path.cpp
 * authors: Codex
 * created: 2026-06-29
 * brief:   current executable path helpers
 */

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

#include "executable_path.h"

#include <filesystem>
#include <vector>

#if !defined(_WIN32)
  #include <unistd.h>
#endif

using namespace std;

static string PathFromArgv0(const string & argv0, const string & fallback_name)
{
  if (argv0.empty())
  {
    return fallback_name;
  }

#if defined(_WIN32)
  if ((argv0.find('/') == string::npos) && (argv0.find('\\') == string::npos))
#else
  if (argv0.find('/') == string::npos)
#endif
  {
    return argv0;
  }

  error_code ec;
  filesystem::path p = filesystem::absolute(argv0, ec);
  return (ec ? argv0 : p.lexically_normal().string());
}

#if defined(_WIN32)
static string Utf16ToUtf8(const wstring & text)
{
  if (text.empty())
  {
    return "";
  }

  int len = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                nullptr, 0, nullptr, nullptr);
  if (len <= 0)
  {
    return "";
  }

  string result(len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      result.data(), len, nullptr, nullptr);
  return result;
}
#endif

string CurrentExecutablePath(const string & argv0, const string & fallback_name)
{
#if defined(_WIN32)
  vector<wchar_t> buf(MAX_PATH);
  while (true)
  {
    DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (0 == len)
    {
      break;
    }

    if (len < buf.size())
    {
      return filesystem::path(Utf16ToUtf8(wstring(buf.data(), len))).lexically_normal().string();
    }

    buf.resize(buf.size() * 2);
  }
#else
  vector<char> buf(4096);
  ssize_t len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
  if (len > 0)
  {
    buf[len] = 0;
    return filesystem::path(buf.data()).lexically_normal().string();
  }
#endif

  return PathFromArgv0(argv0, fallback_name);
}

string CurrentExecutableDir(const string & argv0)
{
  string exe_path = CurrentExecutablePath(argv0, "");
  filesystem::path p(exe_path);
  if (!p.has_parent_path())
  {
    return "";
  }
  return p.parent_path().lexically_normal().string();
}
