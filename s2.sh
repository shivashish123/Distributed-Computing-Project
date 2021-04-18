for((i = 1;; ++i)); do
    echo $i
    ./gen $i > int
 time     ./a < int > out1
     # diff -w <(./a < int) <(./brute < int) || break
done
