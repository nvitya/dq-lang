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
 *
 * The language server deliberately uses a fresh dq-comp worker for each
 * analysis pass.  Compiler globals and module compilation are therefore
 * isolated exactly as they are for command-line builds.
 */


#include "comp_options.h"
#include "lang_server.h"
#include "processrunner.h"

#include "dq_utils.h"

using namespace std;

static string UriDecode(string_view text)
{
  string result;
  for (size_t i = 0; i < text.size(); ++i)
  {
    if ((text[i] == '%') && (i + 2 < text.size())
        && isxdigit(static_cast<unsigned char>(text[i + 1]))
        && isxdigit(static_cast<unsigned char>(text[i + 2])))
    {
      auto digit = [](char c) -> int { return isdigit(static_cast<unsigned char>(c)) ? c - '0' : (tolower(static_cast<unsigned char>(c)) - 'a' + 10); };
      result.push_back(char((digit(text[i + 1]) << 4) | digit(text[i + 2])));
      i += 2;
    }
    else result.push_back(text[i]);
  }
  return result;
}

static string FilePathFromUri(const string & uri)
{
  constexpr string_view prefix = "file://";
  if (!uri.starts_with(prefix)) return {};
  string path = UriDecode(string_view(uri).substr(prefix.size()));
#if defined(_WIN32)
  if ((path.size() > 2) && (path[0] == '/') && (path[2] == ':')) path.erase(0, 1);
#endif
  return path;
}

static string FileUri(const filesystem::path & path)
{
  string raw = path.string();
  string encoded;
  static constexpr char hex[] = "0123456789ABCDEF";
  for (unsigned char c : raw)
  {
    if (isalnum(c) || (c == '/') || (c == '-') || (c == '_') || (c == '.') || (c == '~')) encoded.push_back(c);
    else
    {
      encoded.push_back('%');
      encoded.push_back(hex[c >> 4]);
      encoded.push_back(hex[c & 0x0f]);
    }
  }
  return "file://" + encoded;
}

static bool ReadMessage(string & rpayload)
{
  string line;
  size_t content_length = 0;
  bool has_length = false;
  while (getline(cin, line))
  {
    if (!line.empty() && (line.back() == '\r')) line.pop_back();
    if (line.empty()) break;
    size_t colon = line.find(':');
    if (colon == string::npos) continue;
    string name = line.substr(0, colon);
    for (char & c : name) c = char(tolower(static_cast<unsigned char>(c)));
    if (name == "content-length")
    {
      if (has_length) return false;
      try
      {
        content_length = stoull(line.substr(colon + 1));
        if (content_length > 64 * 1024 * 1024) return false;
        has_length = true;
      }
      catch (...) { return false; }
    }
  }
  if (!has_length) return false;
  rpayload.resize(content_length);
  cin.read(rpayload.data(), static_cast<streamsize>(content_length));
  return static_cast<size_t>(cin.gcount()) == content_length;
}

static void WriteMessage(const string & payload)
{
  cout << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
  cout.flush();
}

static const TJsonNode * JsonChild(const TJsonNode & object, const string & name)
{
  return object.GetKind() == nkObject ? object.Child(name) : nullptr;
}

static bool JsonString(const TJsonNode & object, const string & name, string & rvalue)
{
  const TJsonNode * value = JsonChild(object, name);
  if (!value || value->GetKind() != nkString) return false;
  rvalue = value->GetAsString();
  return true;
}

static bool TryJsonInteger(const TJsonNode * value, long long & rvalue)
{
  if (!value || value->GetKind() != nkNumber) return false;
  try
  {
    string text = value->GetAsString();
    size_t length = 0;
    long long result = stoll(text, &length);
    if (length != text.size()) return false;
    rvalue = result;
    return true;
  }
  catch (...) { return false; }
}

static long long JsonInteger(const TJsonNode * value, long long default_value)
{
  long long result;
  return TryJsonInteger(value, result) ? result : default_value;
}

