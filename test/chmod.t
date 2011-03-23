#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chmod/00.t,v 1.2 2007/01/25 20:48:14 pjd Exp $

desc="chmod changes permission"

dir=`dirname $0`
. ${dir}/misc.sh

echo "1..21"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n2} 0755
cdir=`pwd`
cd ${n2}

#2
expect 0 create ${n0} 0644
expect 0644 stat ${n0} mode
expect 0 chmod ${n0} 0111
expect 0111 stat ${n0} mode
expect 0 unlink ${n0}

#7
expect 0 mkdir ${n0} 0755
expect 0755 stat ${n0} mode
expect 0 chmod ${n0} 0753
expect 0753 stat ${n0} mode
expect 0 rmdir ${n0}

cd ${cdir}
expect 0 rmdir ${n2}

expect 0 mkdir ${n0} 0755
expect 0 create ${n0}/${n1} 0644
expect ENOTDIR chmod ${n0}/${n1}/test 0644
expect 0 unlink ${n0}/${n1}
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect ENOENT chmod ${n0}/${n1}/test 0644
expect ENOENT chmod ${n0}/${n1} 0644
expect 0 rmdir ${n0}
