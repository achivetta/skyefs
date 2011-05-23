fusermount -u /tmp/skye_mnt
sleep 0.1
killall -9 skye_client
cd ~/skyefs
ulimit -c 10000 
. ~/.bashrc


for i in `seq 1 8`; do
	rm -rf /tmp/skye_mnt_$i
	mkdir -p /tmp/skye_mnt_$i
	./skye_client -o pvfs=tcp://cloud1:13334/pvfs2-fs -f -s /tmp/skye_mnt_$i > ~/logs/skye_client-$i-`hostname`.log 2>&1 &
	disown %1
done