static bool IsRequest(const TJsonNode & object)
{
  return JsonChild(object, "id") != nullptr;
}

static void AddJsonId(TJsonNode & object, const TJsonNode * request)
{
  const TJsonNode * value = request ? JsonChild(*request, "id") : nullptr;
  if (value && value->GetKind() == nkNumber)
  {
    long long id;
    if (TryJsonInteger(value, id))
    {
      object.Add("id", id);
      return;
    }
  }
  if (value && value->GetKind() == nkString)
  {
    object.Add("id", value->GetAsString());
    return;
  }
  object.Add("id").GetAsNull();
}

static void AddPosition(TJsonNode & object, const char * name, int line, int character)
{
  TJsonNode & position = object.Add(name).GetAsObject();
  position.Add("line", line);
  position.Add("character", character);
}

static void AddRange(TJsonNode & object, int line, int character, int end_line, int end_character)
{
  TJsonNode & range = object.Add("range").GetAsObject();
  AddPosition(range, "start", line, character);
  AddPosition(range, "end", end_line, end_character);
}

static void AddCompletionItem(TJsonNode & items, const string & label, int kind)
{
  TJsonNode & item = items.Add().GetAsObject();
  item.Add("label", label);
  item.Add("kind", kind);
}

static bool ReadDocumentSymbol(const TJsonNode & json_symbol, SDocumentSymbol & rsymbol)
{
  string path;
  if (json_symbol.GetKind() != nkObject || !JsonString(json_symbol, "path", path)
      || !JsonString(json_symbol, "name", rsymbol.name)) return false;
  rsymbol.path = AbsNormPath(path).string();
  rsymbol.kind = int(JsonInteger(JsonChild(json_symbol, "kind"), 13));
  rsymbol.line = int(JsonInteger(JsonChild(json_symbol, "line"), 1));
  rsymbol.column = int(JsonInteger(JsonChild(json_symbol, "column"), 1));
  const TJsonNode * json_children = JsonChild(json_symbol, "children");
  if (!json_children || json_children->GetKind() != nkArray) return true;
  for (int index = 0; index < json_children->GetCount(); ++index)
  {
    SDocumentSymbol child;
    if (ReadDocumentSymbol(json_children->Child(index), child)) rsymbol.children.push_back(move(child));
  }
  return true;
}

static int Utf8CharBytes(unsigned char c)
{
  if ((c & 0x80) == 0) return 1;
  if ((c & 0xe0) == 0xc0) return 2;
  if ((c & 0xf0) == 0xe0) return 3;
  if ((c & 0xf8) == 0xf0) return 4;
  return 1;
}

static int Utf16TextWidth(string_view text)
{
  int width = 0;
  for (size_t pos = 0; pos < text.size();)
  {
    int bytes = Utf8CharBytes(static_cast<unsigned char>(text[pos]));
    if ((bytes == 1) || (pos + bytes > text.size()))
    {
      ++pos;
      ++width;
    }
    else
    {
      pos += bytes;
      width += (bytes == 4 ? 2 : 1);
    }
  }
  return width;
}

static int Utf16Column(const string & text, int wanted_line, int wanted_byte_column)
{
  size_t start = 0;
  for (int line = 1; (line < wanted_line) && (start < text.size()); ++line)
  {
    size_t end = text.find('\n', start);
    start = (end == string::npos ? text.size() : end + 1);
  }
  size_t end = min(text.size(), start + size_t(max(0, wanted_byte_column)));
  int column = 0;
  for (size_t pos = start; pos < end;)
  {
    unsigned char c = static_cast<unsigned char>(text[pos]);
    int bytes = Utf8CharBytes(c);
    if ((pos + bytes > end) || (bytes == 1))
    {
      ++column;
      ++pos;
    }
    else
    {
      column += (bytes == 4 ? 2 : 1);
      pos += bytes;
    }
  }
  return column;
}

