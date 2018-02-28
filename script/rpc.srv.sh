# !/bin/bash

root=$(cd $(dirname $0)/..; pwd)

export LD_LIBRARY_PATH=$root/src

$root/benchmark/rpc/srv --flagfile=$root/script/rpc.srv.conf
