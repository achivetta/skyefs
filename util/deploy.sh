#!/bin/zsh

set -e -x

DEPLOY_DIR=skyefs_deploy

for server in pvfs1 pvfs2 pvfs3; do
	ssh $server sudo killall skye_server || true
	ssh $server rm -rf $DEPLOY_DIR
	ssh $server mkdir -p $DEPLOY_DIR
	tar -c skye_server skye_client | ssh $server tar -x -C $DEPLOY_DIR
	ssh $server ulimit -c 10000 "&&" sudo $DEPLOY_DIR/skye_server -f 'tcp://pvfs1:3334/pvfs2-fs' &
done
