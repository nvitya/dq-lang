# DQ Language Server

## Status and terminology

This document specifies the first implementable DQ language server. The key
words **must**, **should**, and **may** are normative.

The server is part of `dq-comp` and is started with:

```text
dq-comp --langserver [language-analysis options] [project.dqproj]
```

Language-server-specific code belongs in `compiler/langserver`. Small hooks and
data structures needed to expose parser or semantic information may live with
the compiler object that owns that information.

The first implementation targets the stable core of
[Language Server Protocol 3.17](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/).
It must not depend on proposed or editor-specific protocol extensions.

### Implemented baseline

The current implementation provides the transport and compiler integration on
which the rest of this specification builds.  It supports `initialize`,
`initialized`, `shutdown`, `exit`, `textDocument/didOpen`, full
`textDocument/didChange`, `textDocument/didSave`, and
`textDocument/didClose`.  It publishes compiler diagnostics for every open
`.dq` root and stages all open DQ documents in a private temporary directory
before invoking a fresh compiler worker.  The baseline also validates the
initialize/shutdown lifecycle and returns JSON-RPC parse, invalid-request,
not-initialized, and unknown-method errors for requests.

It deliberately advertises only full document synchronization and diagnostics.
Hover, navigation, completion, signature help, and document symbols remain
specified below as the next semantic-index increment; they are not advertised
until that index is implemented.  Likewise, cancellation, debounce, file
watchers, request error responses, and persistent interface caching are design
requirements for that increment rather than properties of the baseline.

## Goals

The planned semantic release must provide:

- compiler diagnostics for the current, possibly unsaved, contents of `.dq`,
  `.dqh`, and module-owned `.dqi` files;
- hover information;
- go to definition;
- context-sensitive completion;
- signature help;
- hierarchical document symbols;
- correct project, target, package-path, module, include, and preprocessor
  semantics shared with normal compilation; and
- a reusable protocol implementation, independent of VS Code.

It must use compiler parsing and name resolution. A second, approximate DQ
parser in the server is not acceptable.

The following are not part of the first release:

- references, rename, workspace symbols, call hierarchy, or type hierarchy;
- formatting, code actions, semantic tokens, inlay hints, or folding ranges;
- TCP, WebSocket, or named-pipe transports;
- simultaneous analysis with different targets in one server process; and
- linking, execution, or generation of `.o`, bitcode, or executable files.

Analysis may generate `.dqm_if` files and semantic-index files, but only below
the server's private temporary directory. Module analysis is intentionally
performed by normal one-shot `dq-comp` child processes so the existing module
compiler and freshness logic remain authoritative.

These exclusions are protocol behavior: the server must not advertise the
corresponding capabilities.

## Process and command-line behavior

`--langserver` selects language-server mode before normal input validation. A
`.dq` input is therefore not required. The optional positional `.dqproj` file
provides the compilation context and its `main` file becomes an initial
analysis root.

Language-server mode accepts options that can change language semantics or
source resolution, including target, CPU features, runtime feature switches,
defines, package paths, module roots, and build tags. `-v`, `-vv`, and `-vvv`
control server logging to standard error. Command-line options continue to
override project-file values.

Options which request compiler output or an action must be rejected with a
clear standard-error message and exit status 2. This includes `-o`, `-c`,
`--link`, `--linker-arg`, `--ifgen`, `--ifdump`, and `-ir`. Optimization, debug,
link, output, and LTO properties read from a project file are retained in the
loaded project model but ignored by analysis. Recognized command-line
optimization options, `-g`, and recognized `--lto` forms are accepted and
ignored as well, so an editor may reuse harmless compiler settings without
changing language-server behavior. With `-vv` or higher, the server logs each
ignored option once.

`dq-comp` also has an internal worker invocation used only by the server:

```text
dq-comp --langserver-worker \
  --source-overlay <overlay-manifest.json> \
  --diagnostic-format=jsonl \
  --build-root <temporary-build-root> \
  --package-build-root <temporary-build-root> \
  <original-root.dq> \
  [language-analysis options] [project.dqproj]
```

Worker mode is one-shot. It performs interface-only compilation of the root,
allows the normal module loader to invoke further `dq-comp --ifgen` processes,
and returns the normal compiler success/failure status. The server passes private
temporary build roots to every worker. The overlay, diagnostic mode, and build
roots are propagated by `ModuleChildArgs` to every nested module compilation.
The current worker stream contains JSON-Lines diagnostics; the semantic result
file is part of the planned semantic-index increment. Worker-only switches are
hidden from normal usage text.
Root workers use the language server's original launch directory, not the
temporary directory, so relative command-line and project paths keep their
normal meaning.

