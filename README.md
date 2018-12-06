# aba_fs

A file system written in C.
_This was an undergraduated college project and the implementation was in collaboration with Betty Lezcano and Alejandro Díaz._

This implementation uses [FUSE](https://en.wikipedia.org/wiki/Filesystem_in_Userspace) to implement a custom file system in user space without the need to modify the kernel of the operating system. It also includes a formatter that format and initialize the disk.

It support the following operations:

* getattr
* readdir
* open
* read
* mkdir
* mknod
* rename
* rmdir
* truncate
* unlink
* write
* chmod
* chown
* release
* statfs
* symlink
* readlink
