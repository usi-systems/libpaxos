#!/usr/local/bin/Rscript

library(ggplot2)
library(tools)
library(plyr)
library(reshape2)
library(scales)
library(Rmisc)

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
    df <- read.csv(latfile, header=TRUE, sep="")
    df$library <- "Libpaxos"
    df$latency <- df$latency * 10^6
    print(summary(df))
    data <- summarySE(df, measurevar="latency", groupvars=c("library","throughput"))
    pdf('figures/output.pdf')
    pd <- position_dodge(.1)
    ggplot(data, aes(x=throughput, y=latency, group=library)) +
    geom_line(aes(linetype=library, color=library), size=1) +
    geom_errorbar(aes(ymin=latency-sd, ymax=latency+sd), width=.1, position=pd) +
    geom_point() +
    labs(x="Throughput (Msgs / S) ", y = "Latency (\U00B5s)")+
    theme_bw() +
    my_theme() +
    theme(legend.key.size = unit(2, "lines"),
        legend.position = c(.2, .9), legend.title=element_blank()) +
    scale_colour_manual(values=c('#d4444a', '#154fa1')) +
    scale_y_continuous(labels=comma) + 
    scale_x_continuous(labels=comma, breaks = c(50000, 100000))
}

boxplot_libpaxos <- function(latfile) {
    df <- read.csv(latfile, header=T, sep="")
    df$library <- "Libpaxos"
    df$latency <- df$latency * 10^6
    df <- na.omit(df)
    print(summary(df))
    data <- df
    print(summary(data))
    ylim1 = boxplot.stats(data$latency)$stats[c(1, 5)]
    pdf('figures/output.pdf')
    ggplot(data, aes(x=throughput, y=latency, group=throughput)) +
    geom_boxplot(outlier.shape = NA) +
    labs(x="Throughput (Msgs / S) ", y = "Latency (\U00B5s)")+
    theme_bw() +
    my_theme() +
    theme(legend.position = c(.2, .9), legend.title=element_blank()) +
    # scale_fill_manual(values=c('#d4444a', '#154fa1')) +
    scale_fill_manual(values=c('#d4444a')) +
    # scale_y_continuous(labels=comma) + 
    scale_y_continuous(labels=comma, limits = quantile(data$latency, c(0.1, 0.9))) +
    scale_x_continuous(labels=comma)
}


args <- commandArgs(trailingOnly = TRUE)
print(args)

# plot_libpaxos_line(args[1])
boxplot_libpaxos(args[1])
