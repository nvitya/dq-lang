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
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "processrunner.h"

using namespace std;

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
    return false;
  }

  int outpipe[2] = {-1, -1};
  int errpipe[2] = {-1, -1};
  int execerrpipe[2] = {-1, -1};

  if (pipe(outpipe) < 0)
  {
    return false;
  }

  if (pipe(errpipe) < 0)
  {
    CloseFd(outpipe[0]);
    CloseFd(outpipe[1]);
    return false;
  }

  if (pipe(execerrpipe) < 0)
  {
    CloseFd(outpipe[0]);
    CloseFd(outpipe[1]);
    CloseFd(errpipe[0]);
    CloseFd(errpipe[1]);
    return false;
  }

  if (!SetCloseOnExec(execerrpipe[1]))
  {
    CloseFd(outpipe[0]);
    CloseFd(outpipe[1]);
    CloseFd(errpipe[0]);
    CloseFd(errpipe[1]);
    CloseFd(execerrpipe[0]);
    CloseFd(execerrpipe[1]);
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
    return false;
  }

  if (0 == pid)
  {
    CloseFd(execerrpipe[0]);

    if (!workdir.empty())
    {
      if (chdir(workdir.c_str()) < 0)
      {
        int err = errno;
        (void)write(execerrpipe[1], &err, sizeof(err));
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
    int err = errno;
    (void)write(execerrpipe[1], &err, sizeof(err));
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
    return false;
  }

  bool out_eof = false;
  bool err_eof = false;

  while (!out_eof or !err_eof)
  {
    pollfd fds[2];
    nfds_t fdcount = 0;

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

    int pr = poll(fds, fdcount, -1);
    if (pr < 0)
    {
      break;
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
  waitpid(pid, &status, 0);

  int exec_errno = 0;
  ssize_t execerr_read = read(execerrpipe[0], &exec_errno, sizeof(exec_errno));
  CloseFd(execerrpipe[0]);

  auto end_tp = chrono::steady_clock::now();
  duration_us = chrono::duration_cast<chrono::microseconds>(end_tp - start_tp).count();

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

  if (execerr_read == sizeof(exec_errno))
  {
    if (stderr_text.empty())
    {
      stderr_text = string(strerror(exec_errno));
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
