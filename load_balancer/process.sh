#! /bin/bash

function cleanup() {
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

trap cleanup EXIT

for set in 0 1 2 3 4 5 6
do
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
        time=$(python plot.py | grep "*** Average completion time")
        if [[ ${PIPESTATUS[0]} -eq 0 ]]; then
            cp histogram.png ./output/histograms/histogram.$set.$probability.png
            echo -e "Set $set | Probability $probability: "$time >> ./output/output.txt
        else
            echo -e "Processing (Set: $set | Probability: $probability%) failed. Skipping..."
        fi
    done
done
