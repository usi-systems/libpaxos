#!/usr/bin/env python
import argparse
import subprocess
import shlex
import os
from threading import Timer
import time


def client(host, path, server_addr, port, output_dir):
    cmd = "ssh danghu@{0} {1}/client_caans {2} {3}".format(host, path, server_addr, port)
    print cmd
    with open('%s/client.txt' % output_dir, 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh

def proxy(host, path, config, proxy_id, output_dir):
    cmd = "ssh danghu@{0} {1}/proxy_caans {2} {3}".format(host, path, config, proxy_id)
    print cmd
    with open('%s/proxy.txt' % output_dir, 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh


def server(host, path, config, output_dir):
    cmd = "ssh danghu@{0} {1}/server_caans {2}".format(host, path, config)
    print cmd
    with open('%s/server.txt' % output_dir, 'w') as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout=out,
                                stderr=out,
                                shell=False)
    return ssh


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
    parser.add_argument('output', help='directory')
    args = parser.parse_args()

    if not os.path.exists(args.output):
       os.makedirs(args.output)

    args.path   = "/home/danghu/workspace/libpaxos/bin/caans"
    args.config = "/home/danghu/workspace/libpaxos/bin/exp.conf"

    pipes = []
    nodes = {'server' : 'node95', 'proxy' : 'node97', 'client' : 'node97' }

    pipes.append(server(nodes['server'], args.path, args.config, args.output))
    pipes.append(proxy(nodes['proxy'], args.path, args.config, 0, args.output))
    pipes.append(client(nodes['client'], args.path, nodes['proxy'], 6789, args.output))

    t= Timer(args.time, kill_all, pipes, nodes)
    t.start()

    for p in pipes:
        p.wait()