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
- `src/main.c`: command-line parsing and command dispatch.
- `src/xipfs_support.c`: workstation flash mapping and common helpers.
- `src/<command>.c`: one source file per command.

## Commands

Run help:

```sh
tools/bin/mkxipfs --help
```

Use an image either explicitly:

```sh
tools/bin/mkxipfs --flash fs.flash ls /
```

or via environment:

```sh
export XIPFS_FILE_IMAGE=fs.flash
tools/bin/mkxipfs ls /
```
