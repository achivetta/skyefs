killall -9 createthr
cd ~/skyefs/util

if [ `hostname` = 'cloud1' ]; then
echo "Removing old scratch directories..."
rm -rf /tmp/skye_mnt/0
mkdir -p /tmp/skye_mnt/0
echo "Done."
fi

./createthr /tmp/skye_mnt/0 25000 > ~/benchmark.logs/`hostname`_thr.log 2> ~/logs/gigabench-`hostname`.log &
sleep 0.2
disown %1
