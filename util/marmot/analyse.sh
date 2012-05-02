paste createthr-node-*.out | awk 'BEGIN{line=0} {sum = 0;for(i=0;i<=NF;i++){sum+=$i; ++i;}; print line " " sum " " $0; line++}'