static int DocumentSymbolNameColumn(const SDocument & document, const SDocumentSymbol & symbol)
{
  size_t line_start = 0;
  for (int line = 1; (line < symbol.line) && (line_start < document.text.size()); ++line)
  {
    size_t line_end = document.text.find('\n', line_start);
    line_start = (line_end == string::npos ? document.text.size() : line_end + 1);
  }
  size_t line_end = document.text.find('\n', line_start);
  if (line_end == string::npos) line_end = document.text.size();
  size_t name_start = document.text.find(symbol.name, line_start);
  if ((name_start == string::npos) || (name_start >= line_end))
  {
    return Utf16Column(document.text, symbol.line, max(0, symbol.column - 1));
  }
  return Utf16Column(document.text, symbol.line, int(name_start - line_start));
}

static void AddDocumentSymbol(TJsonNode & symbols, const SDocument & document, const SDocumentSymbol & document_symbol)
{
  int line = max(0, document_symbol.line - 1);
  int character = DocumentSymbolNameColumn(document, document_symbol);
  int end_character = character + Utf16TextWidth(document_symbol.name);
  TJsonNode & symbol = symbols.Add().GetAsObject();
  symbol.Add("name", document_symbol.name);
  symbol.Add("kind", document_symbol.kind);
  AddRange(symbol, line, character, line, end_character);
  TJsonNode & selection_range = symbol.Add("selectionRange").GetAsObject();
  AddPosition(selection_range, "start", line, character);
  AddPosition(selection_range, "end", line, end_character);
  if (document_symbol.children.empty()) return;

  TJsonNode & children = symbol.Add("children").GetAsArray();
  for (const SDocumentSymbol & child : document_symbol.children)
  {
    AddDocumentSymbol(children, document, child);
  }
}


int ODqLanguageServer::Run()
{
  string payload;
  while (ReadMessage(payload))
  {
    TJsonNode request;
    if (!request.TryParse(payload))
    {
      RespondError(nullptr, -32700, "Parse error");
      continue;
    }
    if (request.GetKind() != nkObject)
    {
      RespondError(nullptr, -32600, "Invalid Request");
      continue;
    }
    Handle(request);
    if (should_exit) break;
  }
  return exit_status;
}

void ODqLanguageServer::Respond(const TJsonNode & request, const TJsonNode & result)
{
  TJsonNode response(nkObject);
  response.Add("jsonrpc", "2.0");
  AddJsonId(response, &request);
  response.Add("result").SetAsJson(result.GetAsJson(true));
  WriteMessage(response.GetAsJson(true));
}

void ODqLanguageServer::RespondError(const TJsonNode * request, int code, string_view message)
{
  TJsonNode response(nkObject);
  response.Add("jsonrpc", "2.0");
  AddJsonId(response, request);
  TJsonNode & error = response.Add("error").GetAsObject();
  error.Add("code", code);
  error.Add("message", string(message));
  WriteMessage(response.GetAsJson(true));
}

bool ODqLanguageServer::StageDocuments(filesystem::path & rmanifest, filesystem::path & rbuild_root)
{
  error_code ec;
  filesystem::path job_dir = temp_root / format("job-{}", chrono::steady_clock::now().time_since_epoch().count());
  filesystem::create_directories(job_dir / "sources", ec);
  if (ec) return false;
  rbuild_root = job_dir / "build";
  filesystem::create_directories(rbuild_root, ec);
  if (ec) return false;

  TJsonNode manifest(nkObject);
  TJsonNode & files = manifest.Add("files").GetAsArray();
  size_t index = 0;
  for (const auto & [uri, document] : documents)
  {
    filesystem::path staged = job_dir / "sources" / format("{}-{}{}", index++, document.version, document.path.extension().string());
    ofstream output(staged, ios::binary);
    output.write(document.text.data(), static_cast<streamsize>(document.text.size()));
    if (!output) return false;
    TJsonNode & file = files.Add().GetAsObject();
    file.Add("source", AbsNormPath(document.path).string());
    file.Add("staged", staged.string());
  }
  rmanifest = job_dir / "overlay.json";
  ofstream output(rmanifest, ios::binary);
  string text = manifest.GetAsJson(true);
  output.write(text.data(), static_cast<streamsize>(text.size()));
  return bool(output);
}