Only standard-I/O transport is supported. Standard input and output are binary
streams on Windows. In `--langserver` mode, standard output is reserved
exclusively for LSP frames. In worker mode it is reserved for the JSON-Lines
diagnostic stream. All logs and crash information go to standard error. No
parser, module loader, or compiler helper may write unstructured text to either
reserved stream.

Malformed server command-line configuration is a launch failure and occurs
before LSP initialization. Errors discovered after initialization are reported
through `window/logMessage` in addition to standard-error logging.

## Protocol transport and lifecycle

Messages use JSON-RPC 2.0 and LSP `Content-Length` framing. Header names are
case-insensitive. The transport must accept additional headers, `\r\n` line
endings, partial reads, and multiple messages in one read. It must reject a
missing, duplicate, malformed, or unreasonably large `Content-Length` without
attempting to resynchronize past arbitrary payload bytes. The initial maximum
message size is 64 MiB. Because a framing error makes the next message boundary
unknowable, the server logs it and terminates with status 1; JSON syntax errors
inside a correctly framed payload receive a JSON-RPC parse-error response.

Use LLVM's existing `llvm::json` support rather than adding another JSON
dependency. Parsing and dispatch must validate required fields and return the
standard JSON-RPC errors for parse errors, invalid requests, unknown methods,
invalid parameters, and internal errors. An invalid request must not terminate
the server.

The supported lifecycle is:

1. `initialize` must be the first request.
2. `initialized` enables diagnostics and dynamic file-watcher registration.
3. `shutdown` stops accepting work, cancels and joins the active process tree,
   then returns `null`; it does not exit.
4. `exit` removes the verified temporary instance directory and terminates with
   status 0 after `shutdown`, or status 1 otherwise.
5. End of standard input cancels workers, performs the same verified temporary
   cleanup, and terminates cleanly when there is no partially framed message.

Before `initialize`, requests other than `initialize` receive `ServerNotInitialized`.
After `shutdown`, further requests receive `InvalidRequest`. Unknown
notifications are ignored. `$/setTrace` and `$/cancelRequest` must be accepted.
Cancellation is handled by the worker scheduler described below; a result
computed after cancellation or supersession must be discarded.

The server reports `serverInfo.name` as `dq-comp` and uses the compiler version
as `serverInfo.version`.

## Advertised capabilities

The planned semantic implementation advertises this surface:

```json
{
  "positionEncoding": "utf-16",
  "textDocumentSync": {
    "openClose": true,
    "change": 1,
    "save": { "includeText": false }
  },
  "hoverProvider": true,
  "definitionProvider": true,
  "completionProvider": {
    "resolveProvider": false,
    "triggerCharacters": [".", "@"]
  },
  "signatureHelpProvider": {
    "triggerCharacters": ["(", ","]
  },
  "documentSymbolProvider": true,
  "workspace": {
    "workspaceFolders": {
      "supported": true,
      "changeNotifications": true
    }
  }
}
```

The implemented baseline instead advertises:

```json
{
  "positionEncoding": "utf-16",
  "textDocumentSync": {
    "openClose": true,
    "change": 1,
    "save": { "includeText": false }
  }
}
```

`TextDocumentSyncKind.Full` is deliberate. It keeps the document store and
version checks simple; incremental synchronization can be added without
changing the analysis interfaces.

LSP positions are zero-based UTF-16 code-unit positions. The compiler's current
line and column values are one-based byte-oriented positions and must never be
copied directly into protocol messages.

## Compilation context

One server process has one immutable semantic compilation context: target,
runtime feature settings, command-line defines, package paths, module roots,
and optional project. Workspace folders only determine which files belong to
the workspace; they do not create separate targets. An editor that opens
projects requiring different contexts must start one server per project.

No automatic `.dqproj` selection is performed in the first release. A project
is selected by the optional command-line project argument. This avoids silently
using the wrong project when a workspace contains several project files.

The VS Code client may pass these non-semantic initialization options under
`initializationOptions.dq`:

```json
{
  "analysisDebounceMs": 150,
  "maxCompletionItems": 200,
  "workerTimeoutMs": 120000
}
```

The debounce is clamped to 0-2000 ms, the item limit to 20-1000, and the worker
timeout to 10000-600000 ms. Unknown fields are ignored. Settings that alter the
semantic compilation context require a server restart in the first release.

## Documents, paths, and source overlays

