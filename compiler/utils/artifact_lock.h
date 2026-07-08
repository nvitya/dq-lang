/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    artifact_lock.h
 * authors: nvitya
 * created: 2026-05-10
 * brief:   Compiler artifact locking and atomic publishing helpers
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

using namespace std;

enum class EArtifactLockMode
{
  SHARED,
  EXCLUSIVE
};

class OArtifactLock
{
public:
  string error;

private:
#if defined(_WIN32)
  void * fd = nullptr;
#else
  int fd = -1;
#endif

public:
  OArtifactLock() = default;
  OArtifactLock(const filesystem::path & artifact_path, EArtifactLockMode mode);
  ~OArtifactLock();

  OArtifactLock(const OArtifactLock &) = delete;
  OArtifactLock & operator=(const OArtifactLock &) = delete;

  bool Lock(const filesystem::path & artifact_path, EArtifactLockMode mode);
  void Unlock();
  bool Locked() const
  {
#if defined(_WIN32)
    return (fd != nullptr);
#else
    return fd >= 0;
#endif
  }
};

filesystem::path ArtifactTempPathFor(const filesystem::path & artifact_path);
bool ArtifactAtomicWrite(const filesystem::path & artifact_path, const vector<uint8_t> & data, string & rerror);
bool ArtifactAtomicReplace(const filesystem::path & tmp_path, const filesystem::path & artifact_path,
                           string & rerror);
bool ArtifactEnsureParentDir(const filesystem::path & artifact_path, string & rerror);
void ArtifactRemoveNoError(const filesystem::path & path);
filesystem::path ArtifactInterfaceSidecarPathForObject(const filesystem::path & object_path);
filesystem::path ArtifactBitcodeSidecarPathForObject(const filesystem::path & object_path);
void ArtifactCleanupInterfaceSidecarForObject(const filesystem::path & object_path);
