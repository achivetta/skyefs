#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/open/00.t,v 1.2 2007/01/25 20:50:02 pjd Exp $

desc="open opens (and eventually creates) a file"

dir=`dirname $0`
. ${dir}/misc.sh

echo "1..24"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n1} 0755
cdir=`pwd`
cd ${n1}

# POSIX: (If O_CREAT is specified and the file doesn't exist) [...] the access
# permission bits of the file mode shall be set to the value of the third
# argument taken as type mode_t modified as follows: a bitwise AND is performed
# on the file-mode bits and the corresponding bits in the complement of the
# process' file mode creation mask. Thus, all bits in the file mode whose
# corresponding bit in the file mode creation mask is set are cleared.
expect 0 open ${n0} O_CREAT,O_WRONLY 0755
expect regular,0755 lstat ${n0} type,mode
expect 0 unlink ${n0}
expect 0 open ${n0} O_CREAT,O_WRONLY 0151
expect regular,0151 lstat ${n0} type,mode
expect 0 unlink ${n0}
expect 0 -U 077 open ${n0} O_CREAT,O_WRONLY 0151
expect regular,0100 lstat ${n0} type,mode
expect 0 unlink ${n0}
expect 0 -U 070 open ${n0} O_CREAT,O_WRONLY 0345
expect regular,0305 lstat ${n0} type,mode
expect 0 unlink ${n0}
expect 0 -U 0501 open ${n0} O_CREAT,O_WRONLY 0345
expect regular,0244 lstat ${n0} type,mode
expect 0 unlink ${n0}

cd ${cdir}
expect 0 rmdir ${n1}

expect 0 mkdir ${n0} 0755
expect ENOENT open ${n0}/${n1}/test O_CREAT 0644
expect ENOENT open ${n0}/${n1} O_RDONLY
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
cdir=`pwd`
cd ${n0}
expect EACCES -u 65534 -g 65534 open ${n1} O_RDONLY,O_CREAT 0644
cd ${cdir}
expect 0 rmdir ${n0}