The document store contains the URI, canonical platform path, language id,
client version, complete UTF-8 text, and a line index for every open document.
Only `file:` URIs are analyzed. Requests for another URI scheme return an empty
language result, not a filesystem path guessed from the URI.

One platform-aware path helper must own URI conversion and canonicalization.
It resolves absolute paths lexically, uses `weakly_canonical` where possible,
normalizes Windows drive letters and separators, and applies the platform's
case-comparison rules. Every subsystem uses the resulting key; it is not
permitted to key one cache by URI and another by the spelling of a path.

Open document contents are materialized as ordinary files below the private
temporary directory. Every analysis job writes an immutable JSON overlay
manifest of the form:

```json
{
  "files": [
    {
      "source": "/original/workspace/file.dq",
      "staged": "/tmp/dq-lsp-.../job-.../sources/0-17.dq"
    }
  ]
}
```

`source` is the canonical logical path used for include resolution, module
identity, source metadata, and diagnostics. `staged` is the staged regular
file read by the compiler. The baseline records the staged file's ordinary size
and timestamp for interface freshness; a content fingerprint is reserved for
the semantic-index cache.

Worker processes install a small manifest-backed source provider. A read of a
mapped logical path opens `staged`; an unmapped path opens the logical path
from the filesystem. Path resolution always uses the logical path, so relative
includes and module identities behave exactly as they do in a normal build.
The manifest applies to main modules, same-basename `.dqh` files, `#include`
files, `#srcdep` freshness checks, and source modules loaded for `use`. Every
nested `dq-comp` inherits the same immutable manifest.

Source existence and metadata queries must use the provider too; in particular,
`OScFeederDq::ResolveSourcePath`, `ModuleSourceExists`, `AddSourceDependency`, and
`OModuleIntf::MetadataMatchesSources` cannot bypass it with direct filesystem
calls. This allows a newly created, unsaved module that exists only in the
manifest to participate in module resolution and freshness checks.

This source-provider hook belongs in `OScFeederDq`/`OScFile` and module
freshness checks. It is only a file redirection layer; the server does not keep
compiler ASTs, scopes, module interfaces, or compiler global state in memory.
Each compiler process remains one-shot.

The staged content file and manifest are written atomically before the worker
starts and remain immutable until every process in that job exits. New semantic
records use logical source paths and byte offsets, never temporary paths or
persistent source pointers.

`didOpen` installs an overlay, `didChange` replaces the entire overlay and
requires a strictly newer document version, and `didClose` removes the overlay.
An out-of-order `didChange` is ignored and logged; notifications cannot receive
an error response.
Closing a dirty dependency causes its remaining owner roots to be reanalyzed
from disk. Closing a non-project `.dq` also removes it as a root; it is only
reanalyzed if another root uses it as a module. A late analysis result whose
captured revision no longer matches the document store must not be published.

The server dynamically registers for changes to `**/*.dq`, `**/*.dqh`,
`**/*.dqi`, `**/*.dqproj`, and `**/*.dqm_if` when the client supports watched
files. Otherwise, open/change/save notifications still provide correct results
for open files, while external disk edits may not be noticed until the next
analysis-triggering request. A change to the selected `.dqproj` is logged and
reported with `window/showMessage`; changing semantic project settings requires
the client to restart the server.

## Temporary directory and artifact policy

At startup the server creates a private process directory with a random suffix
under `std::filesystem::temp_directory_path()`, conceptually:

```text
dq-lsp-<pid>-<random>/
  owner.json
  overlays/<logical-path-hash>/<content-hash>/<filename>
  manifests/<job-id>.json
  results/<job-id>.json
  build-root/.dqbuild/<build-tag>/local/...
  package-build-root/.dqbuild/<build-tag>/pkg/...
```

Creation must fail rather than reuse an existing directory. Permissions should
allow only the current user where the platform supports it. `owner.json`
contains the server PID, start time, compiler version, and random instance id;
it is diagnostic metadata and must not be treated as proof that another process
is dead.

The server overrides `--build-root` and `--package-build-root` for workers so
all generated interfaces and locks stay below `build/`. A project or client
cannot override these private paths. Workers enable interface generation and
artifact refresh, while forcing debug information, IR output, object
generation, LTO, and linking off. Consequently only `.dqm_if`, lock, result,
and diagnostic data can be created. User source trees and their normal
`.dqbuild` directories are read-only from the language server's point of view.