SWorkerResult ODqLanguageServer::RunWorker(const filesystem::path & source,
                                            const filesystem::path & manifest,
                                            const filesystem::path & build_root)
{
  filesystem::path result_path = build_root / format("semantic-{}.json", documents.size());
  OProcessRunner runner;
  runner.args = {g_opt.compiler_executable, "--langserver-worker", "--diagnostic-format=jsonl",
                 "--source-overlay", manifest.string(), "--build-root", build_root.string(),
                 "--package-build-root", build_root.string(), "--target=" + g_opt.target.name,
                 "--langserver-result", result_path.string(),
                 source.string()};
  if (g_opt.no_use_sys) runner.args.push_back("--no-use-sys");
  for (const string & path : g_opt.package_paths)
  {
    runner.args.push_back("--pkg-path");
    runner.args.push_back(path);
  }
  for (const OCmdLineDefine & define : g_opt.cmdline_defines)
  {
    string arg = "-D" + define.name;
    if (define.has_bool_value) arg += define.bool_value ? "=true" : "=false";
    else if (define.has_int_value) arg += "=" + to_string(define.int_value);
    runner.args.push_back(arg);
  }
  runner.Run();
  if (!runner.stderr_text.empty()) cerr << "dq-lsp worker: " << runner.stderr_text;

  SWorkerResult result;
  istringstream lines(runner.stdout_text);
  string line;
  while (getline(lines, line))
  {
    TJsonNode object;
    string kind;
    if (!object.TryParse(line) || object.GetKind() != nkObject || !JsonString(object, "kind", kind)
        || kind != "diagnostic") continue; // worker status output is never protocol data.
    string path;
    string message;
    if (!JsonString(object, "path", path) || !JsonString(object, "message", message)) continue;
    SDiagnostic diagnostic;
    diagnostic.path = AbsNormPath(path).string();
    JsonString(object, "severity", diagnostic.severity);
    if (diagnostic.severity.empty()) diagnostic.severity = "ERROR";
    JsonString(object, "code", diagnostic.code);
    diagnostic.message = message;
    diagnostic.line = int(JsonInteger(JsonChild(object, "line"), 1));
    diagnostic.column = int(JsonInteger(JsonChild(object, "column"), 1));
    result.diagnostics.push_back(move(diagnostic));
  }

  ifstream result_file(result_path, ios::binary);
  string result_text((istreambuf_iterator<char>(result_file)), istreambuf_iterator<char>());
  TJsonNode root;
  if (!root.TryParse(result_text) || root.GetKind() != nkObject) return result;
  const TJsonNode * symbols = JsonChild(root, "documentSymbols");
  if (symbols && symbols->GetKind() == nkArray)
  {
    for (int index = 0; index < symbols->GetCount(); ++index)
    {
      SDocumentSymbol symbol;
      if (ReadDocumentSymbol(symbols->Child(index), symbol)) result.document_symbols.push_back(move(symbol));
    }
  }
  const TJsonNode * namespaces_obj = JsonChild(root, "namespaces");
  if (namespaces_obj && namespaces_obj->GetKind() == nkObject)
  {
    for (int index = 0; index < namespaces_obj->GetCount(); ++index)
    {
      const TJsonNode & ns_value = namespaces_obj->Child(index);
      if (ns_value.GetKind() != nkArray) continue;
      string name = ns_value.GetName();
      vector<SDocumentSymbol> & dst = result.namespaces[name];
      for (int symbol_index = 0; symbol_index < ns_value.GetCount(); ++symbol_index)
      {
        const TJsonNode & object = ns_value.Child(symbol_index);
        string symbol_name;
        if (object.GetKind() != nkObject || !JsonString(object, "name", symbol_name)) continue;
        SDocumentSymbol symbol;
        symbol.name = symbol_name;
        symbol.kind = int(JsonInteger(JsonChild(object, "kind"), 13));
        dst.push_back(move(symbol));
      }
    }
  }
  return result;
}

