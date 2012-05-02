while true; do for hostname in node-{0..31}.skyefs.gigaplus.marmot.pdl.cmu.local; do ssh $hostname ./status.sh ; done; echo; echo; done
