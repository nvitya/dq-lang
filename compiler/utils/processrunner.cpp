/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    processrunner.cpp
 * authors: Codex, nvitya
 * created: 2026-03-17
 * brief:
 */

#include <array>
#include <algorithm>
#include <chrono>
#include <stdexcept>

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <poll.h>

  #include <errno.h>
  #include <fcntl.h>
  #include <signal.h>
  #include <string.h>
  #include <sys/wait.h>
  #include <unistd.h>
#endif

#include "processrunner.h"

using namespace std;

#if defined(_WIN32)

static string WindowsErrorMessage(DWORD err)
{
  LPSTR msg = nullptr;
  DWORD len = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                             | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, err, 0, reinterpret_cast<LPSTR>(&msg), 0, nullptr);

  string result = (len && msg ? string(msg, len) : string("Windows error ") + to_string(err));
  if (msg)
  {
    LocalFree(msg);
  }
  while (!result.empty() && ((result.back() == '\n') || (result.back() == '\r')))
  {
    result.pop_back();
  }
  return result;
}

static wstring Utf8ToWide(const string & text)
{
  if (text.empty())
  {
    return L"";
  }

  int len = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (len <= 0)
  {
    return wstring(text.begin(), text.end());
  }

  wstring result(len, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), len);
  return result;
}

static string QuoteWindowsArg(const string & arg)
{
  if (arg.empty())
  {
    return "\"\"";
  }

  bool needs_quotes = (arg.find_first_of(" \t\n\v\"") != string::npos);
  if (!needs_quotes)
  {
    return arg;
  }

  string result = "\"";
  size_t backslashes = 0;
  for (char c : arg)
  {
    if ('\\' == c)
    {
      ++backslashes;
      continue;
    }

    if ('"' == c)
    {
      result.append(backslashes * 2 + 1, '\\');
      result.push_back(c);
      backslashes = 0;
      continue;
    }

    result.append(backslashes, '\\');
    backslashes = 0;
    result.push_back(c);
  }

  result.append(backslashes * 2, '\\');
  result.push_back('"');
  return result;
}

static string BuildWindowsCommandLine(const vector<string> & args)
{
  string result;
  for (size_t i = 0; i < args.size(); ++i)
  {
    if (i > 0)
    {
      result.push_back(' ');
    }
    result += QuoteWindowsArg(args[i]);
  }
  return result;
}

static bool ReadAvailablePipe(HANDLE pipe_handle, string * dst, bool * eof, bool process_done)
{
  DWORD available = 0;
  if (!PeekNamedPipe(pipe_handle, nullptr, 0, nullptr, &available, nullptr))
  {
    DWORD err = GetLastError();
    if ((ERROR_BROKEN_PIPE == err) || (ERROR_HANDLE_EOF == err))
    {
      *eof = true;
      return true;
    }
    *eof = true;
    return false;
  }

  if (0 == available)
  {
    if (process_done)
    {
      *eof = true;
    }
    return true;
  }

  array<char, 4096> buf;
  DWORD to_read = min<DWORD>(available, static_cast<DWORD>(buf.size()));
  DWORD got = 0;
  if (!ReadFile(pipe_handle, buf.data(), to_read, &got, nullptr))
  {
    DWORD err = GetLastError();
    if ((ERROR_BROKEN_PIPE == err) || (ERROR_HANDLE_EOF == err))
    {
      *eof = true;
      return true;
    }
    *eof = true;
    return false;
  }

  if (got > 0)
  {
    dst->append(buf.data(), got);
  }

  return true;
}

static void CloseHandleNoError(HANDLE & handle)
{
  if (handle && (INVALID_HANDLE_VALUE != handle))
  {
    CloseHandle(handle);
    handle = nullptr;
  }
}

OProcessRunner::OProcessRunner()
{
}

OProcessRunner::~OProcessRunner()
{
}

