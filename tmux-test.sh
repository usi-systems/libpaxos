#!/usr/bin/env bash

VG=""
if [[ -z "${LIBPAXOS_SRC}" ]]; then
  echo "${LIBPAXOS_SRC} is unset"
  exit 1
fi

if [[ -z "${LIBPAXOS_BIN}" ]]; then
  echo "${LIBPAXOS_BIN} is unset"
  exit 1
fi

BIN="${LIBPAXOS_BIN}/caans"
CONFIG="${LIBPAXOS_SRC}/application.conf"
OPT="--verbose"

tmux_test ()  {
	tmux new-session -d -s paxos
	tmux split-window -h -t paxos
	# tmux select-layout -t paxos tiled
	tmux send-keys -t paxos:0.0 "$BIN/sw_coordinator $CONFIG" C-m
	tmux send-keys -t paxos:0.1 "$BIN/sw_acceptor $CONFIG 0" C-m
	tmux new-window
	tmux split-window -h -t paxos
	tmux split-window -v -t paxos
	tmux send-keys -t paxos:1.0 "$BIN/rock_learner $CONFIG 0 1" C-m
	sleep 2
	tmux send-keys -t paxos:1.1 "$BIN/proxy_caans $CONFIG 0 6789" C-m
	tmux send-keys -t paxos:1.2 "$BIN/c_bench 127.0.0.1 6789" C-m
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
