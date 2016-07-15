#!/usr/local/bin/Rscript

# ./plot_together.R hardware.dat software.dat

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

plot_libpaxos_line <- function(latfile, caans_file) {
    caans <- read.csv(caans_file, header = TRUE, sep="")
    caans$library <- "CAANS"
    caans$latency <- caans$latency * 10^6
    print(summary(caans))

    libpaxos <- read.csv(latfile, header = TRUE, sep="")
    libpaxos$library <- "Libpaxos"
    libpaxos$latency <- libpaxos$latency * 10^6
    print(summary(libpaxos))

    df <- rbind(caans, libpaxos)
    # data <- ddply(df, c("library", "throughput"), summarise, lat=mean(latency), sd = sd(latency))
    data <- summarySE(df, measurevar="latency", groupvars=c("library","throughput"))
    print(head(data))
    write.table(data, "caans_and_libpaxos.csv", row.names=F, quote=F)
    pdf('figures/caans_vs_libpaxos.pdf')
    pd <- position_dodge(.1)
    ggplot(data, aes(x=throughput, y=latency, group=library)) +
    geom_line(aes(linetype=library, color=library), size=1) +
    geom_errorbar(aes(ymin=latency-se, ymax=latency+se), width=.1, position=pd) +
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

plot_boxplot <- function(latfile, caans_file) {
    caans <- read.csv(caans_file, header = TRUE, sep="")
    caans$library <- "CAANS"
    caans$latency <- caans$latency * 10^6
    print(summary(caans))

    libpaxos <- read.csv(latfile, header = TRUE, sep="")
    libpaxos$library <- "Libpaxos"
    libpaxos$latency <- libpaxos$latency * 10^6
    print(summary(libpaxos))

    data <- rbind(caans, libpaxos)
    ylim1 = boxplot.stats(data$latency)$stats[c(1, 5)]
    pdf('figures/output.pdf')
    ggplot(data, aes(x=throughput, y=latency, group=throughput, fill=library)) +
    geom_boxplot(outlier.shape = NA) +
    labs(x="Throughput (Msgs / S) ", y = "Latency (\U00B5s)")+
    theme_bw() +
    my_theme() +
    theme(legend.position = c(.2, .9), legend.title=element_blank()) +
    scale_fill_manual(values=c('#d4444a', '#154fa1')) +
    scale_y_continuous(labels=comma, limits = quantile(data$latency, c(0.1, 0.9))) +
    scale_x_continuous(labels=comma)
}


args <- commandArgs(trailingOnly = TRUE)
print(args)

# plot_libpaxos_line(args[1], args[2])
plot_boxplot(args[1], args[2])