Staged contents are content-addressed and may be reused across jobs. Manifests
and result files are job-specific. Completed job files may be removed after the
result is accepted, but interface artifacts are cached for the life of the
server. On a clean shutdown, the server removes only its verified instance
directory. It must check the expected parent, directory-name prefix, and
matching owner instance id before recursive removal. A crash may leave the
directory behind; automatic deletion of other stale instance directories is
out of scope for the first release.

For temporary interfaces, a source dependency's freshness value comes from the
active source provider. Disk files use the existing size and modification time.
Mapped files use their staged size and manifest fingerprint. This may reuse the
existing 64-bit file-time field as an analysis-only freshness token because
temporary interfaces never leave the private build tree; normal compiler
artifacts retain their existing on-disk timestamp semantics. Adding an explicit
fingerprint record to the interface format is also valid, but is not required
for the first implementation.

Removing an overlay or changing its contents therefore makes only interfaces
that recorded that logical source dependency stale. Unchanged module
interfaces remain reusable across edits. Temporary artifact locking and atomic
replacement continue to use the compiler's existing mechanisms.

## Analysis roots and dependency graph

Every open `.dq` file and the selected project's `main` file is an analysis
root. A same-basename `.dqh` file is owned by its `.dq` module. A `.dqi` file is
owned by each root that reaches it through `#include`. The server maintains
reverse edges so changing an included file reanalyzes all affected roots.
`#srcdep` creates an invalidation edge but does not parse the referenced file.

If an opened `.dqh` has a same-basename `.dq`, that `.dq` is scheduled even if
it is not open. An opened `.dqi` with no known owner cannot be interpreted in a
module scope, so diagnostics and semantic requests return empty results until a
root includes it. The server must not invent a standalone grammar or name
resolution mode for fragments.

Module `use` edges are part of the same graph. Analysis-only module loading
must behave as follows:

1. Module artifact paths resolve below the private temporary build roots.
2. A fresh compatible temporary `.dqm_if` provides the imported module's public
   semantic interface.
3. If it is stale or missing, the existing interface freshness path invokes a
   nested `dq-comp --ifgen` with the same project semantics, overlay manifest,
   diagnostic format, and temporary build roots.
4. Existing module-cycle detection, artifact locks, and `use`
   selection/re-export rules apply unchanged.
5. If source locations are needed for navigation, the server schedules a
   separate semantic worker for that module source and caches its result.

The current `.dqm_if` format contains module source metadata but not declaration
positions. Therefore a temporary compiled interface is sufficient for type
checking, hover, and completion, but source analysis is required before
definition navigation can return the imported declaration.

A semantic cache key includes the canonical module path, effective compilation
context, and revisions of all files read by that analysis. An entry is usable
only if all captured revisions still match; changes to unrelated open files do
not invalidate it.
Compiled interface reuse is independently decided by the compiler's freshness
metadata within the temporary artifact tree.

Diagnostics and semantic records are retained per root. Diagnostics published
for one URI are the deduplicated union of contributions from all current roots
that read that URI. Reanalyzing or removing a root first removes its old
contribution, preventing one root from accidentally clearing another root's
diagnostics.

Nested compiler diagnostics are cached by logical path and source fingerprint.
When a later root worker reuses a temporary interface and therefore emits no
new child output, the root contribution uses the matching cached diagnostics
listed by that interface's source fingerprints. A changed fingerprint never
reuses the old diagnostics; a successful recompilation with no diagnostics
therefore clears them deterministically.

## Compiler worker contract

The long-lived language-server process retains a normalized copy of compiler
configuration but does not initialize or retain parser, AST, module, builtin,
define, namespace, or LLVM state. It launches the same `dq-comp` executable in
`--langserver-worker` mode for each root analysis. This preserves the current
one-compilation-per-process ownership model for `g_opt`, `g_compiler`,
`g_module`, `g_builtins`, `g_defines`, and `g_namespaces` inside workers.
The launcher uses the absolute executable path established by
`InitializeCompilerExecutable`; workers propagate that path to module children
so analysis cannot accidentally mix compiler versions from `PATH`.

Worker mode shares the normal compiler setup and parser, but has a separate
driver around the parsing portion of `ODqCompiler::Run()`. It performs
preprocessing, parsing, name resolution, type checking, dependency interface
compilation/loading, and temporary root interface emission. It does not
initialize debug information, generate LLVM IR, emit an object, or link. It
must serialize a partial result after recoverable source errors, before normal
process teardown.

The worker pre-scans and validates its internal paths, loads the overlay, and
installs the JSON diagnostic sink before loading a project or initializing the
compiler. Startup and project diagnostics must therefore obey the same
machine-readable output contract as parser diagnostics.

