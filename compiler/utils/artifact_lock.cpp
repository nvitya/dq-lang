/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    artifact_lock.cpp
 * authors: nvitya
 * created: 2026-05-10
 * brief:   Compiler artifact locking and atomic publishing helpers
 */

#include "artifact_lock.h"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <format>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

static atomic<uint64_t> g_artifact_tmp_counter = 0;

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

  int open_flags = (EArtifactLockMode::SHARED == mode ? O_RDONLY : (O_CREAT | O_RDWR));

  while (true)
  {
    int new_fd = open(artifact_path.c_str(), open_flags | O_CLOEXEC, 0666);
    if (new_fd < 0)
    {
      error = format("Can not open artifact for locking {}: {}", artifact_path.string(), strerror(errno));
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
        && (0 == stat(artifact_path.c_str(), &path_st))
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
}

void OArtifactLock::Unlock()
{
  if (fd >= 0)
  {
    flock(fd, LOCK_UN);
    close(fd);
    fd = -1;
  }
}

filesystem::path ArtifactTempPathFor(const filesystem::path & artifact_path)
{
  filesystem::path result = artifact_path;
  result += format(".tmp.{}.{}", getpid(), ++g_artifact_tmp_counter);
  return result;
}

bool ArtifactAtomicReplace(const filesystem::path & tmp_path, const filesystem::path & artifact_path,
                           string & rerror)
{
  error_code ec;
  filesystem::rename(tmp_path, artifact_path, ec);
  if (ec)
  {
    rerror = format("Can not publish artifact {}: {}", artifact_path.string(), ec.message());
    ArtifactRemoveNoError(tmp_path);
    return false;
  }
  return true;
}

bool ArtifactAtomicWrite(const filesystem::path & artifact_path, const vector<uint8_t> & data, string & rerror)
{
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

filesystem::path ArtifactInterfaceSidecarPathForObject(const filesystem::path & object_path)
{
  filesystem::path result = object_path;
  result.replace_extension(".dqm_if");
  return result;
}

void ArtifactCleanupInterfaceSidecarForObject(const filesystem::path & object_path)
{
  filesystem::path sidecar_path = ArtifactInterfaceSidecarPathForObject(object_path);
  OArtifactLock lock(sidecar_path, EArtifactLockMode::EXCLUSIVE);
  if (lock.Locked())
  {
    ArtifactRemoveNoError(sidecar_path);
  }
}
