# `nanonet`

`nanonet` is a small event-driven networking package. It currently targets the
hosted socket runtime used by the project examples.

```dq
use nanonet/nano_sockets
use nanonet/nano_http
```

## Sockets

`nano_sockets` provides non-blocking TCP/UDP sockets, an event watcher, and a
server/connection framework.

| Type | Purpose |
| --- | --- |
| `ONanoSocket` | low-level socket wrapper |
| `OSocketWatcher` | event loop around platform socket events |
| `OSConnection` | base server-side connection object |
| `ONanoServer` | listener and connection manager |
| `ENanoNet` | socket/server exception |

Server connections are object types derived from `OSConnection`. Override
`HandleInput` and optionally `HandleOutput`.

```dq
object OMyConnection(OSConnection):
    function HandleInput(aobj : Object) [[override]]
endobj

function OMyConnection.HandleInput(aobj : Object) [[override]]:
    var buf : [256]byte
    var n : int = sock.Recv(&buf[0], SizeOf(buf))
    if n <= 0:
        Close()
    endif
endfunc
```

Create a server with the connection type and listen port:

```dq
var svr <- ONanoServer(OMyConnection, 8080)
svr.InitListener()
while true:
    svr.WaitForEvents(1000)
endwhile
```

## HTTP

`nano_http` builds an HTTP request/response layer on top of `nano_sockets`.

| Type | Purpose |
| --- | --- |
| `OStrMap` | simple string map used for headers, query values, cookies |
| `OSConnectionHttp` | HTTP connection base class |
| `ONanoHttpServer` | HTTP server with content-type map |

Override `ProcessRequest()` to handle dynamic responses:

```dq
object OConn(OSConnectionHttp):
    function ProcessRequest() -> bool [[override]]
endobj

function OConn.ProcessRequest() -> bool [[override]]:
    response_headers["Content-Type"] = "text/plain"
    response = "hello"
    return true
endfunc
```

Important request fields on `OSConnectionHttp` include:

| Field | Meaning |
| --- | --- |
| `method` | request method, uppercase |
| `uri` | path without query string |
| `url` | original request URL |
| `getstr` | raw query string |
| `http_ver` | HTTP version text |
| `keep_alive` | connection reuse flag |
| `ucheaders` | request headers with uppercase keys |
| `qsvars` | parsed query string values |
| `cookies` | parsed cookie values |

Response fields:

| Field | Meaning |
| --- | --- |
| `response_code` | status code, default `200` |
| `response_headers` | output headers |
| `response` | in-memory response body |
| `full_content_length` | length for streamed file responses |

## Static Files

`HandleStaticFiles(root)` serves files below a directory and rejects unsafe `..`
paths.

```dq
function OConn.ProcessRequest() -> bool [[override]]:
    if HandleStaticFiles("public"):
        return true
    endif
    response_code = 404
    response = "not found"
    return true
endfunc
```

The HTTP server maps common extensions such as `.html`, `.css`, `.js`, `.png`,
`.jpg`, `.svg`, and `.ico` to content types. Unknown extensions use
`application/octet-stream`.

See the examples in `examples/nanonet/`, especially
`http_static_files.dq`.

