#! /bin/bash

function cleanup() {
    mv config_client_0 config_client &>/dev/null
    mv config_server_0 config_server &>/dev/null
    rm histogram.png &>/dev/null
    rm client.pickle &>/dev/null
    rm server.pickle &>/dev/null

    tar -zcf output.tar.gz ./output/ && rm -rf ./output
}

archive="./output.tar.gz"

while getopts i: flag
do
    case "${flag}" in
        i) archive=${OPTARG};;
    esac
done

if ! [[ -f $archive ]]; then
    exit 1
fi

tar -zxf $archive
mv config_client config_client_0
mv config_server config_server_0

trap cleanup EXIT

for set in 0 1 2 3 4 5 6
do
    cp config_client_$set config_client
    cp config_server_$set config_server
    for multiplier in 2 1 0
    do
        probability=$((multiplier*50))
        if ! cp ./output/pickles/set_$set/client.pickle.$set.$probability client.pickle &> /dev/null; then
            echo -e "Client pickle (Set: $set | Probability: $probability%) not found. Skipping..."
            continue
        fi
        if ! cp ./output/pickles/set_$set/server.pickle.$set.$probability server.pickle &> /dev/null; then
            echo -e "Server pickle (Set: $set | Probability: $probability%) not found. Skipping..."
            continue
        fi
        time=$(python plot.py | grep -F "***")
        if [[ ${PIPESTATUS[0]} -eq 0 ]]; then
            cp histogram.png ./output/histograms/histogram.$set.$probability.png
            echo -e "Set $set | Probability $probability" >> ./output/output.txt
            echo "$time" >> ./output/output.txt
            echo "---" >> ./output/output.txt
        fi
    done
done
