killall -9 gigabench
cd ~/skyefs/util

if [ `hostname` = 'cloud1' ]; then
echo "Removing old scratch directories..."
rm -rf /tmp/skye_mnt/0
mkdir -p /tmp/skye_mnt/0
echo "Done."
fi

./gigabench -D /tmp/skye_mnt -F /h/achivett/benchmark.logs -W create -N 25000 > ~/logs/gigabench-`hostname`.log 2>&1 &
disown %1
