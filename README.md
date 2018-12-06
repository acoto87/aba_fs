# aba_fs

A file system written in C.

This implementation uses [FUSE](https://en.wikipedia.org/wiki/Filesystem_in_Userspace) to implement the file system in user space without the need to modify the kernel of the operating system. It also includes a formatter tha format and initialize the disk with the partition.

It support the following operations:

.getattr    = abafs_getattr,
.readdir    = abafs_readdir,
.open	    = abafs_open,
.read	    = abafs_read,
.mkdir      = abafs_mkdir,
.mknod     	= abafs_mknod,
.rename     = abafs_rename,
.rmdir      = abafs_rmdir,
.truncate   = abafs_truncate,
.unlink     = abafs_unlink,
.write      = abafs_write,
.chmod		= abafs_chmod,
.chown		= abafs_chown,
.release	= abafs_release,
.statfs		= abafs_statfs,
.symlink = abafs_symlink,
.readlink = abafs_readlink,
