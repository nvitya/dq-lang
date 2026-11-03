# Additional Standard Packages

The following modules are also shipped in `stdpkg`. They have focused source
interfaces and examples rather than separate API manuals. Check each module's
public declarations for exact signatures and supported platforms.

| Import | Purpose | Source or example |
| --- | --- | --- |
| `math` | Mathematical functions backed by the native math library. | [`math.dq`](https://github.com/nvitya/dq-lang/blob/main/stdpkg/math/math.dq) |
| `time` | Time values and time-related helpers. | [`time.dq`](https://github.com/nvitya/dq-lang/blob/main/stdpkg/time/time.dq) |
| `strparse` | Cursor-based text parsing. | [`test_strparse.dq`](https://github.com/nvitya/dq-lang/blob/main/stdpkg/strparse/examples/test_strparse.dq) |
| `collections/strstrmap` | String-to-string map. | [`strstrmap.dq`](https://github.com/nvitya/dq-lang/blob/main/stdpkg/collections/strstrmap.dq) |
| `shell` | Hosted shell/process helpers. | [`shell examples`](https://github.com/nvitya/dq-lang/tree/main/stdpkg/shell/examples) |
| `db` | Shared database abstractions. | [`db.dq`](https://github.com/nvitya/dq-lang/blob/main/stdpkg/db/db.dq) |
| `sqlite3` | SQLite binding using the SQLite library. | [`sqlite_test.dq`](https://github.com/nvitya/dq-lang/blob/main/stdpkg/sqlite3/examples/sqlite_test.dq) |
| `mysql` | MySQL client binding. | [`mysql_test.dq`](https://github.com/nvitya/dq-lang/blob/main/stdpkg/mysql/examples/mysql_test.dq) |
| `socket` | Low-level socket declarations. | [`socket.dq`](https://github.com/nvitya/dq-lang/blob/main/stdpkg/socket/socket.dq) |
| `libc/stdio` | C standard I/O declarations and stream helpers. | [`stdio.dq`](https://github.com/nvitya/dq-lang/blob/main/stdpkg/libc/stdio.dq) |

## Native Dependencies

A DQ import supplies declarations and code; it does not install a native library.
Database applications need the matching client development libraries at link
time and the corresponding shared libraries at runtime. For cross targets, the
libraries must match the target ABI and be available in the selected sysroot or
linker search paths.

Use [C Interoperability](../020-language/200-c-interoperability.md) when adding
bindings, and [Project Files](../030-compiler/050-project-files.md) for explicit
linker inputs. For networking applications, start with the higher-level
[`nanonet`](070-nanonet.md) package.
