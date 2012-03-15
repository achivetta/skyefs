#!/bin/bash
for server in pvfs{1,2,3}; do
	scp skye_server $server:skyefs_deploy
	scp skye_client $server:skyefs_deploy
	scp util/createthr $server:skyefs_deploy/util/
	scp util/achivett/* $server:skyefs_deploy/util/
done

for server in pvfs{1,2,3}; do
	ssh $server ./skyefs_deploy/util/run.sh &
done
