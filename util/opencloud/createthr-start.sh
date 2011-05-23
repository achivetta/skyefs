killall -9 createthr
cd ~/skyefs/util

if [ `hostname` = 'cloud1' ]; then
echo "Removing old scratch directories..."
rm -rf /tmp/skye_mnt_1/0
mkdir -p /tmp/skye_mnt_1/0
echo "Done."
fi

for i in `seq 1 8`; do
	./createthr /tmp/skye_mnt_$i/0 100000 > ~/benchmark.logs/`hostname`-$i-thr.log 2> ~/logs/createthr-$i-`hostname`.log &
	disown %1
done
