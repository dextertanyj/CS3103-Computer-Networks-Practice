#!/bin/bash

function cleanup() {
    mv config_client_0 config_client &>/dev/null
    mv config_server_0 config_server &>/dev/null
    rm histogram.png &>/dev/null
    rm client.pickle &>/dev/null
    rm server.pickle &>/dev/null

    tar -zcf output.tar.gz ./output/ && rm -rf ./output
}

file=jobScheduler
stdout="/dev/null"
stderr="/dev/null"
process=true

while getopts t:f:vsd flag
do
    case "${flag}" in
        t) type=${OPTARG};;
        f) file=${OPTARG};;
        v) stderr="/dev/stderr";;
        d) stdout="/dev/stdout"
           stderr="/dev/stderr";;
        s) process=false;;
    esac
done

if [[ -z "$type" ]] || [[ "$type" != "python" ]] && [[ "$type" != "c++" ]]; then
    echo "Usage: $0 -t python|c++ [-f FILE_PATH] [-v] [-d] [-s]"
    echo "Options:  -t Type of program"
    echo "          -f Path to program"
    echo "          -v Verbose"
    echo "          -d Debug"
    echo "          -s Skip plotting"
    exit 1
fi

if [[ "$type" == "python" ]] && ! [[ "$file" =~ .*\.py ]]; then
    file=$file".py"
fi

if ! [[ -f "./$file" ]]; then
    echo "File ./$file not found"
    exit 2
fi

chmod +x ./server_client

mkdir -p ./output/{pickles,histograms}
touch ./output/output.txt
mv config_client config_client_0
mv config_server config_server_0

trap cleanup EXIT

for set in 0 1 2 3 4 5 6
do
    cp config_client_$set config_client
    cp config_server_$set config_server
    mkdir -p ./output/pickles/set_$set
    echo "Running set $set"
    for multiplier in 2 1 0
    do
        probability=$((multiplier*50))
        echo "Running with probability $probability%"
        ./server_client -port 123$set$multiplier -prob $probability 1>$stdout 2>$stderr &
        PID=$!
        sleep 1
        if [[ "$type" == "python" ]]; then
            python "$file" "-port" "123$set$multiplier" 1>$stdout 2>$stderr &
        fi
        if [[ "$type" == "c++" ]]; then
            ./$file "123$set$multiplier" 1>$stdout 2>$stderr &
        fi
        schedulerPID=$!
        wait $PID
        kill -s SIGINT $schedulerPID
        cp client.pickle ./output/pickles/set_$set/client.pickle.$set.$probability
        cp server.pickle ./output/pickles/set_$set/server.pickle.$set.$probability
        if $process; then
            time=$(python plot.py | grep "*** Average completion time")
            if [[ ${PIPESTATUS[0]} -eq 0 ]]; then
                cp histogram.png ./output/histograms/histogram.$set.$probability.png
                echo -e "Set $set | Probability $probability: "$time >> ./output/output.txt
            fi
        fi
    done
done
