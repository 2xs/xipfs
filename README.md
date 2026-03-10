# xipfs

`xipfs` (eXecute-In-Place File System) is a small flash-oriented file system for embedded targets.
Its main goal is to store binaries in non-volatile memory and execute them in place, without copying code to RAM first.

## Highlights

- Execute code directly from flash (XIP workflow).
- Compact on-media layout based on flash-page allocation.
- Deterministic storage behavior (no journaling layer).
- Workstation build target to generate and manipulate `.flash` images offline.

## Repository Layout

- `include/` and `src/`: core xipfs library.
- `boards/`: board-specific configuration.
- `workstation/`: host build target used for image tooling and tests.
- `tools/`: `mkxipfs` command-line utility.

## mkxipfs Tool

`mkxipfs` is a host-side tool that operates on a flash image file and reuses the same xipfs library logic.

Current commands include:

- `create <file.flash> <size>`
- `build <file.flash> <host_dir> [size]`
- `ls [-l] [path]`
- `tree [path]`
- `put <host_file> <xipfs_file>` or `put <xipfs_file>` (stdin)
- `get <xipfs_file> <host_file>` or `get <xipfs_file>` (stdout)
- `mkdir <xipfs_dir>`
- `rm <xipfs_file>`
- `rmdir <xipfs_dir>`
- `mv <src> <dst>`
- `test`

For commands working on an existing image, select it with:

- `--flash <file.flash>`
- or environment variable `XIPFS_FILE_IMAGE`

## Build

Library (workstation target):

```sh
make BOARD=workstation
```

Tool:

```sh
make -C tools
```

## Notes and Constraints

- On-media offsets are 32-bit (`xipfs_off_t`), so generated images keep embedded-target compatibility.
- File-system behavior is designed for flash memory constraints, not for desktop POSIX semantics.
- This project is suitable for controlled embedded deployments where simplicity and predictability matter.
