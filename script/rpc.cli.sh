# !/bin/bash

root=$(cd $(dirname $0)/..; pwd)

export LD_LIBRARY_PATH=$root/src

$root/benchmark/cli --flagfile=$root/script/rpc.cli.conf
