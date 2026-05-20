# Getting Started with XiPFS and FAE format.

This document aims at providing valuable intel to produce and embed XiPFS mount points filled with regular files, but also with FAE executable files.

## XiPFS.
For the record, before going any further, let's state a few things about XiPFS.

XiPFS stands for e**X**ecute **i**n **P**lace **F**ile**S**ystem.  
It can host regular files, but also executable ones.

XiPFS is **dedicated to flash memory**, and its **granularity** is the board flash page.  
All files will consume an integer number of flash pages, related to their actual bytesize and the board flash page one.  
Directories are special entities in that regard, and only use 1 flash memory page when they are empty.  
Otherwise, XiPFS relies on the contents path to identify a directory, and then require no flash memory overhead in those cases.

Executable files are tagged as such at their creation into the filesystem,  according to the filename extension *(\*.fae)*.  
They are expected to be stored in the **FAE file format**, which is dedicated to the deployment of post-issuance software in IoT devices.

When these files are processed, **only** `.data`, `.bss` **segments and relocation data  will be stored in RAM.**  
The **actual code** (`.text` segment) will be left **untouched in flash memory**, and will be **executed from it.**  

An important feature of XiPFS is to **offer to run files with memory protection enabled**, by making use of boards' **MPU hardware**.  
By defining MPU regions with appropriate read, write and execute permissions at runtime, the filesystem ensures that FAE files will only have access to their legitimate areas in both RAM and flash memory.  
All other attempts will trigger an **hardware memory violation exception** and will **stop execution immediately**.

