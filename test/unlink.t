#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/unlink/00.t,v 1.1 2007/01/17 01:42:12 pjd Exp $

desc="unlink removes regular files, symbolic links, fifos and sockets"

dir=`dirname $0`
. ${dir}/misc.sh

echo "1..9"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n2} 0755
cdir=`pwd`
cd ${n2}

expect 0 create ${n0} 0644
expect regular lstat ${n0} type
expect 0 unlink ${n0}
expect ENOENT lstat ${n0} type

cd ${cdir}
expect 0 rmdir ${n2}

expect 0 mkdir ${n0} 0755
case "${os}:${fs}" in
SunOS:UFS)
	expect 0 unlink ${n0}
	expect ENOENT rmdir ${n0}
	;;
Linux:*)
	# Non-POSIX value returned by Linux since 2.1.132
	expect EISDIR unlink ${n0}
	expect 0 rmdir ${n0}
	;;
*)
	expect EPERM unlink ${n0}
	expect 0 rmdir ${n0}
	;;
esac