The result file is UTF-8 JSON with this versioned top-level shape:

```json
{
  "formatVersion": 1,
  "root": "/logical/root.dq",
  "success": false,
  "cancelled": false,
  "files": [
    { "path": "/logical/root.dq", "fingerprint": "..." }
  ],
  "includes": [],
  "sourceDependencies": [],
  "modules": [],
  "semanticIndex": {}
}
```

`files` lists every parsed source with the exact provider fingerprint used.
`includes`, `sourceDependencies`, and `modules` contain resolved logical paths
and the source range of the corresponding directive/use. Each module entry also
contains the source path/fingerprint pairs recorded by its loaded temporary
interface. `semanticIndex` holds the records defined below. Diagnostics travel
separately as JSON Lines on the worker's captured standard output so diagnostics
emitted by nested module compilers can be forwarded without merging result
files.

The worker writes the result to a sibling temporary file, closes it, and
atomically replaces the requested result path. A missing, malformed, unknown
version, or fingerprint-mismatched result is a failed analysis even if the
worker exited successfully. A nonzero worker exit with a valid partial result
still publishes its source diagnostics and partial semantic information.

All paths inside the result and diagnostic stream are logical canonical source
paths. Temporary content paths and artifact paths are never exposed through
LSP.

### Structured diagnostics

`ODqCompBase::Error`, `Warning`, and `Hint` must report an `SDqDiagnostic` to a
diagnostic sink instead of formatting directly to standard output. The record
contains:

```text
severity: error | warning | hint
id: stable DQ diagnostic identifier
message: formatted message
primary_range: source path plus half-open byte offsets
related: optional source ranges and messages
```

The normal compiler uses a text sink that preserves its existing output and
error counters. A worker uses a JSON-Lines sink selected by an internal
diagnostic-format option:

```json
{"kind":"diagnostic","path":"/logical/file.dq","start":12,"end":16,"severity":"error","code":"TypeSpecExpected","message":"...","related":[]}
```

There is exactly one JSON object per line. `start` and `end` are half-open byte
offsets in the logical source. `related` contains objects with `path`, `start`,
`end`, and `message`. The server treats a non-JSON standard-output line as a
worker protocol error and sends captured standard error only to its log.

The diagnostic format and overlay option are inherited by module children.
`RunModuleChildCompile` forwards a child's diagnostic lines to its own standard
output on both success and failure; this is necessary because a successful
module compile may still emit warnings or hints. Ordinary verbose/status output
is disabled in workers.

Raw module-loader and source-loader failures reachable during analysis must
also become structured diagnostics. A failure with no useful position is
attached to byte offset zero of the requesting root. If a worker crashes before
emitting a usable diagnostic, the server publishes one `AnalysisWorkerFailed`
diagnostic at the root and logs the captured process details.

### Source ranges and semantic instrumentation

Add a half-open `OScRange` (source file, start byte, end byte) alongside
`OScPosition`. Declaration and reference ranges must cover the identifier token,
not leading attributes or whitespace. Compound declarations also record a full
range from their leading keyword through the closing keyword or brace.

The parser and semantic resolver populate a semantic index as names are
declared and resolved. Instrument the points that already know the answer; do
not perform a second textual name-resolution pass. The index contains:

```text
SymbolRecord:
  stable internal id
  kind and name
  owner/module identity
  declaration name range, optional full range
  display signature and optional constant value

OccurrenceRecord:
  source range
  target symbol id, or unresolved
  declaration | read | write | call | type | namespace role

TokenRecord:
  source range
  identifier | keyword | number | string | character | comment | punctuation
  active-preprocessor-branch flag

ScopeRecord:
  source path, range, and parent scope id
  value, type, and namespace bindings with their visibility start offsets

CallRecord:
  callee range
  candidate signature ids
  selected overload, if resolved
  argument ranges

DocumentSymbolRecord:
  symbol id, parent id, full range, selection range

PathRecord:
  include or use spelling range
  resolved source path, if any
```

The existing feeder emits `TokenRecord` values while it scans, including
comments and inactive preprocessor regions. Inactive tokens have no semantic
occurrences. These records support cursor classification and incomplete-call
recovery without adding another lexer.

Local symbol ids include their declaration file and byte offset. Module-level
ids include canonical module identity, owner chain, symbol kind, and overload
signature. IDs are internal and are never exposed as a stable public format.

Display text belongs on the relevant symbol/type objects through concise
formatting methods, with a small dispatcher only where several symbol kinds
must be combined. This keeps hover, completion, and signature help consistent
and avoids a broad server-side duplicate type printer.