bool OProcessRunner::Run()
{
  exit_code = -1;
  duration_us = 0;
  stdout_text.clear();
  stderr_text.clear();
  cmdline = BuildCmdLine();

  if (args.empty())
  {
    exit_code = PROCRUNERR_INVALID_ARGS;
    return false;
  }

  SECURITY_ATTRIBUTES sa = {};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE out_read = nullptr;
  HANDLE out_write = nullptr;
  HANDLE err_read = nullptr;
  HANDLE err_write = nullptr;

  if (!CreatePipe(&out_read, &out_write, &sa, 0))
  {
    exit_code = PROCRUNERR_PIPE_CREATE;
    return false;
  }
  if (!SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0))
  {
    CloseHandleNoError(out_read);
    CloseHandleNoError(out_write);
    exit_code = PROCRUNERR_SETUP;
    return false;
  }

  if (!CreatePipe(&err_read, &err_write, &sa, 0))
  {
    CloseHandleNoError(out_read);
    CloseHandleNoError(out_write);
    exit_code = PROCRUNERR_PIPE_CREATE;
    return false;
  }
  if (!SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0))
  {
    CloseHandleNoError(out_read);
    CloseHandleNoError(out_write);
    CloseHandleNoError(err_read);
    CloseHandleNoError(err_write);
    exit_code = PROCRUNERR_SETUP;
    return false;
  }

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  si.hStdOutput = out_write;
  si.hStdError = err_write;

  PROCESS_INFORMATION pi = {};
  string cmd = BuildWindowsCommandLine(args);
  wstring wcmd = Utf8ToWide(cmd);
  wstring wworkdir = Utf8ToWide(workdir);

  auto start_tp = chrono::steady_clock::now();

  BOOL created = CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, TRUE, 0, nullptr,
                                workdir.empty() ? nullptr : wworkdir.c_str(), &si, &pi);

  CloseHandleNoError(out_write);
  CloseHandleNoError(err_write);

  if (!created)
  {
    stderr_text = WindowsErrorMessage(GetLastError());
    CloseHandleNoError(out_read);
    CloseHandleNoError(err_read);
    exit_code = PROCRUNERR_EXEC;
    return false;
  }

  bool out_eof = false;
  bool err_eof = false;
  bool timed_out = false;
  bool read_failed = false;
  bool process_done = false;

  while (!process_done || !out_eof || !err_eof)
  {
    DWORD wait_ms = 10;
    if (!process_done && (exec_timeout_ms >= 0))
    {
      auto now_tp = chrono::steady_clock::now();
      auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(now_tp - start_tp).count();
      int64_t remaining_ms = int64_t(exec_timeout_ms) - elapsed_ms;
      if (remaining_ms <= 0)
      {
        TerminateProcess(pi.hProcess, 1);
        timed_out = true;
        process_done = true;
      }
      else
      {
        wait_ms = static_cast<DWORD>(min<int64_t>(remaining_ms, 10));
      }
    }

    if (!process_done)
    {
      DWORD wr = WaitForSingleObject(pi.hProcess, wait_ms);
      if (WAIT_OBJECT_0 == wr)
      {
        process_done = true;
      }
      else if (WAIT_FAILED == wr)
      {
        TerminateProcess(pi.hProcess, 1);
        read_failed = true;
        process_done = true;
      }
    }

    if (!out_eof && !ReadAvailablePipe(out_read, &stdout_text, &out_eof, process_done))
    {
      read_failed = true;
    }
    if (!err_eof && !ReadAvailablePipe(err_read, &stderr_text, &err_eof, process_done))
    {
      read_failed = true;
    }
  }

  DWORD child_exit = 0;
  if (!GetExitCodeProcess(pi.hProcess, &child_exit))
  {
    exit_code = PROCRUNERR_WAIT;
    if (stderr_text.empty())
    {
      stderr_text = WindowsErrorMessage(GetLastError());
    }
    CloseHandleNoError(pi.hThread);
    CloseHandleNoError(pi.hProcess);
    CloseHandleNoError(out_read);
    CloseHandleNoError(err_read);
    return false;
  }

  CloseHandleNoError(pi.hThread);
  CloseHandleNoError(pi.hProcess);
  CloseHandleNoError(out_read);
  CloseHandleNoError(err_read);

  auto end_tp = chrono::steady_clock::now();
  duration_us = chrono::duration_cast<chrono::microseconds>(end_tp - start_tp).count();

  exit_code = static_cast<int>(child_exit);

  if (timed_out)
  {
    exit_code = PROCRUNERR_TIMEOUT;
    if (stderr_text.empty())
    {
      stderr_text = "Process execution timed out.";
    }
    return false;
  }

  if (read_failed)
  {
    exit_code = PROCRUNERR_POLL;
    if (stderr_text.empty())
    {
      stderr_text = "Process output read failed.";
    }
    return false;
  }

  return true;
}

string OProcessRunner::BuildCmdLine()
{
  string s;

  for (size_t i = 0; i < args.size(); ++i)
  {
    if (i > 0)
    {
      s += " ";
    }

    s += args[i];
  }

  return s;
}

