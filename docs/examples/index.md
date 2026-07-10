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
| `examples/string/cstrings.dq` | fixed C strings |
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

Run an example with:

```bash
dq-run examples/string/txtfmt.dq
```

Some examples, especially networking examples, may need a free local port and a
host platform with the required socket APIs.

