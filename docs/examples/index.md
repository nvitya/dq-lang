# Examples

The repository contains small focused examples and larger demonstrations.

## Language And Basics

| Example | Shows |
| --- | --- |
| `examples/langdemo.dq` | broad language demo |
| `examples/langdemo_braces.dq` | brace-style syntax variant |
| `examples/basic/new_delete.dq` | manual allocation helpers |
| `examples/objects/dq_objects.dq` | objects, constructors, methods |
| `examples/objects/inheritance.dq` | object inheritance |

## Strings And Formatting

| Example | Shows |
| --- | --- |
| `examples/string/embstrs.dq` | Embedded Strings |
| `examples/string/txtfmt.dq` | `Format`, `AddFmt`, stdout formatting |

## Files

| Example | Shows |
| --- | --- |
| `examples/file/fileiotest.dq` | file IO, directory helpers |

## Networking

| Example | Shows |
| --- | --- |
| `examples/nanonet/simplebin_server.dq` | simple socket server |
| `examples/nanonet/http_static_files.dq` | HTTP server, query parsing, static files |

## Threads

See the [`threads` package documentation](../packages/threads/index.md)
for the APIs and example commands.

| Example | Shows |
| --- | --- |
| `stdpkg/threads/examples/thread_test.dq` | starting threads and polling completion |
| `stdpkg/threads/examples/wait_test.dq` | message-channel waits and timeout checks |
| `stdpkg/threads/examples/stress_test.dq` | channel correctness with multiple producers and consumers |
| `stdpkg/threads/examples/work_distribution.dq` | distributing CPU work through a bounded channel |

Run an example with:

```bash
dq-run examples/string/txtfmt.dq
```

Some examples, especially networking examples, may need a free local port and a
host platform with the required socket APIs.
