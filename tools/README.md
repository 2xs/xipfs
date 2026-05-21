# mkxipfs

`mkxipfs` is the xipfs workstation utility used to create, inspect, and modify xipfs flash images (`*.flash`).

## Build

```sh
make -C tools
```

Binary output:

- `tools/bin/mkxipfs`

## Structure

- `include/mkxipfs.h`: shared interfaces for the tool.
- `include/test.h` : shared test framework.
- `src/main.c`: command-line parsing and command dispatch.
- `src/xipfs_support.c`: workstation flash mapping.
- `src/mkxipfs.c` ; workstation common helpers.
- `src/<command>.c`: one source file per command.

## Commands

Run help:

```sh
tools/bin/mkxipfs --help
```

For all `mkxipfs` commands, you need to select a target board that will set related flash memory specifications.  
Available supported targets are listed within `mkxipfs` help.

Select a target board either explictly :

```sh
tools/bin/mkxipfs --target <supported_target_name> ...
```

or via environment :
```sh
export XIPFS_TARGET=dwm1001
tools/bin/mkxipfs ...
```

Use an image either explicitly:

```sh
tools/bin/mkxipfs --target dwm1001 --flash fs.flash ls /
```

or via environment:

```sh
export XIPFS_FILE_IMAGE=fs.flash
tools/bin/mkxipfs --target dwm1001  ls /
```
