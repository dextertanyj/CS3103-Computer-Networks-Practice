#!/bin/bash

file=jobScheduler

while getopts t:f: flag
do
    case "${flag}" in
        t) type=${OPTARG};;
        f) file=${OPTARG};;
    esac
done

if [[ -z "$type" ]] || [[ "$type" != "python" ]] && [[ "$type" != "c++" ]]; then
    echo "Usage: $0 -t python|c++ [-f FILE_PATH]"
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

mkdir -p ./tmp/{pickles,histograms}
touch ./tmp/output.txt
mv config_client config_client_0
mv config_server config_server_0
for set in 0
do
    cp config_client_$set config_client
    cp config_server_$set config_server
    mkdir -p ./tmp/pickles/set_$set
    echo "Running set $set"
    for multiplier in 2
    do
        probability=$((multiplier*50))
        echo "Running with probability $probability%"
        ./server_client -port 123$set$multiplier -prob $probability &>/dev/null &
        PID=$!
        sleep 1
        if [[ "$type" == "python" ]]; then
            python "$file" "-port" "123$set$multiplier" &>/dev/null &
        fi
        if [[ "$type" == "c++" ]]; then
            ./$file "123$set$multiplier" &>/dev/null &
        fi
        schedulerPID=$!
        wait $PID
        kill -s SIGINT $schedulerPID
        cp client.pickle ./tmp/pickles/set_$set/client.pickle.$set.$probability
        cp server.pickle ./tmp/pickles/set_$set/server.pickle.$set.$probability
        time=$(python plot.py | grep "*** Average completion time")
        if [[ ${PIPESTATUS[0]} -eq 0 ]]; then
            cp histogram.png ./tmp/histograms/histogram.$set.$probability.png
            echo -e "Set $set | Probability $probability: "$time >> ./tmp/output.txt
        fi
    done
done

mv config_client_0 config_client
mv config_server_0 config_server
rm histogram.png
rm client.pickle
rm server.pickle

tar -zcvf output.tar.gz ./tmp/

rm -rf ./tmp

exit 0