A scope that is active in several included source segments produces one
`ScopeRecord` per contiguous file segment. Bindings are filtered at the cursor's
offset so completion does not expose a local declaration before the parser
would make it visible. An `OScRange` never crosses files. If textual inclusion
causes a declaration to begin and end in different files, its full range is
omitted and document-symbol range falls back to the source-local name or
declaration segment.

## Position and range conversion

Each source snapshot builds a line table containing the starting byte offset of
every line. Conversion between byte offsets and LSP positions must:

- treat CRLF as one line break and accept lone CR or LF;
- count Unicode supplementary characters as two UTF-16 code units;
- clamp an end-of-file position to the end of the last line;
- reject a client position in the middle of a surrogate pair; and
- remain well-defined for invalid UTF-8 by treating each invalid byte as one
  replacement character and producing an `InvalidUtf8` diagnostic.

A text-document request containing an unmappable position receives
`InvalidParams`. A notification containing one is ignored and logged.

Compiler point diagnostics are expanded to the offending token when a token
range is known. Otherwise they cover the next Unicode code point on the same
line, or are zero-width at end of line/file. Protocol ranges are always
half-open.

## Feature behavior

All text-document requests use the exact URI and latest version received before
the request. If its analysis is pending, the request bypasses the diagnostic
debounce and waits for that version unless cancelled. Results from an older
snapshot must not be returned as if they described the current text.

Parsing errors do not invalidate all semantic features. The parser's recovered,
partial declarations and occurrences are included in the snapshot. A feature
returns `null`, an empty array, or an incomplete completion list only where the
required semantic fact could not be established.

### Diagnostics

Diagnostics use `textDocument/publishDiagnostics` (push diagnostics). For open
documents the notification includes the analyzed document version. Each item
has:

- `source: "dq"`;
- `code`: the stable diagnostic id such as `TypeSpecExpected`;
- the mapped severity (`Error` 1, `Warning` 2, `Hint` 4); and
- `relatedInformation` when the compiler supplies related ranges and the client
  supports it.

Identical `(range, severity, code, message)` entries are deduplicated and sorted
by range, severity, and code. At most 1000 diagnostics are published per file;
the last item reports that additional diagnostics were suppressed. Successful
analysis publishes an empty array when necessary to clear previous results.
Closing a file publishes the current disk-backed result without a version, or
an empty array if it has no remaining root contribution.

Analysis after a text change is debounced by the configured interval. Changes
to a dependency coalesce all affected roots into one work batch.

### Hover

`textDocument/hover` finds the smallest occurrence containing the cursor. It
returns `null` in comments, ordinary string content, whitespace, and for an
unresolved name. The result uses `MarkupContent` with `kind: "markdown"`, the
occurrence range, and a fenced `dq` declaration such as:

```dq
function Write(ref value : T, count : int = 1) : bool
```

Types, variables, constants, fields, properties, functions, overloads, enum
items, namespaces, and builtins are supported. An overload set shows each
candidate on a separate line, capped at 20. Constant values are shown only when
the compiler already has a safe printable representation.

### Go to definition

`textDocument/definition` returns a `Location[]`:

- a resolved symbol use returns its declaration name range;
- a selected overload call returns that overload;
- an unresolved overload-set use returns all source-backed candidates;
- an include path returns the included file at `(0, 0)`;
- a module-use path returns the module source at `(0, 0)`; and
- a declaration returns its own location.

Builtins and imported symbols whose source cannot be located return an empty
array. An imported interface with source metadata is lazily source-analyzed so
that navigation reaches the declaration rather than the `.dqm_if` file.

### Completion

Completion is suppressed in comments and in string/character literals, except
for a future path-completion implementation. It classifies the cursor using the
token stream plus the innermost `ScopeRecord`:

- after `@`, offer named namespaces;
- after `@namespace.`, offer that namespace's visible members;
- after a value/member `.`, offer members of the resolved compound type,
  respecting visibility;
- in a type position, offer visible types and type-forming keywords;
- at module root, offer legal root keywords and visible module symbols; and
- in a statement/expression, offer legal keywords plus visible values, types,
  and builtins.

Items set `label`, `kind`, `detail`, and `sortText`. `detail` uses the same
display formatter as hover. The first release inserts plain identifiers and
does not insert argument snippets or additional imports. Private or shadowed
symbols that normal DQ lookup cannot select are omitted. Results are sorted by
exact prefix match, local-before-imported scope distance, symbol kind, and
label. If the configured item limit is reached, return `isIncomplete: true`.

