#!/usr/bin/env python
import argparse
import subprocess
import shlex
import os
from threading import Timer
import time


def run_client(cid, host, path, server_addr, port, output_dir):
    cmd = "ssh danghu@{0} {1}/client_caans {2} {3}".format(host, path,
        server_addr, port)
    print cmd
    with open('%s/client-%d.txt' % (output_dir, cid), 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh

def run_proxy(host, path, config, appconf, proxy_id, proxy_port, output_dir):
    cpu_id = proxy_id
    cmd = "ssh danghu@{0} taskset -c {1} {2}/proxy {3} {4} {5} {6}".format(host,
        cpu_id, path, config, appconf, proxy_id, proxy_port)
    print cmd
    with open('%s/proxy-%d.txt' % (output_dir, proxy_id), 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh


def run_learner(host, path, config, server_id, appconf, output_dir):
    cpu_id = server_id + 1
    cmd = "ssh danghu@{0} taskset -c {1} {2}/learner {3} {4} {5}".format(host,
        cpu_id, path, config, appconf, server_id)
    print cmd
    with open('%s/learner-%d.txt' % (output_dir, server_id), 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh

def run_coordinator(host, path, config, proposer_id, output_dir):
    cpu_id = proposer_id + 7
    cmd = "ssh danghu@{0} taskset -c {1} {2}/proposer {3} {4}".format(host, cpu_id,
     path, proposer_id, config)
    print cmd
    with open('%s/coordinator-%d.txt' % (output_dir, cpu_id), 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh

def run_acceptor(host, path, config, acceptor_id, output_dir):
    cpu_id = acceptor_id
    cmd = "ssh danghu@{0} taskset -c {1} {2}/acceptor {3} {4}".format(host, cpu_id,
        path, acceptor_id, config)
    print cmd
    with open('%s/acceptor-%d.txt' % (output_dir, acceptor_id), 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh


def run_ps(host, output):
    command = 'ps -C proxy,client_caans,learner,proposer,acceptor -o %cpu,%mem,comm --sort %cpu | tail -n4'
    cmd = "ssh {0} {1}".format(host, command)
    with open("%s/%s-cpu.txt" % (output, host), "a+") as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                stdout=out,
                stderr=out,
                shell=False)
    return ssh

def kill_proxies(*proxies):
    for n in proxies:
        cmd = "ssh danghu@%s pkill proxy" % n
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

def kill_clients(*clients):
    for n in clients:
        cmd = "ssh danghu@%s pkill client_caans" % n
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

def kill_learners(*learners):
    for n in learners:
        cmd = "ssh danghu@%s pkill learner" % n
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

def kill_proposers(*proposers):
    for n in proposers:
        cmd = "ssh danghu@%s pkill proposer" % n
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

def kill_acceptors(*acceptors):
    for n in acceptors:
        cmd = "ssh danghu@%s pkill acceptor" % n
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run experiment.')
    parser.add_argument('--time', type=int, default=30, help='experiment time')
    parser.add_argument('--osd', type=int, default=1, help='number of clients')
    parser.add_argument('-c', '--clients', type=int, default=4, help='client per proxy')
    parser.add_argument('output', help='output directory')
    args = parser.parse_args()

    try:
        if not os.path.exists(args.output):
           os.makedirs(args.output)

        args.path = "/home/danghu/workspace/libpaxos/bin/sample"
        args.client_path = "/home/danghu/workspace/libpaxos/bin/caans"
        args.config = "/home/danghu/workspace/libpaxos/libpaxos.conf"
        args.app_config = "/home/danghu/workspace/libpaxos/bin/application.conf"

        # learners = [ 'node95', 'node96', 'node98' ]
        learners = [ 'node95' ]
        acceptors = [ 'node95', 'node96', 'node98' ]
        proxies = [ 'node97', 'node97', 'node97', 'node97', 'node97' ]
        nodes = [ 'node95', 'node96', 'node97', 'node98' ]

        n_proxies = 0;
        if (args.osd % args.clients == 0):
            n_proxies = args.osd / args.clients
        else:
            n_proxies = args.osd / args.clients + 1
        print "number of proxies in use: %d" % n_proxies

        for i in range(len(acceptors)):
            run_acceptor(acceptors[i], args.path, args.config, i, args.output)
        # start coordinators
        run_coordinator('node97', args.path, args.config, 0, args.output)

        # start learner
        for i in range(len(learners)):
            run_learner(learners[i], args.path, args.config, i, args.app_config,
                args.output)

        time.sleep(1)
        for j in range(n_proxies):
            run_proxy(proxies[j], args.path, args.config, args.app_config, j,
                6789+j, args.output)

        time.sleep(1)
        for i in range(args.osd):
            proxy_id = (i / args.clients);
            print "start client %d, on proxy %d" % ( i , proxy_id)
            run_client(i, proxies[proxy_id], args.client_path, proxies[proxy_id],
                6789 + proxy_id, args.output)
    finally:
        t1 = Timer(args.time, kill_proxies, nodes)
        t2 = Timer(args.time, kill_clients, nodes)
        t3 = Timer(args.time, kill_learners, learners)
        t4 = Timer(args.time, kill_acceptors, acceptors)
        t5 = Timer(args.time, kill_proposers, ['node97'])
        t1.start()
        t2.start()
        t3.start()
        t4.start()
        t5.start()

        for i in range(args.time/2):
            for n in nodes:
                run_ps(n, args.output)
            time.sleep(1)
