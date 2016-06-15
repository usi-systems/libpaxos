#!/usr/local/bin/Rscript

# ./plot_result.R result.dat

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

plot_lines <- function(input) {
    data <- read.csv(input, header = TRUE, sep="")
    pdf('figures/caans_vs_libpaxos.pdf')
    pd <- position_dodge(.1)
    print(head(data))
    ggplot(data, aes(x=throughput, y=latency, group=library)) +
    geom_line(aes(linetype=library, color=library), size=1) +
    geom_errorbar(aes(ymin=latency-se, ymax=latency+se), width=.1, position=pd) +
    geom_point() +
    labs(x="Throughput (Msgs / S) ", y = "Latency (\U00B5s)")+
    theme_bw() +
    my_theme() +
    theme(legend.key.size = unit(2, "lines"),
        legend.position = c(.8, .9), legend.title=element_blank()) +
    scale_colour_manual(values=c('#d4444a', '#154fa1')) +
    scale_y_continuous(labels=comma) + 
    scale_x_continuous(labels=comma, breaks=pretty_breaks(n=3))
}


args <- commandArgs(trailingOnly = TRUE)
plot_lines(args[1])
