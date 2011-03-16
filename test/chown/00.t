#!/bin/bash
# $FreeBSD: src/tools/regression/fstest/tests/chown/00.t,v 1.1 2007/01/17 01:42:08 pjd Exp $

desc="chown changes ownership"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..60"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n2} 0755
cdir=`pwd`
cd ${n2}

# super-user can always modify ownership
# 2
expect 0 create ${n0} 0644
expect 0 chown ${n0} 123 456
expect 123,456 lstat ${n0} uid,gid
expect 0 chown ${n0} 0 0
expect 0,0 lstat ${n0} uid,gid
expect 0 unlink ${n0}
# 14
expect 0 mkdir ${n0} 0755
expect 0 chown ${n0} 123 456
expect 123,456 lstat ${n0} uid,gid
expect 0 chown ${n0} 0 0
expect 0,0 lstat ${n0} uid,gid
expect 0 rmdir ${n0}
# 20
#expect 0 create ${n0} 0644
#expect 0 symlink ${n0} ${n1}
#expect 0 chown ${n1} 123 456
#expect 123,456 stat ${n1} uid,gid
#expect 123,456 stat ${n0} uid,gid
#expect 0 lchown ${n1} 135 579
#expect 135,579 lstat ${n1} uid,gid
#expect 123,456 stat ${n1} uid,gid
#expect 123,456 stat ${n0} uid,gid
#expect 0 unlink ${n0}
#expect 0 unlink ${n1}

# non-super-user can modify file group if he is owner of a file and
# gid he is setting is in his groups list.
# 31
expect 0 create ${n0} 0644
expect 0 chown ${n0} 65534 65533
expect 65534,65533 lstat ${n0} uid,gid
expect 0 -u 65534 -g 65532,65531 -- chown ${n0} -1 65532
expect 65534,65532 lstat ${n0} uid,gid
expect 0 -u 65534 -g 65532,65531 chown ${n0} 65534 65531
expect 65534,65531 lstat ${n0} uid,gid
expect 0 unlink ${n0}

# successfull chown(2) call (except uid and gid equal to -1) updates ctime.
# 109
expect 0 create ${n0} 0644
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 chown ${n0} 65534 65533
expect 65534,65533 lstat ${n0} uid,gid
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -lt $ctime2
expect 0 unlink ${n0}
# 114
expect 0 mkdir ${n0} 0755
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 chown ${n0} 65534 65533
expect 65534,65533 lstat ${n0} uid,gid
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -lt $ctime2
expect 0 rmdir ${n0}

# 124
#expect 0 symlink ${n1} ${n0}
#ctime1=`${fstest} lstat ${n0} ctime`
#sleep 1
#expect 0 lchown ${n0} 65534 65533
#expect 65534,65533 lstat ${n0} uid,gid
#ctime2=`${fstest} lstat ${n0} ctime`
#test_check $ctime1 -lt $ctime2
#expect 0 unlink ${n0}
# 129
expect 0 create ${n0} 0644
expect 0 chown ${n0} 65534 65533
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 -u 65534 -g 65532 chown ${n0} 65534 65532
expect 65534,65532 lstat ${n0} uid,gid
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -lt $ctime2
expect 0 unlink ${n0}
# 135
expect 0 mkdir ${n0} 0755
expect 0 chown ${n0} 65534 65533
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 -u 65534 -g 65532 chown ${n0} 65534 65532
expect 65534,65532 lstat ${n0} uid,gid
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -lt $ctime2
expect 0 rmdir ${n0}

# 148
#expect 0 symlink ${n1} ${n0}
#expect 0 lchown ${n0} 65534 65533
#ctime1=`${fstest} lstat ${n0} ctime`
#sleep 1
#expect 0 -u 65534 -g 65532 lchown ${n0} 65534 65532
#expect 65534,65532 lstat ${n0} uid,gid
#ctime2=`${fstest} lstat ${n0} ctime`
#test_check $ctime1 -lt $ctime2
#expect 0 unlink ${n0}
# 154
expect 0 create ${n0} 0644
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 -- chown ${n0} -1 -1
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -lt $ctime2
expect 0 unlink ${n0}
# 158
expect 0 mkdir ${n0} 0644
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 -- chown ${n0} -1 -1
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -lt $ctime2
expect 0 rmdir ${n0}

# 166
#expect 0 symlink ${n1} ${n0}
#ctime1=`${fstest} lstat ${n0} ctime`
#sleep 1
#expect 0 -- lchown ${n0} -1 -1
#ctime2=`${fstest} lstat ${n0} ctime`
#test_check $ctime1 -lt $ctime2
#expect 0 unlink ${n0}

# unsuccessful chown(2) does not update ctime.
# 170
expect 0 create ${n0} 0644
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect EPERM -u 65534 -- chown ${n0} 65534 -1
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -eq $ctime2
expect 0 unlink ${n0}
# 174
expect 0 mkdir ${n0} 0755
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect EPERM -u 65534 -g 65534 -- chown ${n0} -1 65534
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -eq $ctime2
expect 0 rmdir ${n0}

# 182
#expect 0 symlink ${n1} ${n0}
#ctime1=`${fstest} lstat ${n0} ctime`
#sleep 1
#expect EPERM -u 65534 -g 65534 lchown ${n0} 65534 65534
#ctime2=`${fstest} lstat ${n0} ctime`
#test_check $ctime1 -eq $ctime2
#expect 0 unlink ${n0}

# 186
cd ${cdir}
expect 0 rmdir ${n2}
