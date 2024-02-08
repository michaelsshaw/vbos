# vbos - very bad os - now with executables!

```
"Don't reinvent the wheel" - Some idiot, probably
```

For-fun operating system created by me!

## Installation

You will need to deploy limine onto a filesystem image, the limine barebones
project will suffice for creating this. In addition, you must also have an
ext2 filesystem image.

NOTE: For versions of limine >=5.x, multiple images must be used. Limine will
no longer accept ext2 filesystems for booting kernel images, and this OS as of
yet only supports ext2.

Be sure to set `SERIAL=yes` at the top of your limine.cfg as well as the
`KERNEL_CMDLINE=` parameter `root=` as your root filesystem (not necessarily
the boot filesystem). Booting without the `root=` parameter will cause a panic
giving you a list of available block devices to mount. Only ext2 is supported
for mounting filesystems.

If you wish to use a partition scheme, it must be GPT. If not, mounting the bare
block device should (probably) work fine.

## License

This project is licensed under GPL-2.0-only, a copy of which can be found in the
COPYING file. All source files, except for `limine.h` are released under this
license. 

Copyright 2023 Michael Shaw. 
