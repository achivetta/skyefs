killall createthr
killall skye_client
killall skye_server
killall pvfs2-client
killall -9 pvfs2-server
rm -rf /l/d1/skye-ajc
cd ~/orangefs/src/server/
./pvfs2-server -f ~/pvfs2.conf && ./pvfs2-server ~/pvfs2.conf || echo "UNABLE TO START PVFS SERVER"