The semantic index must retain enough scope information before a syntax error
to complete at the error site. If no semantic scope covers the cursor, lexical
keyword completion is still allowed.

### Signature help

Signature help is triggered by `(` and `,` and may also be requested manually.
It identifies the innermost call containing the cursor, ignoring nested calls,
comments, and strings when counting commas. It returns all viable overloads in
compiler overload order, the compiler-selected overload as `activeSignature`
when known, and the zero-based argument as `activeParameter`. Each parameter
range in the signature label must be supplied so editors highlight it reliably.

If the current call is syntactically incomplete, the server may combine the
lexical call boundary with the latest visible overload set. It must not claim a
selected overload that the compiler did not resolve.

### Document symbols

`textDocument/documentSymbol` returns hierarchical `DocumentSymbol` values for
declarations physically present in the requested URI. It does not mix symbols
from included files into the including document.

Module variables, constants, types, functions, structs, unions, objects, enum
items, fields, methods, and properties are included. Members are children of
their owning type. `range` is the full declaration range and `selectionRange`
is the name token. Recovered declarations without a known closing token end at
the last token known to belong to the declaration.

For an unowned `.dqi`, the result is empty until the dependency graph provides
an owning module context.

## Scheduling, concurrency, and stale work

The protocol reader must remain responsive while compiler processes run. Use
one I/O event loop and one job scheduler that runs at most one root worker at a
time. Nested module compiler calls are made and serialized by that worker's
normal module loader. Limiting root workers makes temporary artifact mutation
deterministic and avoids oversubscribing large projects. Protocol responses and
notifications are serialized through one writer.

Every document and dependency has a monotonically increasing server revision.
An analysis job captures its root generation and all source revisions. A newer
change cancels queued older jobs; a running old job may finish, but its result
is discarded atomically. Publishing a snapshot and updating dependency edges
is one event-loop operation so queries never observe half of a new graph.

Interactive requests have priority over debounced background diagnostics. Two
requests for the same root revision share one analysis job. Cancellation of one
waiting request does not cancel work still needed by another request or by
diagnostics.

Each root worker and all descendants must be placed in one killable process
group on POSIX and one kill-on-close Job Object on Windows. Cancelling obsolete
work first requests graceful termination, then forcefully terminates the whole
tree after a short grace interval. `OProcessRunner` must expose this operation;
killing only the direct worker can leave a module compiler running and holding
temporary artifact locks.

Only the server-created root starts the POSIX process group. Nested
`OProcessRunner` instances inherit it and must not create independent groups.
On Windows, worker descendants must not request breakaway from the server's Job
Object.

The configured worker timeout applies to the whole root process tree. Timeout,
spawn failure, malformed output, an unexpected compiler exception, or a fatal
worker signal becomes a logged internal error and an `AnalysisWorkerFailed`
diagnostic. It clears the affected root's stale semantic snapshot but does not
terminate the language server. Captured standard output and standard error are
each limited to 64 MiB for a root tree; exceeding either limit terminates the
job so a failing process cannot consume unlimited server memory.

## Proposed source layout

```text
compiler/langserver/
  lang_server.h/.cpp          lifecycle, dispatch, capabilities
  lsp_transport.h/.cpp        Content-Length framing and JSON-RPC messages
  document_store.h/.cpp       overlays, canonical paths, revisions, line maps
  analysis_engine.h/.cpp      scheduling, root graph, temp tree, worker bridge
  semantic_index.h/.cpp       immutable records and feature queries
  lsp_features.h/.cpp         conversion of query results to protocol values
```

The split is descriptive rather than a requirement to create every listed
file. Closely related pieces should be combined when that produces less code.
Protocol-independent position and semantic-index tests should not need to start
an LSP process.

Compiler changes outside this directory are expected for:

- manifest-backed file redirection in the feeder and freshness checks;
- structured diagnostics and text/JSON-Lines sinks;
- `OScRange` and declaration/reference instrumentation;
- worker-mode result serialization and propagation of worker options through
  module child arguments;
- cancellable process-tree handling in `OProcessRunner`; and
- display formatting on symbols and types.

`compiler/CMakeLists.txt` already globs compiler source directories; it must add
the language-server directory and the required unit/integration test target.

## VS Code client requirements

The existing extension in `tools/vscode-dq` remains the client. It must add an
`onLanguage:dq` activation event and use `vscode-languageclient` to start:

```text
<dq.languageServer.path> --langserver [configured arguments] [project file]
```

The default path is `dq-comp`. The extension owns these settings:

