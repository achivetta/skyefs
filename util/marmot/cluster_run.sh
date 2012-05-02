for hostname in node-{0..31}.skyefs.gigaplus.marmot.pdl.cmu.local; do (ssh $hostname ./run.sh $1 &) ; done