bool RunInteractiveProcess(const vector<string> & aargs, int & aexit_code, string * astderr_text, const string & aworkdir)
{
  aexit_code = -1;
  if (astderr_text)
  {
    astderr_text->clear();
  }

  if (aargs.empty())
  {
    aexit_code = PROCRUNERR_INVALID_ARGS;
    return false;
  }

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

  PROCESS_INFORMATION pi = {};
  string cmd = BuildWindowsCommandLine(aargs);
  wstring wcmd = Utf8ToWide(cmd);
  wstring wworkdir = Utf8ToWide(aworkdir);

  BOOL created = CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, TRUE, 0, nullptr,
                                aworkdir.empty() ? nullptr : wworkdir.c_str(), &si, &pi);
  if (!created)
  {
    aexit_code = PROCRUNERR_EXEC;
    if (astderr_text)
    {
      *astderr_text = WindowsErrorMessage(GetLastError());
    }
    return false;
  }

  DWORD wr = WaitForSingleObject(pi.hProcess, INFINITE);
  if (WAIT_FAILED == wr)
  {
    aexit_code = PROCRUNERR_WAIT;
    if (astderr_text)
    {
      *astderr_text = WindowsErrorMessage(GetLastError());
    }
    CloseHandleNoError(pi.hThread);
    CloseHandleNoError(pi.hProcess);
    return false;
  }

  DWORD child_exit = 0;
  if (!GetExitCodeProcess(pi.hProcess, &child_exit))
  {
    aexit_code = PROCRUNERR_WAIT;
    if (astderr_text)
    {
      *astderr_text = WindowsErrorMessage(GetLastError());
    }
    CloseHandleNoError(pi.hThread);
    CloseHandleNoError(pi.hProcess);
    return false;
  }

  CloseHandleNoError(pi.hThread);
  CloseHandleNoError(pi.hProcess);
  aexit_code = static_cast<int>(child_exit);
  return true;
}

#else

struct SExecErrorInfo
{
  int runner_exit_code;
  int sys_errno;
};

static void SetNonBlocking(int afd)
{
  int flags = fcntl(afd, F_GETFL, 0);
  if (flags < 0)
  {
    throw runtime_error("fcntl(F_GETFL) failed");
  }

  if (fcntl(afd, F_SETFL, flags | O_NONBLOCK) < 0)
  {
    throw runtime_error("fcntl(F_SETFL) failed");
  }
}

static void CloseFd(int & afd)
{
  if (afd >= 0)
  {
    close(afd);
    afd = -1;
  }
}

static bool SetCloseOnExec(int afd)
{
  int flags = fcntl(afd, F_GETFD, 0);
  if (flags < 0)
  {
    return false;
  }

  return (fcntl(afd, F_SETFD, flags | FD_CLOEXEC) >= 0);
}

static void ReadPipeData(int afd, string * adst, bool * aeof)
{
  array<char, 4096> buf;

  while (true)
  {
    ssize_t r = read(afd, buf.data(), buf.size());
    if (r > 0)
    {
      adst->append(buf.data(), r);
      continue;
    }

    if (0 == r)
    {
      *aeof = true;
      return;
    }

    if ((errno == EAGAIN) or (errno == EWOULDBLOCK))
    {
      return;
    }

    *aeof = true;
    return;
  }
}

static bool ReadExecError(int afd, int & aexit_code, string * astderr_text)
{
  SExecErrorInfo errinfo { 0, 0 };
  ssize_t execerr_read = read(afd, &errinfo, sizeof(errinfo));

  if (execerr_read == sizeof(errinfo))
  {
    aexit_code = errinfo.runner_exit_code;
    if (astderr_text && astderr_text->empty())
    {
      *astderr_text = string(strerror(errinfo.sys_errno));
    }
    return true;
  }

  return false;
}

static bool PrepareExecErrorPipe(int execerrpipe[2])
{
  if (pipe(execerrpipe) < 0)
  {
    return false;
  }

  if (!SetCloseOnExec(execerrpipe[1]))
  {
    CloseFd(execerrpipe[0]);
    CloseFd(execerrpipe[1]);
    return false;
  }

  return true;
}

OProcessRunner::OProcessRunner()
{
}

OProcessRunner::~OProcessRunner()
{
}

