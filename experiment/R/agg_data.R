#!/usr/local/bin/Rscript

library(tools) 

options(warn=1)
args <- commandArgs(trailingOnly = TRUE)

if ( length(args) < 3 ) {
    stop ( paste("At least three arguments must be supplied\n",
    "agg_data.R mean_throughput aggregated_output [latency_input]+"), call.=FALSE)
}

read_average_throughput <- function(average_throughput_csv) {
    df <- read.csv(average_throughput_csv, header=T)
    return (df)
}


aggegrate <- function(latency_input_file, throughput_dataframe) {
    df <- read.csv(latency_input_file, sep="", col.names=c("osd", "latency"))
    df <- merge(df, throughput_dataframe, by="osd")
    return (df)
}


agg_df <- data.frame(osd=character(), lat=numeric(), tput=numeric())
throughput_dataframe <- read_average_throughput(args[1])

for (i in 3:length(args)) {
    df <- aggegrate(args[i], throughput_dataframe)
    agg_df <- rbind(agg_df, df)
}

# agg_df <- na.omit(agg_df)
write.table(agg_df, args[2], row.names=F, quote=F)
