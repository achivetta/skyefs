#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chflags/00.t,v 1.1 2007/01/17 01:42:08 pjd Exp $

desc="chflags changes flags"

dir=`dirname $0`
. ${dir}/misc.sh

require chflags

echo "1..173"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n2} 0755
cdir=`pwd`
cd ${n2}

expect 0 create ${n0} 0644
expect none stat ${n0} flags
expect 0 chflags ${n0} UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE,SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK
expect UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE,SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK stat ${n0} flags
expect 0 chflags ${n0} UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE
expect UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE stat ${n0} flags
expect 0 chflags ${n0} SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK
expect SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK stat ${n0} flags
expect 0 chflags ${n0} none
expect none stat ${n0} flags
expect 0 unlink ${n0}

expect 0 mkdir ${n0} 0644
expect none stat ${n0} flags
expect 0 chflags ${n0} UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE,SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK
expect UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE,SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK stat ${n0} flags
expect 0 chflags ${n0} UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE
expect UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE stat ${n0} flags
expect 0 chflags ${n0} SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK
expect SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK stat ${n0} flags
expect 0 chflags ${n0} none
expect none stat ${n0} flags
expect 0 rmdir ${n0}

expect 0 mkfifo ${n0} 0644
expect none stat ${n0} flags
expect 0 chflags ${n0} UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE,SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK
expect UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE,SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK stat ${n0} flags
expect 0 chflags ${n0} UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE
expect UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE stat ${n0} flags
expect 0 chflags ${n0} SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK
expect SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK stat ${n0} flags
expect 0 chflags ${n0} none
expect none stat ${n0} flags
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 symlink ${n0} ${n1}
expect none stat ${n1} flags
expect none lstat ${n1} flags
expect 0 lchflags ${n1} UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE,SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK
expect UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE,SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK lstat ${n1} flags
expect none stat ${n1} flags
expect 0 lchflags ${n1} UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE
expect UF_NODUMP,UF_IMMUTABLE,UF_APPEND,UF_NOUNLINK,UF_OPAQUE lstat ${n1} flags
expect none stat ${n1} flags
expect 0 lchflags ${n1} SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK
expect SF_ARCHIVED,SF_IMMUTABLE,SF_APPEND,SF_NOUNLINK lstat ${n1} flags
expect none stat ${n1} flags
expect 0 lchflags ${n1} none
expect none lstat ${n1} flags
expect none stat ${n1} flags
expect 0 unlink ${n1}
expect 0 unlink ${n0}

cd ${cdir}
expect 0 rmdir ${n2}
