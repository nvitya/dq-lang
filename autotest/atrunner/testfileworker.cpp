/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    testfileworker.cpp
 * authors: Codex
 * created: 2026-03-17
 * brief:
 */

#include "testfileworker.h"

OTestFileWorker::OTestFileWorker()
{
}

OTestFileWorker::~OTestFileWorker()
{
}

void OTestFileWorker::Process(OTestFile * atestfile)
{
  testfile = atestfile;
}

