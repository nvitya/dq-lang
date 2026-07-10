# `file`

The `file` module provides simple file, directory, and path helpers.

```dq
use file

FileWriteText("hello.txt", "hello\n")
var text : str = FileReadText("hello.txt")
```

Errors raise `EFileError`.

## `OFile`

`OFile` wraps a libc file handle and closes it from its destructor.

```dq
var f <- OFile()
f.Open("data.bin", "rb")
var bytes : [*]byte = f.ReadBytes()
f.Close()
```

| Method / property | Meaning |
| --- | --- |
| `Open(name, mode)` | open with a libc-style mode such as `"r"`, `"rb"`, `"w"` |
| `Close()` | close if open |
| `Opened()` / `.opened` | true when a handle is open |
| `Size()` / `.size` | file size, preserving current position |
| `Seek(pos)` / `.position = pos` | seek from file start |
| `CurPos()` / `.position` | current offset |
| `Read(ptr, size)` | read bytes into memory |
| `ReadText()` | read the whole file as `str` |
| `ReadBytes()` | read the whole file as `[*]byte` |
| `Write(ptr, size)` | write bytes from memory |
| `WriteText(text)` | write text |
| `WriteBytes(bytes)` | write a byte slice |

## Convenience Functions

```dq
FileRead(name, ptr, maxlen)
FileReadText(name)
FileReadBytes(name)

FileWrite(name, ptr, len)
FileWriteText(name, text)
FileWriteBytes(name, bytes)

FileExists(name)
```

`FileExists` returns false for directories.

## Directories

```dq
DirExists("out")
DirCreate("out/nested", true)
DirRemove("out/nested")
```

`DirCreate(path, true)` creates missing parent directories. Without `true`, an
existing directory is an error.

## Path Helpers

```dq
PathTranslateSeparators(path)
PathDirName(path)
PathFileName(path)
PathFileExt(path)
```

Separators are normalized to `/`.

