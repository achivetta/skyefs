echo "$(hostname | cut -d '.' -f 1) $(uptime) 	"
echo "	pvfs2-server: $(pidof pvfs2-server)	skye_server: $(pidof skye_server)	skye_client: $(pidof skye_client | wc -w )	createthr: $(pidof createthr | wc -w)	"
vmstat 1 2 | tail -n 1 | awk '{print "	user:", $13, "system:", $14, "idle:", $15, "wait:", $16}'
