#!/usr/local/bin/Rscript

library(ggplot2)
library(tools)
library(plyr)
library(reshape2)
library(scales)

options(warn=1)

my_theme <- function() {
    theme(panel.grid.major.x = element_blank(),
        panel.grid.minor.x = element_blank(),
        text = element_text(size=34, family='Times'),
        axis.title.y=element_text(margin=margin(0,10,0,0)),
        axis.title.x=element_text(margin=margin(10,0,0,0)),
        legend.text = element_text(size=30, family='Times'),
        legend.position = c(.8, .6)
    )
}

plot_libpaxos_line <- function(latfile) {
    df <- read.csv(latfile, header=T, sep="")
    df$library <- "Libpaxos"
    df$latency <- df$latency * 10^6
    df <- na.omit(df)
    print(summary(df))
    data <- ddply(df, c("library", "throughput"), summarise, lat=mean(latency), sd = sd(latency))
    print(summary(data))
    pdf('figures/output.pdf')
    ggplot(data, aes(x=throughput, y=lat, fill=library)) +
    geom_line(size=1, colour='#d4444a') +
    geom_errorbar(aes(ymin=lat-sd, ymax=lat+sd), width=.2,
                 position=position_dodge(.9)) +
    labs(x="Throughput (Msgs / S) ", y = "Latency (\U00B5s)")+
    theme_bw() +
    my_theme() +
    theme(legend.position = c(.2, .9), legend.title=element_blank()) +
    # scale_fill_manual(values=c('#d4444a', '#154fa1')) +
    scale_fill_manual(values=c('#d4444a')) +
    scale_y_continuous(labels=comma) + 
    scale_x_continuous(labels=comma)
}

boxplot_libpaxos <- function(latfile) {
    df <- read.csv(latfile, header=T, sep="")
    df$library <- "Libpaxos"
    df$latency <- df$latency * 10^6
    df <- na.omit(df)
    print(summary(df))
    # data <- ddply(df, c("library", "throughput"), summarise, lat=mean(latency), sd = sd(latency))
    data <- df
    print(summary(data))
    pdf('figures/output.pdf')
    ggplot(data, aes(x=throughput, y=latency, group=throughput)) +
    geom_boxplot() +
    # geom_errorbar(aes(ymin=lat-sd, ymax=lat+sd), width=.2,
    #              position=position_dodge(.9)) +
    labs(x="Throughput (Msgs / S) ", y = "Latency (\U00B5s)")+
    theme_bw() +
    my_theme() +
    theme(legend.position = c(.2, .9), legend.title=element_blank()) +
    # scale_fill_manual(values=c('#d4444a', '#154fa1')) +
    scale_fill_manual(values=c('#d4444a')) +
    # scale_y_continuous(labels=comma) + 
    scale_y_log10(labels=comma) +
    scale_x_continuous(labels=comma)
}


args <- commandArgs(trailingOnly = TRUE)
print(args)

# plot_libpaxos_line(args[1])
boxplot_libpaxos(args[1])
