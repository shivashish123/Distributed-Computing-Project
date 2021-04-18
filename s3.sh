for((i = 30;; ++i)); do
    echo $i
    #./gen $i > int
    #echo "input done"
    time ./a < int > out
     # diff -w <(./a < int) <(./brute < int) || break
done
