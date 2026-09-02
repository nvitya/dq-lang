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

static bool IsRequest(const llvm::json::Object & object)
{
  return object.get("id") != nullptr;
}

static string JsonId(const llvm::json::Object & object)
{
  const llvm::json::Value * value = object.get("id");
  if (!value) return "null";
  if (optional<int64_t> id = value->getAsInteger()) return to_string(*id);
  if (optional<llvm::StringRef> id = value->getAsString()) return "\"" + JsonEscape(id->str()) + "\"";
  return "null";
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


int ODqLanguageServer::Run()
{
  string payload;
  while (ReadMessage(payload))
  {
    auto parsed = llvm::json::parse(payload);
    if (!parsed)
    {
      RespondError(nullptr, -32700, "Parse error");
      continue;
    }
    llvm::json::Object * request = parsed->getAsObject();
    if (!request)
    {
      RespondError(nullptr, -32600, "Invalid Request");
      continue;
    }
    Handle(*request);
    if (should_exit) break;
  }
  return exit_status;
}

void ODqLanguageServer::Respond(const llvm::json::Object & request, const string & result)
{
  WriteMessage("{\"jsonrpc\":\"2.0\",\"id\":" + JsonId(request) + ",\"result\":" + result + "}");
}

void ODqLanguageServer::RespondError(const llvm::json::Object * request, int code, string_view message)
{
  string id = request ? JsonId(*request) : "null";
  WriteMessage(format("{{\"jsonrpc\":\"2.0\",\"id\":{},\"error\":{{\"code\":{},\"message\":\"{}\"}}}}",
                      id, code, JsonEscape(message)));
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

  ostringstream manifest;
  manifest << "{\"files\":[";
  bool first = true;
  size_t index = 0;
  for (const auto & [uri, document] : documents)
  {
    filesystem::path staged = job_dir / "sources" / format("{}-{}{}", index++, document.version, document.path.extension().string());
    ofstream output(staged, ios::binary);
    output.write(document.text.data(), static_cast<streamsize>(document.text.size()));
    if (!output) return false;
    if (!first) manifest << ',';
    first = false;
    manifest << "{\"source\":\"" << JsonEscape(AbsNormPath(document.path).string())
             << "\",\"staged\":\"" << JsonEscape(staged.string()) << "\"}";
  }
  manifest << "]}";
  rmanifest = job_dir / "overlay.json";
  ofstream output(rmanifest, ios::binary);
  string text = manifest.str();
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
    auto parsed = llvm::json::parse(line);
    if (!parsed) continue;  // worker status output is never protocol data.
    llvm::json::Object * object = parsed->getAsObject();
    if (!object || (object->getString("kind") != "diagnostic")) continue;
    optional<llvm::StringRef> path = object->getString("path");
    optional<llvm::StringRef> severity = object->getString("severity");
    optional<llvm::StringRef> code = object->getString("code");
    optional<llvm::StringRef> message = object->getString("message");
    if (!path || !message) continue;
    SDiagnostic diagnostic;
    diagnostic.path = AbsNormPath(path->str()).string();
    diagnostic.severity = severity ? severity->str() : "ERROR";
    diagnostic.code = code ? code->str() : "";
    diagnostic.message = message->str();
    diagnostic.line = int(object->getInteger("line").value_or(1));
    diagnostic.column = int(object->getInteger("column").value_or(1));
    result.diagnostics.push_back(move(diagnostic));
  }

  ifstream result_file(result_path, ios::binary);
  string result_text((istreambuf_iterator<char>(result_file)), istreambuf_iterator<char>());
  auto parsed_result = llvm::json::parse(result_text);
  llvm::json::Object * root = parsed_result ? parsed_result->getAsObject() : nullptr;
  llvm::json::Array * symbols = root ? root->getArray("documentSymbols") : nullptr;
  if (symbols)
  {
    for (llvm::json::Value & value : *symbols)
    {
      llvm::json::Object * object = value.getAsObject();
      if (!object) continue;
      optional<llvm::StringRef> path = object->getString("path");
      optional<llvm::StringRef> name = object->getString("name");
      if (!path || !name) continue;
      SDocumentSymbol symbol;
      symbol.path = AbsNormPath(path->str()).string();
      symbol.name = name->str();
      symbol.kind = int(object->getInteger("kind").value_or(13));
      symbol.line = int(object->getInteger("line").value_or(1));
      symbol.column = int(object->getInteger("column").value_or(1));
      result.document_symbols.push_back(move(symbol));
    }
  }
  return result;
}

