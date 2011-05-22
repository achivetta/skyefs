killall skye_client
killall skye_server
cd ~/skyefs
ulimit -c 10000 
. ~/.bashrc

./skye_server -f 'tcp://cloud1:13334/pvfs2-fs' > ~/logs/skye_server-`hostname`.log 2>&1 &
disown %1