- `dq.languageServer.enabled` (default `true`);
- `dq.languageServer.path` (default `dq-comp`);
- `dq.languageServer.arguments` (array of strings, default empty);
- `dq.languageServer.projectFile` (string, default empty); and
- `dq.languageServer.trace` (`off`, `messages`, or `verbose`).

The client selects `.dq`, `.dqh`, and `.dqi` with language id `dq`, passes the
workspace folders, forwards file-watch events, and stops the server during
extension deactivation. It must show a useful error when the executable cannot
be started. The existing run command, grammar, snippets, and problem matcher
remain independent. The packaged VSIX must include `vscode-languageclient` and
its runtime dependencies, either by bundling the extension or by replacing the
current dependency-free packaging command.

## Testing

### Unit tests

Unit tests must cover:

- framing with split headers/payloads, adjacent frames, extra headers, malformed
  lengths, EOF, and the message-size limit;
- JSON-RPC id preservation and error codes;
- URI/path round trips on the host platform;
- LF, CRLF, Unicode BMP, supplementary Unicode, invalid UTF-8, and EOF position
  conversion;
- full-text document versions and rejected stale changes;
- overlay-manifest validation, logical-to-staged reads, and provider freshness
  tokens;
- temporary-instance ownership checks and safe cleanup target validation;
- worker argument construction and propagation to nested module compilers;
- diagnostic deduplication and per-root contribution removal;
- semantic occurrence selection at range boundaries; and
- completion ordering and limits.

### Black-box protocol tests

A test harness starts the built `dq-comp --langserver`, exchanges framed
messages, and asserts exact protocol behavior. At minimum it verifies:

1. initialize, shutdown, and exit;
2. no non-protocol bytes are written to standard output;
3. an unsaved syntax/type error appears and is cleared with the matching
   document version;
4. an unsaved `.dqi` or `.dqh` edit changes diagnostics in its owning `.dq`;
5. command-line and project defines change conditional compilation;
6. hover and definition work for local, member, imported, and overloaded
   symbols;
7. completion observes lexical scope and member visibility;
8. signature help selects the correct nested call argument;
9. document symbols use full and selection ranges in the correct URI;
10. a superseded slow analysis cannot publish stale diagnostics;
11. malformed requests return errors and the next valid request succeeds; and
12. module compilation starts nested compiler workers when required, while all
    artifacts remain below the verified temporary instance directory and no
    linker is started.

The overlay tests must include a relative include, a same-basename header, and
an unsaved imported module. Diagnostics and navigation must report the original
logical URIs, never paths below `dq-lsp-*`.

Tests should use temporary workspaces and compare normalized protocol objects,
not timing-sensitive raw byte streams. The server test suite is run by
`make test` in addition to the existing compiler autotests.

### Resource regression tests

One test changes and reanalyzes a representative module at least 100 times,
then asserts that server memory, completed job metadata, staged contents, and
cached snapshots return to a bounded steady state. It also verifies that
temporary module interfaces are reused when their source fingerprints do not
change and that cancelled jobs leave no descendant processes or held locks.

## Implementation order and completion criteria

The work should be delivered in these independently testable slices:

1. **Transport and lifecycle**: `--langserver`, clean standard output,
   JSON-RPC framing, initialize/shutdown/exit, and black-box harness.
2. **Temporary workers and diagnostics**: private directory creation/cleanup,
   staged overlays, worker launch, full sync, position mapping, JSON-Lines
   diagnostics, process-tree cancellation, and stale-result suppression.
3. **Dependency correctness**: includes, headers, project context, reverse
   invalidation, nested interface compiler calls, temporary artifact freshness,
   and caching.
4. **Semantic index and navigation**: ranges, declarations, occurrences,
   document symbols, hover, and definition.
5. **Editing assistance**: scope snapshots, completion, signature help, and
   incomplete-source recovery.
6. **VS Code integration and hardening**: client activation, settings, watcher,
   cancellation, malformed input, and resource regression tests.

The first release is complete only when all advertised capabilities satisfy the
black-box tests, unsaved content is authoritative throughout the dependency
graph, analysis writes only approved files below its private temporary tree,
normal `dq-comp` diagnostics remain text-compatible, and `make test` passes.

## Later extensions

References and rename should build on the same stable symbol ids and occurrence
records, adding a workspace-wide index and edit-conflict checks. Semantic
tokens should augment, not replace, the TextMate grammar. Formatting should be
specified separately because DQ currently has no canonical formatter. A future
multi-context server should replace the single immutable compilation context
with one worker queue and temporary artifact tree per project.