void ODqLanguageServer::PublishDiagnostics(const SDocument & document, const vector<SDiagnostic> & diagnostics)
{
  string message = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":\""
      + JsonEscape(document.uri) + "\",\"version\":" + to_string(document.version) + ",\"diagnostics\":[";
  for (size_t i = 0; i < diagnostics.size(); ++i)
  {
    const SDiagnostic & diagnostic = diagnostics[i];
    if (i) message += ',';
    int severity = diagnostic.severity == "ERROR" ? 1 : (diagnostic.severity == "WARNING" ? 2 : 3);
    int line = max(0, diagnostic.line - 1);
    int character = Utf16Column(document.text, diagnostic.line, max(0, diagnostic.column - 1));
    message += format("{{\"range\":{{\"start\":{{\"line\":{},\"character\":{}}},\"end\":{{\"line\":{},\"character\":{}}}}},\"severity\":{},\"code\":\"{}\",\"source\":\"dq-comp\",\"message\":\"{}\"}}",
                      line, character, line, character, severity, JsonEscape(diagnostic.code), JsonEscape(diagnostic.message));
  }
  message += "]}}";
  WriteMessage(message);
}

string ODqLanguageServer::DocumentSymbolsJson(const SDocument & document) const
{
  string result = "[";
  auto it = document_symbols.find(AbsNormPath(document.path).string());
  if (it == document_symbols.end()) return result + "]";
  for (size_t i = 0; i < it->second.size(); ++i)
  {
    const SDocumentSymbol & symbol = it->second[i];
    if (i) result += ',';
    int line = max(0, symbol.line - 1);
    int character = DocumentSymbolNameColumn(document, symbol);
    int end_character = character + Utf16TextWidth(symbol.name);
    result += format("{{\"name\":\"{}\",\"kind\":{},\"range\":{{\"start\":{{\"line\":{},\"character\":{}}},\"end\":{{\"line\":{},\"character\":{}}}}},\"selectionRange\":{{\"start\":{{\"line\":{},\"character\":{}}},\"end\":{{\"line\":{},\"character\":{}}}}}}}",
                     JsonEscape(symbol.name), symbol.kind, line, character, line, end_character,
                     line, character, line, end_character);
  }
  return result + "]";
}

void ODqLanguageServer::Reanalyze()
{
  filesystem::path manifest;
  filesystem::path build_root;
  unordered_map<string, vector<SDiagnostic>> all_diagnostics;
  unordered_map<string, vector<SDocumentSymbol>> all_document_symbols;
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
    }
  }
  document_symbols = move(all_document_symbols);
  for (const auto & [uri, document] : documents)
  {
    auto it = all_diagnostics.find(AbsNormPath(document.path).string());
    PublishDiagnostics(document, it == all_diagnostics.end() ? vector<SDiagnostic>() : it->second);
  }
}

void ODqLanguageServer::Handle(const llvm::json::Object & request)
{
  optional<llvm::StringRef> method_ref = request.getString("method");
  if (!method_ref)
  {
    if (IsRequest(request)) RespondError(&request, -32600, "Invalid Request");
    return;
  }
  string method = method_ref->str();
  const llvm::json::Object * params = request.getObject("params");
  if (method == "initialize")
  {
    if (!IsRequest(request)) return;
    if (initialize_received)
    {
      if (IsRequest(request)) RespondError(&request, -32600, "Initialize request already received");
      return;
    }
    initialize_received = true;
    Respond(request, "{\"capabilities\":{\"positionEncoding\":\"utf-16\",\"textDocumentSync\":{\"openClose\":true,\"change\":1,\"save\":{\"includeText\":false}},\"documentSymbolProvider\":true},\"serverInfo\":{\"name\":\"dq-comp\"}}" );
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
    Respond(request, "null");
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
                  && (method != "textDocument/documentSymbol")))
  {
    if (IsRequest(request)) RespondError(&request, -32601, "Method not found");
    return;
  }

  const llvm::json::Object * text_document = params->getObject("textDocument");
  if (!text_document) return;
  optional<llvm::StringRef> uri_ref = text_document->getString("uri");
  if (!uri_ref) return;
  string uri = uri_ref->str();
  if (method == "textDocument/documentSymbol")
  {
    auto it = documents.find(uri);
    Respond(request, it == documents.end() ? "[]" : DocumentSymbolsJson(it->second));
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
  const llvm::json::Value * text_value = nullptr;
  if (method == "textDocument/didOpen") text_value = text_document->get("text");
  else
  {
    const llvm::json::Array * changes = params->getArray("contentChanges");
    if (changes && !changes->empty())
    {
      const llvm::json::Object * change = (*changes)[0].getAsObject();
      if (change) text_value = change->get("text");
    }
  }
  if (!text_value) return;
  optional<llvm::StringRef> text = text_value->getAsString();
  if (!text) return;
  SDocument & document = documents[uri];
  document.uri = uri;
  document.path = filesystem::path(path);
  document.text = text->str();
  document.version = text_document->getInteger("version").value_or(document.version + 1);
  Reanalyze();
}

int RunDqLanguageServer()
{
  ODqLanguageServer server;
  return server.Run();
}