bool OProcessRunner::Run()
{
  exit_code = -1;
  duration_us = 0;
  stdout_text.clear();
  stderr_text.clear();
  cmdline = BuildCmdLine();

  if (args.empty())
  {
    exit_code = PROCRUNERR_INVALID_ARGS;
    return false;
  }

  int outpipe[2] = {-1, -1};
  int errpipe[2] = {-1, -1};
  int execerrpipe[2] = {-1, -1};

  if (pipe(outpipe) < 0)
  {
    exit_code = PROCRUNERR_PIPE_CREATE;
    return false;
  }

  if (pipe(errpipe) < 0)
  {
    CloseFd(outpipe[0]);
    CloseFd(outpipe[1]);
    exit_code = PROCRUNERR_PIPE_CREATE;
    return false;
  }

  if (!PrepareExecErrorPipe(execerrpipe))
  {
    CloseFd(outpipe[0]);
    CloseFd(outpipe[1]);
    CloseFd(errpipe[0]);
    CloseFd(errpipe[1]);
    exit_code = PROCRUNERR_SETUP;
    return false;
  }

  auto start_tp = chrono::steady_clock::now();

  pid_t pid = fork();
  if (pid < 0)
  {
    CloseFd(outpipe[0]);
    CloseFd(outpipe[1]);
    CloseFd(errpipe[0]);
    CloseFd(errpipe[1]);
    CloseFd(execerrpipe[0]);
    CloseFd(execerrpipe[1]);
    exit_code = PROCRUNERR_FORK;
    return false;
  }

  if (0 == pid)
  {
    CloseFd(execerrpipe[0]);

    if (!workdir.empty())
    {
      if (chdir(workdir.c_str()) < 0)
      {
        SExecErrorInfo errinfo { PROCRUNERR_CHDIR, errno };
        (void)write(execerrpipe[1], &errinfo, sizeof(errinfo));
        _exit(127);
      }
    }

    dup2(outpipe[1], STDOUT_FILENO);
    dup2(errpipe[1], STDERR_FILENO);

    CloseFd(outpipe[0]);
    CloseFd(outpipe[1]);
    CloseFd(errpipe[0]);
    CloseFd(errpipe[1]);

    vector<char *> argv;
    argv.reserve(args.size() + 1);
    for (const string & s : args)
    {
      argv.push_back(const_cast<char *>(s.c_str()));
    }
    argv.push_back(nullptr);

    execvp(argv[0], argv.data());
    SExecErrorInfo errinfo { PROCRUNERR_EXEC, errno };
    (void)write(execerrpipe[1], &errinfo, sizeof(errinfo));
    CloseFd(execerrpipe[1]);
    _exit(127);
  }

  CloseFd(outpipe[1]);
  CloseFd(errpipe[1]);
  CloseFd(execerrpipe[1]);

  try
  {
    SetNonBlocking(outpipe[0]);
    SetNonBlocking(errpipe[0]);
  }
  catch (...)
  {
    CloseFd(outpipe[0]);
    CloseFd(errpipe[0]);
    CloseFd(execerrpipe[0]);
    waitpid(pid, nullptr, 0);
    exit_code = PROCRUNERR_SETUP;
    return false;
  }

  bool out_eof = false;
  bool err_eof = false;
  bool timed_out = false;
  bool poll_failed = false;

  while (!out_eof or !err_eof)
  {
    pollfd fds[2];
    nfds_t fdcount = 0;
    int poll_timeout_ms = -1;

    if (!timed_out and (exec_timeout_ms >= 0))
    {
      auto now_tp = chrono::steady_clock::now();
      auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(now_tp - start_tp).count();
      int64_t remaining_ms = int64_t(exec_timeout_ms) - elapsed_ms;

      if (remaining_ms <= 0)
      {
        kill(pid, SIGKILL);
        timed_out = true;
        continue;
      }

      poll_timeout_ms = static_cast<int>(remaining_ms);
    }

    if (!out_eof)
    {
      fds[fdcount].fd = outpipe[0];
      fds[fdcount].events = POLLIN | POLLHUP;
      fds[fdcount].revents = 0;
      ++fdcount;
    }

    if (!err_eof)
    {
      fds[fdcount].fd = errpipe[0];
      fds[fdcount].events = POLLIN | POLLHUP;
      fds[fdcount].revents = 0;
      ++fdcount;
    }

    int pr = poll(fds, fdcount, poll_timeout_ms);
    if (pr < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }
      poll_failed = true;
      kill(pid, SIGKILL);
      break;
    }
    else if (0 == pr)
    {
      kill(pid, SIGKILL);
      timed_out = true;
      continue;
    }

    nfds_t idx = 0;
    if (!out_eof)
    {
      if (fds[idx].revents & (POLLIN | POLLHUP))
      {
        ReadPipeData(outpipe[0], &stdout_text, &out_eof);
      }
      ++idx;
    }

    if (!err_eof)
    {
      if (fds[idx].revents & (POLLIN | POLLHUP))
      {
        ReadPipeData(errpipe[0], &stderr_text, &err_eof);
      }
    }
  }

  CloseFd(outpipe[0]);
  CloseFd(errpipe[0]);

  int status = 0;
  pid_t wait_rv = waitpid(pid, &status, 0);

  CloseFd(execerrpipe[1]);
  bool has_exec_error = ReadExecError(execerrpipe[0], exit_code, &stderr_text);
  CloseFd(execerrpipe[0]);

  auto end_tp = chrono::steady_clock::now();
  duration_us = chrono::duration_cast<chrono::microseconds>(end_tp - start_tp).count();

  if (wait_rv < 0)
  {
    exit_code = PROCRUNERR_WAIT;
    if (stderr_text.empty())
    {
      stderr_text = string(strerror(errno));
    }
    return false;
  }

  if (WIFEXITED(status))
  {
    exit_code = WEXITSTATUS(status);
  }
  else if (WIFSIGNALED(status))
  {
    exit_code = -WTERMSIG(status);
  }
  else
  {
    exit_code = -1;
  }

  if (has_exec_error)
  {
    return false;
  }

  if (timed_out)
  {
    exit_code = PROCRUNERR_TIMEOUT;
    if (stderr_text.empty())
    {
      stderr_text = "Process execution timed out.";
    }
    return false;
  }

  if (poll_failed)
  {
    exit_code = PROCRUNERR_POLL;
    if (stderr_text.empty())
    {
      stderr_text = "Process polling failed.";
    }
    return false;
  }

  return true;
}

