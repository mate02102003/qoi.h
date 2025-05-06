#!/bin/bash

set -x

gcc main.c -o main -Wall -Wextra -I. -ggdb
./main $1