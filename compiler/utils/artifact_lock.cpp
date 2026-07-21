/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    artifact_lock.cpp
 * authors: nvitya
 * created: 2026-05-10
 * brief:   Compiler artifact locking and atomic publishing helpers
 */

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <process.h>
  #include <windows.h>
#endif

#include "artifact_lock.h"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <format>

#if !defined(_WIN32)
  #include <fcntl.h>
  #include <sys/file.h>
  #include <sys/stat.h>
  #include <unistd.h>
#endif

using namespace std;

static atomic<uint64_t> g_artifact_tmp_counter = 0;

#if defined(_WIN32)
static string WindowsErrorMessage(DWORD err)
{
  LPSTR msg = nullptr;
  DWORD len = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                             | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, err, 0, reinterpret_cast<LPSTR>(&msg), 0, nullptr);

  string result = (len && msg ? string(msg, len) : format("Windows error {}", err));
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
#endif

OArtifactLock::OArtifactLock(const filesystem::path & artifact_path, EArtifactLockMode mode)
{
  Lock(artifact_path, mode);
}

OArtifactLock::~OArtifactLock()
{
  Unlock();
}

bool OArtifactLock::Lock(const filesystem::path & artifact_path, EArtifactLockMode mode)
{
  Unlock();
  error.clear();

  lock_path = artifact_path;
  lock_path += ".lock";
  locked_exclusive = (EArtifactLockMode::EXCLUSIVE == mode);
  if (!ArtifactEnsureParentDir(lock_path, error))
  {
    return false;
  }

#if defined(_WIN32)
  DWORD access = GENERIC_READ | GENERIC_WRITE;
  HANDLE new_handle = INVALID_HANDLE_VALUE;
  int retries = 0;
  while (true)
  {
    new_handle = CreateFileW(lock_path.wstring().c_str(), access,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (INVALID_HANDLE_VALUE == new_handle)
    {
      if (GetLastError() == ERROR_ACCESS_DENIED && retries < 200)
      {
        Sleep(10);
        retries++;
        continue;
      }
      error = format("Can not open artifact lock {}: {}", lock_path.string(), WindowsErrorMessage(GetLastError()));
      return false;
    }
    break;
  }

  OVERLAPPED ov = {};
  DWORD flags = (EArtifactLockMode::EXCLUSIVE == mode ? LOCKFILE_EXCLUSIVE_LOCK : 0);
  if (!LockFileEx(new_handle, flags, 0, MAXDWORD, MAXDWORD, &ov))
  {
    error = format("Can not lock artifact {}: {}", artifact_path.string(), WindowsErrorMessage(GetLastError()));
    CloseHandle(new_handle);
    return false;
  }

  fd = new_handle;
  return true;
#else
  while (true)
  {
    int new_fd = open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0666);
    if (new_fd < 0)
    {
      error = format("Can not open artifact lock {}: {}", lock_path.string(), strerror(errno));
      return false;
    }

    int op = (EArtifactLockMode::SHARED == mode ? LOCK_SH : LOCK_EX);
    while (flock(new_fd, op) < 0)
    {
      if (EINTR == errno)
      {
        continue;
      }

      error = format("Can not lock artifact {}: {}", artifact_path.string(), strerror(errno));
      close(new_fd);
      return false;
    }

    struct stat fd_st = {};
    struct stat path_st = {};
    bool same_path = (0 == fstat(new_fd, &fd_st))
        && (0 == stat(lock_path.c_str(), &path_st))
        && (fd_st.st_dev == path_st.st_dev)
        && (fd_st.st_ino == path_st.st_ino);

    if (same_path)
    {
      fd = new_fd;
      return true;
    }

    flock(new_fd, LOCK_UN);
    close(new_fd);
  }
#endif
}

void OArtifactLock::Unlock()
{
#if defined(_WIN32)
  if (fd)
  {
    OVERLAPPED ov = {};
    HANDLE handle = static_cast<HANDLE>(fd);
    UnlockFileEx(handle, 0, MAXDWORD, MAXDWORD, &ov);
    CloseHandle(handle);
    fd = nullptr;
  }
#else
  if (fd >= 0)
  {
    if (!lock_path.empty())
    {
      if (locked_exclusive)
      {
        ArtifactRemoveNoError(lock_path);
      }
      else
      {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0)
        {
          ArtifactRemoveNoError(lock_path);
        }
      }
    }

    flock(fd, LOCK_UN);
    close(fd);
    fd = -1;
  }
