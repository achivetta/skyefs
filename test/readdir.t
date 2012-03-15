#!/bin/sh

desc="lists files in a directory"

dir=`dirname $0`
. ${dir}/misc.sh

echo "1..5"

n0=`namegen`
n1=`namegen`
n2=`namegen`
n3=`namegen`

expect 0 mkdir ${n1} 0755
cdir=`pwd`
cd ${n1}

echo 0 > $n1 && echo 'ok 2' || echo 'not ok 2'

for filename in `seq 1 100`; do
	touch $filename;
	echo $filename >> $n1;
done

sort -g $n1 > $n3
mv $n3 $n2

${fstest} readdir . | grep -v fstest | grep '[0-9]' > $n2
sort -g $n2 > $n3
mv $n3 $n2

diff $n1 $n2 >&2 && echo 'ok 3' || echo 'not ok 3'

rm * && echo 'ok 4' || echo 'not ok 4'

ntest=`expr $ntest + 3`

cd ${cdir}
expect 0 rmdir ${n1}
