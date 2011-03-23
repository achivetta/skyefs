#!/bin/bash
# $FreeBSD: src/tools/regression/fstest/tests/chown/00.t,v 1.1 2007/01/17 01:42:08 pjd Exp $

desc="chown changes ownership"

dir=`dirname $0`
. ${dir}/misc.sh

echo "1..14"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n2} 0755
cdir=`pwd`
cd ${n2}

# 2
expect 0 create ${n0} 0644
expect 0 chown ${n0} 123 456
expect 123,456 lstat ${n0} uid,gid
expect 0 chown ${n0} 0 0
expect 0,0 lstat ${n0} uid,gid
expect 0 unlink ${n0}
# 8
expect 0 mkdir ${n0} 0755
expect 0 chown ${n0} 123 456
expect 123,456 lstat ${n0} uid,gid
expect 0 chown ${n0} 0 0
expect 0,0 lstat ${n0} uid,gid
expect 0 rmdir ${n0}

cd ${cdir}
expect 0 rmdir ${n2}
