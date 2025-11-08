This  is currently has a parabyte licence, that means you need to contact me about specifics,

The original author of this has blessed me to do what ever i wish with his project.

## Building with Open Watcom (MS-DOS target)

The original tree was meant for Borland C under DOS. To build the MS-DOS
executables from a Debian host, install Open Watcom V2 (or newer), source
its `owsetenv.sh`, and make sure the complete EZNOS source archive is
present. In particular, `global.h` and the translation units listed in
`ALTERED/MAKEFILE/MAKEFILE` must exist; otherwise the Watcom script will
print the files that still need to be copied over.

```
source /opt/watcom/owsetenv.sh    # path depends on your installation
scripts/build_watcom.sh           # builds build/watcom/bin/eznos.exe
scripts/build_watcom.sh --clean   # removes build/watcom/*
```

The helper reads the `mkdep` section of `ALTERED/MAKEFILE/MAKEFILE` to
work out which modules need to be compiled, generates the missing
`hardware.h` shim from `pc.h`, and drives `wcc`, `wasm`, and `wcl` to
emit a 16-bit large-model MS-DOS executable. You can override tool
paths and extra compiler/assembler flags via the `WATCOM_*` environment
variables described at the top of `scripts/build_watcom.sh`.
