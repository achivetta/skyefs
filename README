SkyeFS
=======

SkyeFS is a FUSE filesystem providing distributed directories using Giga+ and
PVFS.


What's Giga+?
-------------

There are a huge variety of distributed filesystems available and a great deal
of research has gone into the problem of distributing filesystem data over a
cluster of machines.  However, many of these filesystems are unable to scale
metadata.  Some systems, such as HDFS, store all directory entries on a single
machine while others, like PVFS, store all entries in a particular directory on
a single machine.  Both of these techniques scale poorly for directories with
many entries or highly concurrent write workloads.  

[Giga+][1] is a technique for distributing the directory entries in a directory
between a set of servers.  When a directory is initially created, it is entirely
contained in one server.  As the directory grows, it splits and the hash space
of file names is distributed to more servers until the load is balanced between
the available servers.

[1]: http://static.usenix.org/events/fast11/tech/full_papers/PatilNew.pdf "Scale and Concurrency of GIGA+: File System Directories with Millions of Files"

Where do PVFS and FUSE enter the mix?
-------------------------------------

SkyeFS uses the [Parallel Virtual File System][2] (PVFS) as the underlying
distributed filesystem on top of which we layer the Giga+ scheme.  We use
multiple PVFS directories to represent each logical SkyeFS directory and a
client and server pair to manage the partitioning.  The [Filesystem in
Userspace][3] tool provides us with an easy way to implement a VFS compatible
filesystem without writing kernel code.

[2]: http://www.pvfs.org "Parallel Virtual File System, Version 2"
[3]: http://fuse.sourceforge.net "FUSE: Filesystem in Userspace"


So, what do I get out of this?
------------------------------

Using SkyeFS on top of PVFS allows your directories to scale with the number of
available servers.  If you need to store millions of files in a single directory
or are doing highly concurrent inserts and deletes in a large directory, you
may see improved performance and server utilization.

The SkyeFS client and server are lightweight wrappers around PVFS operations
and, other than the FUSE overhead, add very little additional overhead to PVFS
operations.  SkyeFS was designed to ensure that server failures will never leave
the filesystem in an inconsistent state, maintaining the failure semantics of
PVFS.

Is the code ready now?
----------------------

Yes! But, depending on your use case, some more work may be required before
you'll be happy with it.  Currently, there are same areas with unfinished work.
This includes some of the failure recovery code (e.g. resuming partition splits)
and some performance sensitive code (e.g. flow control in the server).  These
areas are indicated with TODO or FIXME comments in the code.

How do I get started?
---------------------

First, setup an empty PVFS filesystem.  Then, launch an instance of the
`skye_server` on each PVFS MDS.  You can now use `skye_client` to mount the
filesystem.  Run these commands without arguments for their usage.  Or see the
scripts in `util/` for examples.
