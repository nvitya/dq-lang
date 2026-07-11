# Packaging for Linux

Compact release package, without llvm and tools:
```
make -j"$(nproc)" package-linux-release
```

Full release package, with llvm and tools:
```
make -j"$(nproc)" package-linux-full-release
```

# Packaging for Windows

Full package (with llvm and tools):
```
make -j"$(nproc)" package-windows-release
```
