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
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <regex>

#include "jsontools.h"

using namespace std;

struct SDocument
{
  string uri;
  filesystem::path path;
  string text;
  int64_t version = 0;
  uint64_t revision = 0;
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
  string type_name;
  int kind = 13;
  int line = 1;
  int column = 1;
  vector<SDocumentSymbol> children;
};

struct SWorkerDependency
{
  string path;
  int64_t filesize = 0;
  int64_t filetime = 0;
  uint64_t staged_revision = 0;
};

using SSymbolListPtr = shared_ptr<const vector<SDocumentSymbol>>;

struct SWorkerResult
{
  bool success = false;
  vector<SDiagnostic> diagnostics;
  vector<SDocumentSymbol> document_symbols;
  unordered_map<string, SSymbolListPtr> namespaces;
  unordered_set<string> module_namespaces;
  vector<filesystem::path> used_module_sources;
  vector<SWorkerDependency> dependencies;
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
  struct SStagedSource
  {
    filesystem::path path;
    string text;
    uint64_t revision = 0;
  };
  struct SCachedInterfaceSymbols
  {
    int64_t filesize = 0;
    int64_t filetime = 0;
    SSymbolListPtr symbols;
  };
  unordered_map<string, SStagedSource> staged_sources;
  unordered_map<string, SDocument> documents;
  unordered_map<string, vector<SDocumentSymbol>> document_symbols;
  unordered_map<string, SSymbolListPtr> namespaces;
  unordered_map<string, unordered_map<string, SSymbolListPtr>> document_local_namespaces;
  unordered_map<string, SCachedInterfaceSymbols> cached_interface_namespaces;
  unordered_map<string, SWorkerResult> worker_cache;
  unordered_set<string> module_namespaces;
  uint64_t document_revision = 0;
  bool known_interfaces_dirty = true;
  bool analysis_dirty = false;
  bool initialize_received = false;
  bool initialized = false;
  bool shutdown_requested = false;
  bool should_exit = false;
  int exit_status = 0;

  void Handle(const TJsonNode & request);
  void Respond(const TJsonNode & request, TJsonNode result);
  void RespondError(const TJsonNode * request, int code, string_view message);
  void PublishDiagnostics(const SDocument & document, const vector<SDiagnostic> & diagnostics);
  TJsonNode DocumentSymbolsJson(const SDocument & document) const;
  TJsonNode DefinitionJson(const SDocument & document, int line, int character) const;
  void FlushPendingAnalysis();
  void Reanalyze();
  bool IsWorkerCacheFresh(const SWorkerResult & cached) const;
  bool WriteKnownInterfaces(filesystem::path & rpath);
  bool StageDocuments(filesystem::path & rmanifest, filesystem::path & rbuild_root);
  SWorkerResult RunWorker(const filesystem::path & source, const filesystem::path & manifest,
                          const filesystem::path & build_root);
};


int RunDqLanguageServer();