At last, XiPFS has been integrated in [RIOT OS](https://github.com/riot-os/RIOT), and should be compatible with all boards featuring **addressable** **N**on **V**olatile **M**emory.  
It should also be compatible with all **32 bits ARM boards**, providing an **ARMv7-M MPU**.

**However, only the DWM1001 board has been tested and is confirmed to function correctly.**

XiPFS by itself can be found and cloned from this [Github repository](https://github.com/2xs/xipfs/tree/main).

## FAE files.
ELF file format contain many information, but only a subset is actually needed to perform runtime software relocation.  
Moreover, its complex structure makes it error-prone to parse.

To address these points, FAE format has been designed as an *adhoc* binary file format, for the deployment of **post-issuance software in constrained environments**.  

**From ELF files**, a tool **extracts only needed information** for relocation and runtime execution, such as relocation tables, `.text`, `.got`, `.data` sections etc...  
A minimal **startup sequence** (*CRT0) is added as a **header**, which will perform relocation and will prepare software runtime environment.

At last, all these components are concatenated into a single **binary output file**, whose size is approximately 20% of the original ELF one.


To produce FAE files, please git clone the [2XS' FAE format repository](https://github.com/2xs/fae_format/tree/master).

More information on pre-requisites, required GCC compiler flags and FAE format details can be found in [FAE format's GETTING_STARTED.md document](https://github.com/2xs/fae_format/blob/master/GETTING_STARTED.md).

## Preparing an XiPFS mount point offboard.
Once FAE files have been produced, it is possible to create a memory image of an XiPFS mount point off-board and to populate this latter with files.

First things first, git clone the main branch of [XiPFS repository](https://github.com/2xs/xipfs/tree/main).  
Then, enter the following lines to make `mkxipfs` tool :  
```
$make -C tools
```

It should produce a `mkxipfs` file located in `tools/bin` directory.  
```
$ls tools/bin  
mkxipfs
```

Please, read the accompanying [README.md](https://github.com/2xs/xipfs/blob/main/tools/README.md) for a brief introduction to `mkxipfs`.

### Building your first XiPFS mount point memory image offboard.
There are two ways to make XIPFS mount point memory images.

Either, **you can create an empty mount point memory image** whose bytesize is known in advance.  
For example, to obtain a 32 KiB empty memory image named `mount_point.flash`, you can enter :  
>```$tools/bin/mkxipfs --target dwm1001 create mount_point.flash 32768```  
OR  
```$tools/bin/mkxipfs --target dwm1001 create mount_point.flash 32K```

Or, **you can build a mount point memory image** from the contents of a directory in the host filesystem.  
>```$tools/bin/mkxipfs --target dwm1001 build mount_point.flash ~/directory/to/copy```  
OR  
```$tools/bin/mkxipfs --target dwm1001 build mount_point.flash ~/directory/to/copy 32K```

Please note that with the first variant, the memory footprint size will be deduced from the contents of the directory.

In the second variant, when the provided bytesize is less than the one computed from the directory contents, `mkxipfs` will return an error.

On success, for both variants, `mkxipfs` will reproduce the target directory tree from the host filesystem into an XiPFS one stored in `mount_point.flash` file.

### Managing XiPFS memory images offboard.
After this step, you can populate the flash file and/or manage its content thanks to other `mkxipfs` options.

**To list memory image contents**, use `ls` or `tree` commands :  
>```$tools/bin/mkxipfs --target dwm1001 --flash mount_point.flash ls [-l] [path]```  
OR  
```$tools/bin/mkxipfs --target dwm1001 --flash mount_point.flash tree [path]```
  

**To add files from host to the memory image**, use `put` command, such as in :  
```
$tools/bin/mkxipfs --target dwm1001 --flash mount_point.flash put <host_file> <xipfs_file>
$tools/bin/mkxipfs --target dwm1001 --flash mount_point.flash put <xipfs_file>
```

The second variant will read `stdin` to fill the xipfs file with data.  

**To retrieve files from memory image to the host filesystem**, use `get` command :  
```
$tools/bin/mkxipfs --target dwm1001 --flash mount_point.flash get <host_file> <xipfs_file>
$tools/bin/mkxipfs --target dwm1001 --flash mount_point.flash get <xipfs_file>
```
The second variant will output <xipfs_file> data to `stdout`.

**To remove files from a memory image**, use `rm` command :
```
$tools/bin/mkxipfs --target dwm1001 --flash mount_point.flash rm <xipfs_file>
```

**To create directories into a memory image**, use `mkdir` command :
```
$tools/bin/mkxipfs --target dwm1001 --flash mount_point.flash mkdir <xipfs_dir>
```

**To remove empty directories from a memory image**, use `rmdir` command :
```
$tools/bin/mkxipfs --target dwm1001 --flash mount_point.flash rmdir <xipfs_dir>
```

**To move/rename files and directories**, use `mv` command :
```
$tools/bin/mkxipfs --target dwm1001 --flash mount_point.flash mv <xipfs_src> <xipfs_dst>
```

Please do notice that **removing files or directories does not lead to an automatic flash file resize**.  
After initial creation/setting, flash memory file size will never change no matter what operation is performed on it.  
For now, there is no `mkxipfs` option for resizing an existing flash file.

For a complete list of `mkxipfs` cli options, enter the following line :  
```$tools/bin/mkxipfs```

### Step by step build example.

Given the following directory in the host filesystem.
```
$ tree /tmp/test_123456
/tmp/test_123456/
/tmp/test_123456/
├── A
│   ├── main.fae
│   └── pi.fae
├── B
└── regular_file.txt
$
$ stat -c %s /tmp/test_123456/regular_file.txt 
12
$ stat -c %s /tmp/test_123456/A/main.fae 
1248
$ stat -c %s /tmp/test_123456/A/pi.fae 
1280
```

Let's **build** an XiPFS memory image from it, for the DWM1001 board :  
```
$ tools/bin/mkxipfs --target dwm1001 build /tmp/mount_point_from_build.flash /tmp/test_123456/
Current NVM configuration :
        - Pages count : 128
        - Page size   : 4096
        - Erase state value   : ff
        - Write block alignment  : 4
        - Write block size  : 4
Created '/tmp/mount_point_from_build.flash' with 16384 bytes (4 pages of 4096 bytes).
To reuse this image in following commands:
  export XIPFS_FILE_IMAGE='/tmp/mount_point_from_build.flash'
Build complete: /tmp/mount_point_from_build.flash from /tmp/test_123456/ (4 pages, 16384 bytes).
```

The `mkxipfs` output is quite verbose, and displays the DWM1001 flash memory properties; such as the **page bytesize** set at **4096 bytes**.
 
Given that :  
- `B` folder is empty, then the filesystem should use 1 flash page,
- `regular_file.txt` size is 12 bytes, then the filesystem should use 1 flash page,
- `main.fae` size is 1248 bytes, then the filesystem should use 1 flash page,
- `pi.fae` size is 1280 bytes, then the filesystem should use 1 flash page.

From these remarks, we expect that XiPFS will use **4 pages** of the DWM1001 flash memory, each page being 4096 bytes long.  
We can observe this is consistent with the build summary reporting that **4 pages** of flash memory will be used, for a total of **16 KiB**.

To ensure that an XiPFS memory image build will not exceed an expected bytesize, please use the size variant of the `build` command.  
Here, **to constraint the generated memory image file size** to 16 KiB, we could have entered  :  
```
$ tools/bin/mkxipfs --target dwm1001 build /tmp/mount_point_from_build.flash /tmp/test_123456/ 16K
```


Now, let's check the **structure of the generated mount point** *(NVM configuration display has been omitted)*:  
```
$ tools/bin/mkxipfs --target dwm1001 --flash /tmp/mount_point_from_build.flash tree
/
┣━ A
┃  ┣━ main.fae
┃  ┗━ pi.fae
┣━ B
┗━ regular_file.txt

4 entries
```

The hierarchical display shows the same organization than the original directory located in `/tmp/test_123456` host filesystem.

### Step by step creation example.

Another way to create XiPFS memory image is to create an empty flash file, and to populate it afterwards.  
First, let's **create** the memory image and reserve a 16 KiB space as seen previously thanks to the following line :  
```
$ tools/bin/mkxipfs --target dwm1001 create /tmp/mount_point_from_create.flash 16K
Current NVM configuration :
        - Pages count : 128
        - Page size   : 4096
        - Erase state value   : ff
        - Write block alignment  : 4
        - Write block size  : 4
Created '/tmp/mount_point_from_create.flash' with 16384 bytes (4 pages of 4096 bytes).
To reuse this image in following commands:
  export XIPFS_FILE_IMAGE='/tmp/mount_point_from_create.flash'
```

> From now on, the current NVM configuration display will be omitted for readibility.

At this step, `/tmp/mount_point_from_create.flash` is empty. Let's **create our directories** :  
```
$ tools/bin/mkxipfs --target dwm1001 --flash /tmp/mount_point_from_create.flash mkdir /A
$ tools/bin/mkxipfs --target dwm1001 --flash /tmp/mount_point_from_create.flash mkdir /B
```

Now let's **add our files** to their respective location into the memory file image :  
```
$ tools/bin/mkxipfs --target dwm1001 --flash /tmp/mount_point_from_create.flash put /tmp/test_123456/A/main.fae /A/main.fae
$ tools/bin/mkxipfs --target dwm1001 --flash /tmp/mount_point_from_create.flash put /tmp/test_123456/A/pi.fae /A/pi.fae
$ tools/bin/mkxipfs --target dwm1001 --flash /tmp/mount_point_from_create.flash put /tmp/test_123456/regular_file.txt /regular_file.txt
```

At last, let's check the **structure of this mount point** :  
```
$ tools/bin/mkxipfs --target dwm1001 --flash /tmp/mount_point_from_create.flash tree
/
┣━ A
┃  ┣━ main.fae
┃  ┗━ pi.fae
┣━ B
┗━ regular_file.txt

4 entries
```

Using the `build` or `create` command depends on a chosen workflow :
- `build` command is a no brainer and is handy if you want to embed a replica of an existing directory into a memory image.
- `create` command might be preferred if you want to create a mount point out of the blue.

Please remember that in all cases, once the memory image file has been defined, you can manage its contents with the other options of `mkxipfs`.
