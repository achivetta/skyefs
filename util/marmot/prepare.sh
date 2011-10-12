#!/bin/bash
sudo apt-get -y install build-essential bison libfuse{2,-dev} libdb4.8{,-dev} pkg-config
(cd ~/orangefs; sudo make install)

sudo rm -rf /tmp/pvfs2-storage-space
mkdir -p /tmp/pvfs2-storage-space

sudo mkdir -p /mnt/pvfs

#sudo insmod /lib/modules/2.6.38.7-1.0emulab/kernel/fs/pvfs2/pvfs2.ko

#sudo pvfs2-server pvfs2-fs.conf -f && sudo pvfs2-server pvfs2-fs.conf

#if /usr/testbed/bin/emulab-sync -m; then
	#/usr/testbed/bin/emulab-sync -i 32
#else
	#/usr/testbed/bin/emulab-sync
#fi

#sudo pvfs2-client

#sudo mount -t pvfs2 tcp://node-0.skyefs.testbed:3334/pvfs2-fs /mnt/pvfs
