#!/usr/bin/env bash

VG=""
BIN="build/caans"
CONFIG="application.conf"
OPT="--verbose"

tmux_test ()  {
	tmux new-session -d -s paxos
	tmux split-window -v -t paxos
	tmux split-window -v -t paxos
	tmux split-window -v -t paxos
	# tmux split-window -v -t paxos
	tmux split-window -v -t paxos
	tmux select-layout -t paxos tiled
	tmux send-keys -t paxos:0.0 "./$BIN/rock_learner $CONFIG 0 1" C-m
	tmux send-keys -t paxos:0.1 "./$BIN/sw_acceptor $CONFIG 0" C-m
	sleep 2
	tmux send-keys -t paxos:0.4 "./$BIN/sw_coordinator $CONFIG" C-m
	tmux send-keys -t paxos:0.2 "./$BIN/proxy_caans $CONFIG 0 6789" C-m
	tmux send-keys -t paxos:0.3 "./$BIN/c_bench 10.20.2.132 6789" C-m
	tmux attach-session -t paxos
	tmux kill-session -t paxos
}

usage () {
	echo "$0 [--help] [--build-dir dir] [--config-file]"
	exit 1
}

while [[ $# > 0 ]]; do
	key="$1"
	case $key in
		-b|--build-dir)
		DIR=$2
		shift
		;;
		-c|--config)
		CONFIG=$2
		shift
		;;
		-h|--help)
		usage
		;;
		*)
		usage
		;;
	esac
	shift
done

tmux_test
