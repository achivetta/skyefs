#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/rename/00.t,v 1.1 2007/01/17 01:42:10 pjd Exp $

desc="rename changes file name"

dir=`dirname $0`
. ${dir}/misc.sh

echo "1..41"

n0=`namegen`
n1=`namegen`
n2=`namegen`
n3=`namegen`

expect 0 mkdir ${n3} 0755
cdir=`pwd`
cd ${n3}

expect 0 create ${n0} 0644
expect regular,0644,1 lstat ${n0} type,mode,nlink
inode=`${fstest} lstat ${n0} inode`
expect 0 rename ${n0} ${n1}
expect ENOENT lstat ${n0} type,mode,nlink
expect regular,${inode},0644,1 lstat ${n1} type,inode,mode,nlink
expect 0 rename ${n1} ${n2}
expect ENOENT lstat ${n1} type,mode,nlink
expect regular,${inode},0644,1 lstat ${n2} type,inode,mode,nlink
expect 0 unlink ${n2}

expect 0 mkdir ${n0} 0755
expect dir,0755 lstat ${n0} type,mode
inode=`${fstest} lstat ${n0} inode`
expect 0 rename ${n0} ${n1}
expect ENOENT lstat ${n0} type,mode
expect dir,${inode},0755 lstat ${n1} type,inode,mode
expect 0 rmdir ${n1}

cd ${cdir}
expect 0 rmdir ${n3}

expect 0 mkdir ${n0} 0755

expect 0 create ${n1} 0644
expect EISDIR rename ${n1} ${n0}
expect dir lstat ${n0} type
expect regular lstat ${n1} type
expect 0 unlink ${n1}

#expect 0 symlink test ${n1}
#expect EISDIR rename ${n1} ${n0}
#expect dir lstat ${n0} type
#expect symlink lstat ${n1} type
#expect 0 unlink ${n1}

expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755

expect 0 create ${n1} 0644
expect ENOTDIR rename ${n0} ${n1}
expect dir lstat ${n0} type
expect regular lstat ${n1} type
expect 0 unlink ${n1}

#expect 0 symlink test ${n1}
#expect ENOTDIR rename ${n0} ${n1}
#expect dir lstat ${n0} type
#expect symlink lstat ${n1} type
#expect 0 unlink ${n1}

expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect 0 mkdir ${n1} 0755

expect 0 create ${n1}/${n2} 0644
expect "EEXIST|ENOTEMPTY" rename ${n0} ${n1}
expect 0 unlink ${n1}/${n2}

expect 0 mkdir ${n1}/${n2} 0755
expect "EEXIST|ENOTEMPTY" rename ${n0} ${n1}
expect 0 rmdir ${n1}/${n2}

expect 0 rmdir ${n1}
expect 0 rmdir ${n0}
