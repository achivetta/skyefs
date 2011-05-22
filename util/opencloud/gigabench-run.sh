
echo "Removing old scratch directories..."
rm -rf /tmp/skye_mnt/0
mkdir -p /tmp/skye_mnt/0
echo "Done."

for i in `seq 1 27` `seq 29 33`; do
	server=cloud$i
	ssh $server killall -9 gigabench
	ssh $server skyefs/util/gigabench -D /tmp/skye_mnt -F /h/achivett/benchmark.logs -W create -N 25000 &
done
