#!/bin/bash
set -e
#set -v

hostname=$(hostname | cut -d '.' -f 1)

#set up server
sudo killall -q pvfs2-server || true

sudo rm -rf /pvfs2-storage-space
sudo mkdir -p /pvfs2-storage-space

sudo pvfs2-server pvfs2-fs.conf -f 
sudo pvfs2-server pvfs2-fs.conf

sleep 5

killall skye_client || true
killall skye_server || true
killall createthr || true

ulimit -c 10000

echo "$hostname: Starting skye_server."

cd ~/skyefs_deploy
./skye_server -f 'tcp://pvfs1:3334/pvfs2-fs' 
disown %1

sleep 5

echo "$hostname: Starting skye_client."

for i in 1; do
	fusermount -u /tmp/skye_mnt_$i || true
	sudo rm -rf /tmp/skye_mnt_$i
	mkdir -p /tmp/skye_mnt_$i
	./skye_client -o pvfs=tcp://node-0.skyefs.testbed:3334/pvfs2-fs -f -s /tmp/skye_mnt_$i 
	disown %1
done

sleep 5

echo "$hostname: Starting Workload."

for i in 1; do
	./util/createthr /tmp/skye_mnt_$i/0 4000 
	disown %1
done
