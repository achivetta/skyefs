#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/rmdir/00.t,v 1.1 2007/01/17 01:42:11 pjd Exp $

desc="rmdir removes directories"

dir=`dirname $0`
. ${dir}/misc.sh

echo "1..22"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect dir lstat ${n0} type
expect 0 rmdir ${n0}
expect ENOENT lstat ${n0} type

expect 0 mkdir ${n0} 0755
expect 0 create ${n0}/${n1} 0644
expect ENOTDIR rmdir ${n0}/${n1}/test
expect 0 unlink ${n0}/${n1}
expect 0 rmdir ${n0}

expect 0 create ${n0} 0644
expect ENOTDIR rmdir ${n0}
expect 0 unlink ${n0}

expect 0 mkdir ${n0} 0755
expect 0 rmdir ${n0}
expect ENOENT rmdir ${n0}
expect ENOENT rmdir ${n1}

expect 0 mkdir ${n0} 0755
expect 0 mkdir ${n0}/${n1} 0755
expect EINVAL rmdir ${n0}/${n1}/.
case "${os}" in
FreeBSD)
	expect EINVAL rmdir ${n0}/${n1}/..
	;;
Linux)
	expect 'ENOTEMPTY|EEXIST' rmdir ${n0}/${n1}/..
	;;
*)
	expect EEXIST rmdir ${n0}/${n1}/..
	;;
esac
expect 0 rmdir ${n0}/${n1}
expect 0 rmdir ${n0}
