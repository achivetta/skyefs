export LD_LIBRARY_PATH=$HOME/orangefs/lib
for i in `seq 1 12` `seq 14 27` `seq 29 33` 35; do
	server=cloud$i
	~/orangefs/src/apps/admin/pvfs2-check-server -h $server -f pvfs2-fs -n tcp -p 13334 && echo $server online.
done
