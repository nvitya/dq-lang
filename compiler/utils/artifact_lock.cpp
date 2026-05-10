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

filesystem::path OArtifactLock::LockPathFor(const filesystem::path & artifact_path)
{
  filesystem::path result = artifact_path;
  result += ".lock";
  return result;
}

bool OArtifactLock::Lock(const filesystem::path & artifact_path, EArtifactLockMode mode)
{
  Unlock();
  error.clear();

  filesystem::path lock_path = LockPathFor(artifact_path);
  fd = open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0666);
  if (fd < 0)
  {
    error = format("Can not open artifact lock {}: {}", lock_path.string(), strerror(errno));
    return false;
  }

  int op = (EArtifactLockMode::SHARED == mode ? LOCK_SH : LOCK_EX);
  while (flock(fd, op) < 0)
  {
    if (EINTR == errno)
    {
      continue;
    }

    error = format("Can not lock artifact {}: {}", lock_path.string(), strerror(errno));
    close(fd);
    fd = -1;
    return false;
  }

  return true;
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