void ODqLanguageServer::PublishDiagnostics(const SDocument & document, const vector<SDiagnostic> & diagnostics)
{
  TJsonNode message(nkObject);
  message.Add("jsonrpc", "2.0");
  message.Add("method", "textDocument/publishDiagnostics");
  TJsonNode & params = message.Add("params").GetAsObject();
  params.Add("uri", document.uri);
  params.Add("version", static_cast<long long>(document.version));
  TJsonNode & json_diagnostics = params.Add("diagnostics").GetAsArray();
  for (const SDiagnostic & diagnostic : diagnostics)
  {
    int severity = diagnostic.severity == "ERROR" ? 1 : (diagnostic.severity == "WARNING" ? 2 : 3);
    int line = max(0, diagnostic.line - 1);
    int character = Utf16Column(document.text, diagnostic.line, max(0, diagnostic.column - 1));
    TJsonNode & json_diagnostic = json_diagnostics.Add().GetAsObject();
    AddRange(json_diagnostic, line, character, line, character);
    json_diagnostic.Add("severity", severity);
    json_diagnostic.Add("code", diagnostic.code);
    json_diagnostic.Add("source", "dq-comp");
    json_diagnostic.Add("message", diagnostic.message);
  }
  WriteMessage(message.GetAsJson(true));
}

TJsonNode ODqLanguageServer::DocumentSymbolsJson(const SDocument & document) const
{
  TJsonNode result(nkArray);
  auto it = document_symbols.find(AbsNormPath(document.path).string());
  if (it != document_symbols.end())
  {
    for (const SDocumentSymbol & symbol : it->second)
    {
      AddDocumentSymbol(result, document, symbol);
    }
  }
  return result;
}

void ODqLanguageServer::Reanalyze()
{
  filesystem::path manifest;
  filesystem::path build_root;
  unordered_map<string, vector<SDiagnostic>> all_diagnostics;
  unordered_map<string, vector<SDocumentSymbol>> all_document_symbols;
  unordered_map<string, vector<SDocumentSymbol>> all_namespaces;
  if (StageDocuments(manifest, build_root))
  {
    for (const auto & [uri, document] : documents)
    {
      if (document.path.extension() != ".dq") continue;
      SWorkerResult worker_result = RunWorker(document.path, manifest, build_root);
      for (SDiagnostic & diagnostic : worker_result.diagnostics)
      {
        all_diagnostics[diagnostic.path].push_back(move(diagnostic));
      }
      for (SDocumentSymbol & symbol : worker_result.document_symbols)
      {
        all_document_symbols[symbol.path].push_back(move(symbol));
      }
      for (auto & [ns_name, ns_symbols] : worker_result.namespaces)
      {
        all_namespaces[ns_name] = move(ns_symbols);
      }
    }
  }
  document_symbols = move(all_document_symbols);
  namespaces = move(all_namespaces);
  for (const auto & [uri, document] : documents)
  {
    auto it = all_diagnostics.find(AbsNormPath(document.path).string());
    PublishDiagnostics(document, it == all_diagnostics.end() ? vector<SDiagnostic>() : it->second);
  }
}

static int DocumentSymbolToCompletionKind(int kind)
{
  switch (kind)
  {
    case 12: return 3; // Function
    case 13: return 6; // Variable
    case 14: return 21; // Constant
    case 7:  return 10; // Property
    case 10: return 13; // Enum
    case 5:  return 7; // Class
    case 23: return 22; // Struct
  }
  return 6; // Variable default
}

