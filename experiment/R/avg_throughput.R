#!/usr/local/bin/Rscript

args <- commandArgs(trailingOnly = TRUE)

if ( length(args) == 0 ) {
    stop ( paste("At least one argument must be supplied (input file)\n",
    "avg_throughput.R input_file [output]"), call.=FALSE)
} else if ( length(args) == 1 ) {
    # default output file
    args[2] = "out.csv"
}

library(plyr)

avg_throughput <- function(input_csv, output_csv) {
    df <- read.csv(input_csv, sep="", col.names=c("osd", "second", "tput"))
    print(summary(df))
    data <- ddply(df, "osd", function(x) head(x[order(x$tput, decreasing = TRUE),], 10))
    data <- ddply(data, "osd", summarise, throughput=round(mean(tput), 0))
    sorted <- data[order(data$throughput),]
    print(sorted)
    write.csv(sorted, output_csv, row.names=F, quote=F)
}

avg_throughput(args[1], args[2])