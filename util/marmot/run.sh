#!/bin/bash

set -e
#set -v

clients=8

hostname=$(hostname | cut -d '.' -f 1)
rm -f ~/logs/*$hostname*

export HOSTNAME=$hostname.eth1

#set up server
sudo killall -q pvfs2-server || true

sudo rm -rf /tmp/pvfs2-storage-space
sudo mkdir -p /tmp/pvfs2-storage-space

sudo pvfs2-server pvfs2-fs.conf -f 
sudo pvfs2-server pvfs2-fs.conf

sleep 5

echo "$hostname: Starting sync for PVFS STARTUP"
if /usr/testbed/bin/emulab-sync -m; then
	/usr/testbed/bin/emulab-sync -i 31 -n pvfs-$1
else
	/usr/testbed/bin/emulab-sync -n pvfs-$1
fi
echo "$hostname: Finished sync for PVFS STARTUP"

killall skye_client || true
killall skye_server || true
killall createthr || true

ulimit -c 10000

cd ~/skyefs
./skye_server -f 'tcp://node-0.skyefs.gigaplus:3334/pvfs2-fs' > ~/logs/skye_server-$hostname.log 2>&1 &
disown %1

sleep 10

echo "$hostname: Started sync for skye_server STARTUP"
if /usr/testbed/bin/emulab-sync -m; then
	/usr/testbed/bin/emulab-sync -i 31 -n server-$1
else
	/usr/testbed/bin/emulab-sync -n server-$1
fi
echo "$hostname: Finished sync for skye_server STARTUP"

for i in `seq 1 $clients`; do
	fusermount -u /tmp/skye_mnt_$i || true
	sudo rm -rf /tmp/skye_mnt_$i
	mkdir -p /tmp/skye_mnt_$i
	./skye_client -o pvfs=tcp://node-0.skyefs.gigaplus:3334/pvfs2-fs -f -s /tmp/skye_mnt_$i > ~/logs/skye_client-${hostname}_$i.log 2>&1 &
	disown %1
done

sleep 10

echo "$hostname: Started sync for skye_client STARTUP"
if /usr/testbed/bin/emulab-sync -m; then
	mkdir /tmp/skye_mnt_1/0
	/usr/testbed/bin/emulab-sync -i 31 -n client-$1
else
	/usr/testbed/bin/emulab-sync -n client-$1
fi
echo "$hostname: Finished sync for skye_client STARTUP"

for i in `seq 1 $clients`; do
	cd /tmp/skye_mnt_$i/0
	./util/createthr ./ 4000 > ~/logs/createthr-${hostname}_$i.out 2> ~/logs/createthr-${hostname}_$i.log &
	disown %1
done

echo "$hostname: Workload started."
