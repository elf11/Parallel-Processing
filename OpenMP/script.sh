for i in 1 2 4 8
do
  export OMP_SCHEDULE=dynamic
  export OMP_NUM_THREADS=$i
  time ./paralel $1 $2 out.txt
  diff out.txt $3
done
