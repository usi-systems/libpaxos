#!/bin/sh

mkdir -p sample
mkdir -p csv
mkdir -p figures
rm -f sample/*
rm -f csv/*

for i in $1/*/
do
    i=${i%*/}
    i=${i##*/}
    for j in $( find $1/$i -name client* ); do
        sed -e '/[a-zA-Z]/d' $j | awk -v var=$i '{print var, $0}' >> sample/"$i".csv
    done
    for tpf in $( find $1/$i -name "server*" ); do
        sed '1,4d;$d' $tpf | awk -v var=$i '{print var, $0}' >> "csv/$1_tp.csv"
    done
done
./R/avg_throughput.R "csv/$1_tp.csv" "csv/$1_mean_throughput.csv"
./R/agg_data.R "csv/$1_mean_throughput.csv" "csv/$1_aggregated_data.csv" sample/*
./R/plot_baseline.R "csv/$1_aggregated_data.csv"
open figures/*.pdf
