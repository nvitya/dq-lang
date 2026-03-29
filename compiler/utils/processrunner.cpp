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
#include <chrono>
#include <poll.h>
#include <stdexcept>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "processrunner.h"

using namespace std;

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
