for i in `seq 1 12` `seq 14 27` `seq 29 33` 35; do
	server=cloud$i
	echo "############################## $server ##############################"
	ssh $server ~/bin/$*
done
