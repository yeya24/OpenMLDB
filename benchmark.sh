#! /bin/sh
#
# benchmark.sh



ROOT_DIR=`pwd`

clear_debug() {
    ps -ef | grep rtidb | grep 9426 | awk '{print $2}' | while read line; do kill -9 $line; done
}

clear_debug

./build/bin/rtidb --log_level=debug --gc_safe_offset=0 --gc_interval=1 --endpoint=0.0.0.0:9426 --role=tablet > log 2>&1 &

sleep 2

./build/bin/rtidb --cmd="create t0 1 1 0" --role=client --endpoint=127.0.0.1:9426 --interactive=false 

./build/bin/rtidb --cmd="benchmark" --role=client --endpoint=127.0.0.1:9426 --interactive=false | grep Percentile
./build/bin/rtidb --cmd="drop 1" --role=client --endpoint=127.0.0.1:9426 --interactive=false


clear_debug

