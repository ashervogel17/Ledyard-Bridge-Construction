#!/bin/bash

# Compile C code
gcc -o ./bridge driver.c

# Array of test values
total_cars_cases=(1 5 10 10 100)
max_cars_cases=(1 1 2 5 10)
num_test_cases=5

# Iterate over test cases
for ((i=0; i<$num_test_cases; i++))
do
    echo
    total_cars=${total_cars_cases[$i]}
    max_cars=${max_cars_cases[$i]}
    echo Running program with TOTAL_CARS=$total_cars and MAX_CARS=$max_cars
    ./bridge $total_cars $max_cars
    echo --------
    
done