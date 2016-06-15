#!/usr/bin/env python
import argparse
import subprocess
import shlex
import os
from threading import Timer
import time


def client(cid, host, path, server_addr, port, output_dir):
    cmd = "ssh danghu@{0} {1}/client_caans {2} {3}".format(host, path,
        server_addr, port)
    print cmd
    with open('%s/client-%d.txt' % (output_dir, cid), 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh

def proxy(host, path, config, proxy_id, proxy_port, output_dir):
    cmd = "ssh danghu@{0} taskset -c {3} {1}/proxy_caans {2} {3} {4}".format(host, path,
        config, proxy_id, proxy_port)
    print cmd
    with open('%s/proxy-%d.txt' % (output_dir, proxy_id), 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh


def learner(host, path, config, server_id, number_of_server, output_dir, leveldb):
    cmd = "ssh danghu@{0} taskset -c {5} {1}/server_caans {2} {3} {4}".format(host, path, config, server_id, number_of_server, server_id+1)
    if leveldb:
        cmd = "ssh danghu@{0} taskset -c {5} {1}/server_caans {2} {3} {4} 1".format(host, path, config, server_id, number_of_server, server_id+1)
    print cmd
    with open('%s/server-%d.txt' % (output_dir, server_id), 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh

def sw_coordinator(host, path, config, cpu_id, output_dir):
    cmd = "ssh danghu@{0} taskset -c {3} {1}/sw_coordinator {2}".format(host, path,
        config, cpu_id)
    print cmd
    with open('%s/coordinator-%d.txt' % (output_dir, cpu_id), 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh

def sw_acceptor(host, path, config, acceptor_id, output_dir):
    cmd = "ssh danghu@{0} taskset -c {3} {1}/sw_acceptor {2} {3}".format(host, path,
        config, acceptor_id)
    print cmd
    with open('%s/acceptor-%d.txt' % (output_dir, acceptor_id), 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh

def reset_coordinator(host, path):
    cmd = "ssh danghu@{0} NOPROGRAM=1 {1}/ubuntu.exe -C -i 0".format(host, path)
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd))
    ssh.wait()


def run_ps(host, output):
    command = 'ps -C proxy_caans,client_caans,server_caans,sw_coordinator,sw_acceptor -o %cpu,%mem,comm --sort %cpu | tail -n4'
    cmd = "ssh {0} {1}".format(host, command)
    with open("%s/%s-cpu.txt" % (output, host), "a+") as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                stdout=out,
                stderr=out,
                shell=False)
    return ssh

def kill_proxies(*proxies):
    for n in proxies:
        cmd = "ssh danghu@%s pkill proxy_caans" % n
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

def kill_client(*clients):
    for n in clients:
        cmd = "ssh danghu@%s pkill client_caans" % n
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

def kill_learners(*learners):
    for n in learners:
        cmd = "ssh danghu@%s pkill server_caans" % n
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

def kill_sw_coordinator(*coordinators):
    for n in coordinators:
        cmd = "ssh danghu@%s pkill sw_coordinator" % n
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

def kill_sw_acceptor(*acceptors):
    for n in acceptors:
        cmd = "ssh danghu@%s pkill sw_acceptor" % n
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()


def kill_all(*pipes, **nodes):
    cmd = "ssh danghu@%s pkill server_caans" % nodes['server']
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd))
    ssh.wait()

    cmd = "ssh danghu@%s pkill proxy_caans" % nodes['proxy']
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd))
    ssh.wait()

    cmd = "ssh danghu@%s pkill client_caans" % nodes['client']
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd))
    ssh.wait()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run experiment.')
    parser.add_argument('--time', type=int, default=30, help='experiment time')
    parser.add_argument('--osd', type=int, default=1, help='number of clients')
    parser.add_argument('-c', '--clients', type=int, default=4, help='client per proxy')
    parser.add_argument('-s', '--software', default=False, action="store_true",
        help='Flag to run software libpaxos')
    parser.add_argument('-l', '--leveldb', default=False, action="store_true",
        help='Flag to run leveldb')
    parser.add_argument('output', help='output directory')
    args = parser.parse_args()

    try:
        if not os.path.exists(args.output):
           os.makedirs(args.output)

        args.path   = "/home/danghu/workspace/libpaxos/bin/caans"
        # Run libpaxos or CAANS
        if args.software:
            args.config = "/home/danghu/workspace/libpaxos/bin/sw_multicast.conf"
        else:
            args.config = "/home/danghu/workspace/libpaxos/bin/multicast.conf"
            ubuntu_exe_path = "/opt/sonic-lite/p4/examples/paxos/nfsume/bin"
            reset_coordinator("node97", ubuntu_exe_path)
            print "Reset finished."


        pipes = []
        # nodes = { 'server' : 'node95', 'proxy' : 'node97', 'client' : 'node97' }
        learners = [ 'node95', 'node96', 'node98' ]
        acceptors = [ 'node95', 'node96', 'node98' ]
        proxies = [ 'node97', 'node97', 'node97', 'node97', 'node97',
                     'node97', 'node97', 'node97', 'node97', 'node97' ]
        nodes = [ 'node95', 'node96', 'node97', 'node98' ]

        n_proxies = 0;
        if (args.osd % args.clients == 0):
            n_proxies = args.osd / args.clients
        else:
            n_proxies = args.osd / args.clients + 1
        print "number of proxies in use: %d" % n_proxies

        if args.software:
            # start acceptors
            for i in range(len(acceptors)):
                sw_acceptor(learners[i], args.path, args.config, i, args.output)
            # start coordinators
            sw_coordinator('node97', args.path, args.config, 11, args.output)

        # start learner
        for i in range(len(learners)):
            pipes.append(learner(learners[i], args.path, args.config, i, len(learners), args.output, args.leveldb))

        for j in range(n_proxies):
            print "start proxy %d" % j
            pipes.append(proxy(proxies[j], args.path, args.config, j, 6789+j,
                args.output))

        time.sleep(1)
        for i in range(args.osd):
            proxy_id = (i / args.clients);
            print "start client %d, on proxy %d" % ( i , proxy_id)
            pipes.append(client(i, proxies[proxy_id], args.path, proxies[proxy_id],
                6789 + proxy_id, args.output))
    finally:
        t1 = Timer(args.time, kill_proxies, nodes)
        t2 = Timer(args.time, kill_client, nodes)
        t3 = Timer(args.time, kill_learners, learners)
        t1.start()
        t2.start()
        t3.start()
        if args.software:
            t4 = Timer(args.time, kill_sw_acceptor, acceptors)
            t5 = Timer(args.time, kill_sw_coordinator, ['node97'])
            t4.start()
            t5.start()

        for i in range(args.time/2):
            for n in nodes:
                run_ps(n, args.output)
            time.sleep(1)
