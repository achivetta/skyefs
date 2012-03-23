#!/bin/zsh

set -e

DEPLOY_DIR=skyefs_deploy

for server in pvfs1 pvfs2 pvfs3; do
	ssh $server killall skye_server || true
	ssh $server rm -rf $DEPLOY_DIR
	ssh $server mkdir -p $DEPLOY_DIR/util
	scp skye_server $server:$DEPLOY_DIR
	scp skye_client $server:$DEPLOY_DIR
	scp util/createthr $server:$DEPLOY_DIR/util/
	scp util/achivett/* $server:$DEPLOY_DIR/util/
	ssh $server $DEPLOY_DIR/skye_server -f 'tcp://pvfs1:3334/pvfs2-fs' &
done
