#!/usr/bin/env python

from twisted.internet import protocol, reactor
import datetime

class EchoClient(protocol.Protocol):

    def __init__(self,):
        self.count = 0

    def connectionMade(self):
        self.sendTime()

    def sendTime(self):
        if self.count > 10:
            self.transport.loseConnection()
            return
        now = datetime.datetime.now()
        data = now.strftime("%y %m %d %H:%M:%S.%f")
        self.transport.write(data)
        print "Send %d" % self.count
        self.count  += 1


    def dataReceived(self, data):
        end_time = datetime.datetime.now()
        print "received %r" % (data)
        # self.transport.write(data, (host, port))
        start_time = end_time
        start_time = start_time.strptime(data, ("%y %m %d %H:%M:%S.%f"))
        # print start_time, end_time
        dur =  end_time - start_time
        # self.f.write('%2.6f\n' % dur.total_seconds())
        self.sendTime()

    def connectionLost(self, reason):
        print "connection lost"

class EchoFactory(protocol.ClientFactory):
    protocol = EchoClient

    def clientConnectionFailed(self, connector, reason):
        print "Connection failed - goodbye!"

    def clientConnectionLost(self, connector, reason):
        print "Connection lost - goodbye!"

# this connects the protocol to a server running on port 8000
def main():
    f = EchoFactory()
    reactor.connectTCP("192.168.1.110", 6789, f)
    reactor.run()

# this only runs if the module was *not* imported
if __name__ == '__main__':
    main()