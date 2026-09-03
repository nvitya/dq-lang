/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    lang_server.h
 * authors: nvitya
 * created: 2026-09-02
 * brief:   Language Server implementation
 */

#pragma once

#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <regex>

#include "jsontools.h"

using namespace std;

struct SDocument
{
  string uri;
  filesystem::path path;
  string text;
  int64_t version = 0;
};

struct SDiagnostic
{
  string path;
  string severity;
  string code;
  string message;
  int line = 1;
  int column = 1;
};

struct SDocumentSymbol
{
  string path;
  string name;
  int kind = 13;
  int line = 1;
  int column = 1;
  vector<SDocumentSymbol> children;
};

struct SWorkerResult
{
  vector<SDiagnostic> diagnostics;
  vector<SDocumentSymbol> document_symbols;
  unordered_map<string, vector<SDocumentSymbol>> namespaces;
};

class ODqLanguageServer
{
public:
  ODqLanguageServer()
  {
    error_code ec;
    temp_root = filesystem::temp_directory_path(ec) / format("dq-lsp-{}", chrono::steady_clock::now().time_since_epoch().count());
    filesystem::create_directories(temp_root / "sources", ec);
  }

  ~ODqLanguageServer()
  {
    error_code ec;
    filesystem::remove_all(temp_root, ec);
  }

  int Run();

private:
  filesystem::path temp_root;
  unordered_map<string, SDocument> documents;
  unordered_map<string, vector<SDocumentSymbol>> document_symbols;
  unordered_map<string, vector<SDocumentSymbol>> namespaces;
  bool initialize_received = false;
  bool initialized = false;
  bool shutdown_requested = false;
  bool should_exit = false;
  int exit_status = 0;

  void Handle(const TJsonNode & request);
  void Respond(const TJsonNode & request, const TJsonNode & result);
  void RespondError(const TJsonNode * request, int code, string_view message);
  void PublishDiagnostics(const SDocument & document, const vector<SDiagnostic> & diagnostics);
  TJsonNode DocumentSymbolsJson(const SDocument & document) const;
  TJsonNode DefinitionJson(const SDocument & document, int line, int character) const;
  void Reanalyze();
  bool StageDocuments(filesystem::path & rmanifest, filesystem::path & rbuild_root);
  SWorkerResult RunWorker(const filesystem::path & source, const filesystem::path & manifest,
                          const filesystem::path & build_root);
};


int RunDqLanguageServer();
