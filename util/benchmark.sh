#!/bin/zsh

set -e -x

DEPLOY_DIR=skyefs_deploy
DT=$(date +%Y%m%d%H%M)

for server in pvfs1 pvfs2 pvfs3; do
	ssh $server sudo killall gigabench || true
	ssh $server sudo killall skye_client || true
	ssh $server sudo killall skye_server || true
	ssh $server rm -rf $DEPLOY_DIR
	ssh $server mkdir -p $DEPLOY_DIR
	tar -c skye_server skye_client util/gigabench | ssh $server tar -x -C $DEPLOY_DIR
	ssh $server ulimit -c 10000 "&&" sudo $DEPLOY_DIR/skye_server -f 'tcp://pvfs1:3334/pvfs2-fs' &
done

sleep 10
for server in pvfs1 pvfs2 pvfs3; do
	ssh $server rm -rf "mnt/*"

	ssh $server $DEPLOY_DIR/skye_client -o pvfs=tcp://pvfs3:3334/pvfs2-fs -o allow_root -f -s ~/mnt &

	sleep 1

	ssh $server mkdir -p benchmark_logs/skyefs
	ssh $server mkdir -p mnt/benchmark-$DT/0
done

sleep 10

for server in pvfs1 pvfs2 pvfs3; do 
	ssh $server ./gigabench -D mnt/benchmark-$DT -F benchmark_logs/skyefs/ -W create -N 25000 &
done
