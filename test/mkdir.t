#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/mkdir/00.t,v 1.2 2007/01/25 20:50:02 pjd Exp $

desc="mkdir creates directories"

dir=`dirname $0`
. ${dir}/misc.sh

echo "1..23"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n1} 0755
cdir=`pwd`
cd ${n1}

# POSIX: The file permission bits of the new directory shall be initialized from
# mode. These file permission bits of the mode argument shall be modified by the
# process' file creation mask.
expect 0 mkdir ${n0} 0755
expect dir,0755 lstat ${n0} type,mode
expect 0 rmdir ${n0}
expect 0 mkdir ${n0} 0151
expect dir,0151 lstat ${n0} type,mode
expect 0 rmdir ${n0}
expect 0 -U 077 mkdir ${n0} 0151
expect dir,0100 lstat ${n0} type,mode
expect 0 rmdir ${n0}
expect 0 -U 070 mkdir ${n0} 0345
expect dir,0305 lstat ${n0} type,mode
expect 0 rmdir ${n0}
expect 0 -U 0501 mkdir ${n0} 0345
expect dir,0244 lstat ${n0} type,mode
expect 0 rmdir ${n0}

cd ${cdir}
expect 0 rmdir ${n1}

expect 0 mkdir ${n0} 0755
expect EEXIST mkdir ${n0} 0755
expect 0 rmdir ${n0}

expect 0 create ${n0} 0644
expect EEXIST mkdir ${n0} 0755
expect 0 unlink ${n0}
