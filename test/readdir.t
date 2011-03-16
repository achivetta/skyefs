#!/bin/sh

desc="lists files in a directory"

dir=`dirname $0`
. ${dir}/misc.sh

echo "1..4"

n0=`namegen`
n1=`namegen`
n2=`namegen`
n3=`namegen`

expect 0 mkdir ${n1} 0755
cdir=`pwd`
cd ${n1}

echo $n1  > $n1 && echo 'ok 2' || echo 'not ok 2'
echo $n2 >> $n1
echo 0 >> $n1

for filename in `seq 1 100`; do
	touch $filename;
	echo $filename >> $n1;
done

sort -n $n1 > $n3
mv $n3 $n2

${fstest} readdir . > $n2
sort -n $n2 > $n3
mv $n3 $n2

diff -q $n1 $n2 >/dev/null && echo 'ok 3' || echo 'not ok 3'

#rm $n1 $n2

ntest=`expr $ntest + 2`

cd ${cdir}
expect 0 rmdir ${n1}