string OProcessRunner::BuildCmdLine()
{
  string s;

  for (size_t i = 0; i < args.size(); ++i)
  {
    if (i > 0)
    {
      s += " ";
    }

    s += args[i];
  }

  return s;
}

bool RunInteractiveProcess(const vector<string> & aargs, int & aexit_code, string * astderr_text, const string & aworkdir)
{
  aexit_code = -1;
  if (astderr_text)
  {
    astderr_text->clear();
  }

  if (aargs.empty())
  {
    aexit_code = PROCRUNERR_INVALID_ARGS;
    return false;
  }

  int execerrpipe[2] = {-1, -1};
  if (!PrepareExecErrorPipe(execerrpipe))
  {
    aexit_code = PROCRUNERR_SETUP;
    if (astderr_text)
    {
      *astderr_text = "Failed to create process setup pipe.";
    }
    return false;
  }

  pid_t pid = fork();
  if (pid < 0)
  {
    CloseFd(execerrpipe[0]);
    CloseFd(execerrpipe[1]);
    aexit_code = PROCRUNERR_FORK;
    if (astderr_text)
    {
      *astderr_text = string(strerror(errno));
    }
    return false;
  }

  if (0 == pid)
  {
    CloseFd(execerrpipe[0]);

    if (!aworkdir.empty())
    {
      if (chdir(aworkdir.c_str()) < 0)
      {
        SExecErrorInfo errinfo { PROCRUNERR_CHDIR, errno };
        (void)write(execerrpipe[1], &errinfo, sizeof(errinfo));
        _exit(127);
      }
    }

    vector<char *> argv;
    argv.reserve(aargs.size() + 1);
    for (const string & s : aargs)
    {
      argv.push_back(const_cast<char *>(s.c_str()));
    }
    argv.push_back(nullptr);

    execvp(argv[0], argv.data());
    SExecErrorInfo errinfo { PROCRUNERR_EXEC, errno };
    (void)write(execerrpipe[1], &errinfo, sizeof(errinfo));
    CloseFd(execerrpipe[1]);
    _exit(127);
  }

  CloseFd(execerrpipe[1]);

  int status = 0;
  pid_t wait_rv = waitpid(pid, &status, 0);

  bool has_exec_error = ReadExecError(execerrpipe[0], aexit_code, astderr_text);
  CloseFd(execerrpipe[0]);

  if (wait_rv < 0)
  {
    aexit_code = PROCRUNERR_WAIT;
    if (astderr_text && astderr_text->empty())
    {
      *astderr_text = string(strerror(errno));
    }
    return false;
  }

  if (has_exec_error)
  {
    return false;
  }

  if (WIFEXITED(status))
  {
    aexit_code = WEXITSTATUS(status);
  }
  else if (WIFSIGNALED(status))
  {
    aexit_code = -WTERMSIG(status);
  }
  else
  {
    aexit_code = -1;
  }

  return true;
}

#endif
