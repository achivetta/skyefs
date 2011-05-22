./run.sh pvfs2-start.sh

echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
echo "Do you want to execute check-pvfs.sh?"
read
./check-pvfs.sh

echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
echo "Do you want to execute skyefs_server-start.sh?"
read
./run.sh skyefs_server-start.sh

echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
echo "Do you want to execute skyefs_server-status.sh?"
read
./run.sh skyefs_server-status.sh

echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
echo "Do you want to execute skyefs_client-start.sh?"
read
./run.sh skyefs_client-start.sh

echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
echo "Do you want to execute skyefs_client-status.sh?"
read
./run.sh skyefs_client-status.sh

echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
echo "Do you want to execute createthr-start.sh?"
read
./run.sh createthr-start.sh

tail -n 1 -f ~/logs/*