void ODqLanguageServer::Handle(const TJsonNode & request)
{
  string method;
  if (!JsonString(request, "method", method))
  {
    if (IsRequest(request)) RespondError(&request, -32600, "Invalid Request");
    return;
  }
  const TJsonNode * params = JsonChild(request, "params");
  if (params && params->GetKind() != nkObject) params = nullptr;
  if (method == "initialize")
  {
    if (!IsRequest(request)) return;
    if (initialize_received)
    {
      if (IsRequest(request)) RespondError(&request, -32600, "Initialize request already received");
      return;
    }
    initialize_received = true;
    TJsonNode result(nkObject);
    TJsonNode & capabilities = result.Add("capabilities").GetAsObject();
    capabilities.Add("positionEncoding", "utf-16");
    TJsonNode & text_document_sync = capabilities.Add("textDocumentSync").GetAsObject();
    text_document_sync.Add("openClose", true);
    text_document_sync.Add("change", 1);
    text_document_sync.Add("save").GetAsObject().Add("includeText", false);
    capabilities.Add("documentSymbolProvider", true);
    TJsonNode & completion_provider = capabilities.Add("completionProvider").GetAsObject();
    completion_provider.Add("resolveProvider", false);
    TJsonNode & trigger_characters = completion_provider.Add("triggerCharacters").GetAsArray();
    trigger_characters.Add().SetAsString(".");
    trigger_characters.Add().SetAsString("@");
    result.Add("serverInfo").GetAsObject().Add("name", "dq-comp");
    Respond(request, result);
    return;
  }
  if (method == "initialized")
  {
    if (initialize_received && !shutdown_requested)
    {
      initialized = true;
      Reanalyze();
    }
    return;
  }
  if (method == "shutdown")
  {
    if (!IsRequest(request)) return;
    if (!initialize_received)
    {
      if (IsRequest(request)) RespondError(&request, -32002, "Server not initialized");
      return;
    }
    shutdown_requested = true;
    TJsonNode result(nkNull);
    Respond(request, result);
    return;
  }
  if (method == "exit")
  {
    exit_status = shutdown_requested ? 0 : 1;
    should_exit = true;
    return;
  }
  if ((method == "$/setTrace") || (method == "$/cancelRequest")) return;
  if (!initialize_received || !initialized)
  {
    if (IsRequest(request)) RespondError(&request, -32002, "Server not initialized");
    return;
  }
  if (shutdown_requested)
  {
    if (IsRequest(request)) RespondError(&request, -32600, "Server is shut down");
    return;
  }
  if (!params || ((method != "textDocument/didOpen") && (method != "textDocument/didChange")
                  && (method != "textDocument/didClose") && (method != "textDocument/didSave")
                  && (method != "textDocument/documentSymbol") && (method != "textDocument/completion")))
  {
    if (IsRequest(request)) RespondError(&request, -32601, "Method not found");
    return;
  }

  const TJsonNode * text_document = JsonChild(*params, "textDocument");
  if (!text_document || text_document->GetKind() != nkObject) return;
  string uri;
  if (!JsonString(*text_document, "uri", uri)) return;
  if (method == "textDocument/documentSymbol")
  {
    auto it = documents.find(uri);
    TJsonNode result = it == documents.end() ? TJsonNode(nkArray) : DocumentSymbolsJson(it->second);
    Respond(request, result);
    return;
  }
  if (method == "textDocument/completion")
  {
    auto it = documents.find(uri);
    if (it == documents.end())
    {
      TJsonNode result(nkArray);
      Respond(request, result);
      return;
    }
    const SDocument & document = it->second;
    const TJsonNode * position = JsonChild(*params, "position");
    if (position && position->GetKind() != nkObject) position = nullptr;
    int line = position ? int(JsonInteger(JsonChild(*position, "line"), 0)) : 0;
    int character = position ? int(JsonInteger(JsonChild(*position, "character"), 0)) : 0;
    
    size_t line_start = 0;
    for (int l = 0; (l < line) && (line_start < document.text.size()); ++l)
    {
      size_t line_end = document.text.find('\n', line_start);
      line_start = (line_end == string::npos ? document.text.size() : line_end + 1);
    }
    size_t line_end = document.text.find('\n', line_start);
    if (line_end == string::npos) line_end = document.text.size();
    
    size_t cursor = min(line_end, line_start + character);
    size_t prefix_start = cursor;
    while (prefix_start > line_start && (isalnum(document.text[prefix_start - 1]) || document.text[prefix_start - 1] == '_'))
    {
      prefix_start--;
    }
    
    string scope_name = "..";
    if (prefix_start > line_start && document.text[prefix_start - 1] == '.')
    {
      size_t dot_pos = prefix_start - 1;
      size_t ns_start = dot_pos;
      while (ns_start > line_start && (isalnum(document.text[ns_start - 1]) || document.text[ns_start - 1] == '_'))
      {
        ns_start--;
      }
      if (ns_start > line_start && document.text[ns_start - 1] == '@')
      {
        scope_name = document.text.substr(ns_start, dot_pos - ns_start);
      }
    }
    else if (prefix_start > line_start && document.text[prefix_start - 1] == '@')
    {
       // complete namespace names if they type '@'
       TJsonNode result(nkArray);
       for (const auto & [ns_name, _] : namespaces)
       {
         if (ns_name == "." || ns_name == "..") continue;
         AddCompletionItem(result, ns_name, 9); // 9 = Module/Namespace
       }
       Respond(request, result);
       return;
    }
    
    TJsonNode result(nkArray);
    if (scope_name == "..")
    {
      // The user is typing a regular identifier. In DQ, namespaces like `dq` and `def` are merged.
      // So we offer symbols from all these core scopes.
      const char* merged_scopes[] = { "..", ".", "dq", "def" };
      for (const char* ns : merged_scopes)
      {
        auto ns_it = namespaces.find(ns);
        if (ns_it != namespaces.end())
        {
          for (const auto & symbol : ns_it->second)
          {
            AddCompletionItem(result, symbol.name, DocumentSymbolToCompletionKind(symbol.kind));
          }
        }
      }
    }
    else
    {
      auto ns_it = namespaces.find(scope_name);
      if (ns_it == namespaces.end())
      {
        // Try to infer type of the variable using simple regex
        std::regex re("\\b" + scope_name + "\\s*(?::|:=)\\s*([A-Za-z0-9_]+)");
        std::smatch match;
        if (std::regex_search(document.text, match, re))
        {
          string inferred_type = match[1].str();
          ns_it = namespaces.find(inferred_type);
        }
      }
      
      if (ns_it != namespaces.end())
      {
        for (const auto & symbol : ns_it->second)
        {
          AddCompletionItem(result, symbol.name, DocumentSymbolToCompletionKind(symbol.kind));
        }
      }
    }
    Respond(request, result);
    return;
  }
  if (method == "textDocument/didSave")
  {
    if (documents.contains(uri)) Reanalyze();
    return;
  }
  if (method == "textDocument/didClose")
  {
    auto it = documents.find(uri);
    if (it != documents.end()) PublishDiagnostics(it->second, vector<SDiagnostic>());
    documents.erase(uri);
    Reanalyze();
    return;
  }
  string path = FilePathFromUri(uri);
  if (path.empty()) return;
  const TJsonNode * text_value = nullptr;
  if (method == "textDocument/didOpen") text_value = JsonChild(*text_document, "text");
  else
  {
    const TJsonNode * changes = JsonChild(*params, "contentChanges");
    if (changes && changes->GetKind() == nkArray && changes->GetCount() > 0)
    {
      const TJsonNode & change = changes->Child(0);
      if (change.GetKind() == nkObject) text_value = JsonChild(change, "text");
    }
  }
  if (!text_value || text_value->GetKind() != nkString) return;
  SDocument & document = documents[uri];
  document.uri = uri;
  document.path = filesystem::path(path);
  document.text = text_value->GetAsString();
  document.version = JsonInteger(JsonChild(*text_document, "version"), document.version + 1);
  Reanalyze();
}

int RunDqLanguageServer()
{
  ODqLanguageServer server;
  return server.Run();
}
