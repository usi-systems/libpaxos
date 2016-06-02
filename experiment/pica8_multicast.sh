#!/bin/sh
ovs-vsctl del-br br0
ovs-vsctl add-br br0 -- set bridge br0 datapath_type=pica8
ovs-vsctl add-port br0 te-1/1/2  -- set interface te-1/1/2 type=pica8
ovs-vsctl add-port br0 te-1/1/4  -- set interface te-1/1/4 type=pica8
ovs-vsctl add-port br0 te-1/1/5  -- set interface te-1/1/5 type=pica8
ovs-vsctl add-port br0 te-1/1/7  -- set interface te-1/1/7 type=pica8
ovs-vsctl add-port br0 te-1/1/9  -- set interface te-1/1/9 type=pica8
ovs-vsctl add-port br0 te-1/1/10  -- set interface te-1/1/10 type=pica8
ovs-vsctl add-port br0 te-1/1/13  -- set interface te-1/1/13 type=pica8
ovs-vsctl add-port br0 te-1/1/14  -- set interface te-1/1/14 type=pica8
ovs-vsctl add-port br0 te-1/1/15  -- set interface te-1/1/15 type=pica8
ovs-vsctl add-port br0 te-1/1/16  -- set interface te-1/1/16 type=pica8
ovs-vsctl add-port br0 te-1/1/23  -- set interface te-1/1/23 type=pica8
ovs-vsctl add-port br0 te-1/1/24  -- set interface te-1/1/24 type=pica8
ovs-ofctl add-group br0 group_id=2,type=all,bucket=output:5,bucket=output:13,bucket=output:14
ovs-ofctl add-group br0 group_id=3,type=all,bucket=output:9,bucket=output:23,bucket=output:24
ovs-ofctl add-flow br0 priority=10,in_port=10,udp,nw_dst=224.3.29.73,udp_dst=34952,action=output:4
ovs-ofctl add-flow br0 priority=10,in_port=24,udp,nw_dst=224.3.29.73,udp_dst=34952,action=output:4
ovs-ofctl add-flow br0 priority=10,in_port=23,udp,nw_dst=224.3.29.73,udp_dst=34952,action=output:4
ovs-ofctl add-flow br0 priority=10,in_port=2,udp,nw_dst=224.3.29.73,udp_dst=34952,action=group:2
ovs-ofctl add-flow br0 priority=10,in_port=7,udp,nw_dst=224.3.29.73,udp_dst=34952,action=group:3
ovs-ofctl add-flow br0 priority=10,in_port=15,udp,nw_dst=224.3.29.73,udp_dst=34952,action=group:3
ovs-ofctl add-flow br0 priority=10,in_port=16,udp,nw_dst=224.3.29.73,udp_dst=34952,action=group:3
ovs-ofctl add-flow br0 priority=2,ip,nw_dst=192.168.4.95,action=output:9
ovs-ofctl add-flow br0 priority=2,ip,nw_dst=192.168.4.96,action=output:23
ovs-ofctl add-flow br0 priority=2,ip,nw_dst=192.168.4.97,action=output:10
ovs-ofctl add-flow br0 priority=2,ip,nw_dst=192.168.4.98,action=output:24