#endif
  lock_path.clear();
  locked_exclusive = false;
}

filesystem::path ArtifactTempPathFor(const filesystem::path & artifact_path)
{
  filesystem::path result = artifact_path;
#if defined(_WIN32)
  result += format(".tmp.{}.{}", _getpid(), ++g_artifact_tmp_counter);
#else
  result += format(".tmp.{}.{}", getpid(), ++g_artifact_tmp_counter);
#endif
  return result;
}

bool ArtifactEnsureParentDir(const filesystem::path & artifact_path, string & rerror)
{
  filesystem::path parent_path = artifact_path.parent_path();
  if (parent_path.empty())
  {
    return true;
  }

  error_code ec;
  filesystem::create_directories(parent_path, ec);
  if (ec)
  {
    rerror = format("Can not create artifact directory {}: {}", parent_path.string(), ec.message());
    return false;
  }

  return true;
}

bool ArtifactAtomicReplace(const filesystem::path & tmp_path, const filesystem::path & artifact_path,
                           string & rerror)
{
  if (!ArtifactEnsureParentDir(artifact_path, rerror))
  {
    ArtifactRemoveNoError(tmp_path);
    return false;
  }

#if defined(_WIN32)
  //TODO: is this retry really necessary ?
  int retries = 0;
  while (!MoveFileExW(tmp_path.wstring().c_str(), artifact_path.wstring().c_str(),
                   MOVEFILE_REPLACE_EXISTING))
  {
    if (GetLastError() == ERROR_ACCESS_DENIED && retries < 200)
    {
      Sleep(10);
      retries++;
      continue;
    }
    rerror = format("Can not publish artifact {}: {}", artifact_path.string(), WindowsErrorMessage(GetLastError()));
    ArtifactRemoveNoError(tmp_path);
    return false;
  }
  
  // Force NTFS to synchronously update the directory entry file size cache
  // so that immediate subsequent tools (like LLD) don't see a stale 0-byte size.
  HANDLE h = CreateFileW(artifact_path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h != INVALID_HANDLE_VALUE)
  {
    BY_HANDLE_FILE_INFORMATION info;
    GetFileInformationByHandle(h, &info);
    CloseHandle(h);
  }

  return true;
#else
  error_code ec;
  filesystem::rename(tmp_path, artifact_path, ec);
  if (ec)
  {
    rerror = format("Can not publish artifact {}: {}", artifact_path.string(), ec.message());
    ArtifactRemoveNoError(tmp_path);
    return false;
  }
  return true;
#endif
}

bool ArtifactAtomicWrite(const filesystem::path & artifact_path, const vector<uint8_t> & data, string & rerror)
{
  if (!ArtifactEnsureParentDir(artifact_path, rerror))
  {
    return false;
  }

  filesystem::path tmp_path = ArtifactTempPathFor(artifact_path);
  ofstream outf(tmp_path, ios::binary);
  if (!outf)
  {
    rerror = format("Can not create temporary artifact file: {}", tmp_path.string());
    return false;
  }

  if (!data.empty())
  {
    outf.write(reinterpret_cast<const char *>(data.data()), data.size());
  }
  outf.close();
  if (!outf)
  {
    rerror = format("Can not write temporary artifact file: {}", tmp_path.string());
    ArtifactRemoveNoError(tmp_path);
    return false;
  }

  return ArtifactAtomicReplace(tmp_path, artifact_path, rerror);
}

void ArtifactRemoveNoError(const filesystem::path & path)
{
  error_code ec;
  filesystem::remove(path, ec);
}

filesystem::path ArtifactInterfacePathForObject(const filesystem::path & object_path)
{
  filesystem::path result = object_path;
  result.replace_extension(".dqm_if");
  return result;
}

filesystem::path ArtifactBitcodeSidecarPathForObject(const filesystem::path & object_path)
{
  filesystem::path result = object_path;
  result += ".bc";
  return result;
}
