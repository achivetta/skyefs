fusermount -u /tmp/skye_mnt
sleep 0.1
killall -9 skye_client
cd ~/skyefs
ulimit -c 10000 
. ~/.bashrc

rm -rf /tmp/skye_mnt
mkdir -p /tmp/skye_mnt

./skye_client -o pvfs=tcp://cloud1:13334/pvfs2-fs -f -s /tmp/skye_mnt > ~/logs/skye_client-`hostname`.log 2>&1 &
disown %1

sleep 0.2
mount | grep skye
ps aux | grep skye | grep -v grep
