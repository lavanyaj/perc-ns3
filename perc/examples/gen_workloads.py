import numpy as np
from mp_cpg_lop import *
# start_flows
# stop_flows
# stop_flows
# 1 0
# 1 1
# 1 2
# 2 0
# 3 1
# 0 1000 3 1000
# 1 1000 3 1001
# 2 1000 3 1002
# 1 0 0.9
# 1 1 0.9
# 1 2 0.9
# 2 1 1.35
# 2 2 1.35
# 3 2 2.7

# Workload to emulate consecutive incasts, some overlapping.
# End hosts numbered from 0 through 143. Ports from 1000 to 65535.
class GenIncasts:
    def __init__(self, prefix, min_degree, max_degree):
        self.prefix = prefix
        self.min_degree = min_degree
        self.max_degree = max_degree
        print "output to ", prefix, ", incasts with degree between ", min_degree, " and ", max_degree
        self.flows_file = open("%s/flows.txt"%self.prefix, "w")
        self.flow_arrivals_file = open("%s/flow_arrivals.txt"%self.prefix, "w")
        self.flow_departures_file = open("%s/flow_departures.txt"%self.prefix, "w")
        self.events_file = open("%s/events.txt"%self.prefix, "w")
        self.opt_rates_file = open("%s/opt_rates.txt"%self.prefix, "w")

        self.num_hosts = 144
        self.min_port = 1000
        self.max_port = 65535
        self.ports = {}
        return

    # pfabric topology has 144 * 2 + 36 * 2 interfaces
    # first 144 have capacity edge data rate (e.g., 10 Gb/s)
    # last 72 have capacity fabric data rate (e.g., 40 Gb/s)
    # For a given set of flows, not all links are used,
    #  that's okay.
    def generate_empty_instance(self, nflows):
        nswitches = 144 * 2 + 36 * 2
        A = np.zeros((nflows, nswitches))
        c = np.ones((nswitches, 1))
        for j in range(288, nswitches):
            c[j,0] = 4
        # capacities scaled down from 10 and 40 to 1 and 4
        return A,c

    def generate_incast_instance(self):
        # pick number of flows
        # pick nflows hosts at random
        # for each flow figure out path (tricky w ecmp)
        # also need this to compute rates
        # in ns3 fixed route for each src-dst interface pai
        nhosts = 144
        nflows = np.random.randint(self.min_degree,self.max_degree+1)
        A,c = self.generate_empty_instance(nflows)
        # for host number h, h * 2 is the NIC
        hosts = np.random.choice(nhosts, nflows+1, replace=False) * 2
        dst = hosts[-1]
        srcs = hosts[:-1]
        #print nflows, " random flows from hosts", str(hosts/2)
        # maybe for now assume fabric not bn to calculate rates?
        for i in range(nflows):
            #print "setting A[", i, ",", srcs[i], "] to 1"
            A[i, srcs[i]] = 1
            A[i, dst] = 1
            #print "non-zero links are ", str(np.nonzero(A[i,:])[0])
            #print A[i,:]
            
        #print A
        return A,c

    def run(self, num_epochs):
        nflows = 0
        epoch = 1
        # only one incast at a time
        for i in range(num_epochs):            
            self.events_file.write("start_flows\n")
            A,c = self.generate_incast_instance()
            #print "incast traffic matrix", A
            
            nflows_prev = nflows
            nflows += A.shape[0]
            wf_maxmin = MPMaxMin(A,c)
            self.write_flows_to_file(nflows_prev, A)
            self.write_flow_arrivals(epoch, nflows_prev, A)
            self.write_opt_rates(epoch, nflows_prev, wf_maxmin.maxmin_x)
            epoch += 1
            self.events_file.write("stop_flows_flows\n")
            self.write_flow_departures(epoch, nflows_prev, A.shape[0])
            epoch += 1
        return

    def write_flows_to_file(self, start_index, A):
        nflows, nswitches = A.shape
        for i in range(nflows):            
            hosts = np.nonzero(A[i,:])[0]
            flow = start_index+i
            src = hosts[0]/2
            dst = hosts[1]/2
            if src not in self.ports:
                self.ports[src] = 1000
            if dst not in self.ports:
                self.ports[dst] = 1000
            assert(self.ports[src] < 65536)
            assert(self.ports[dst] < 65536)
            src_port = self.ports[src]
            dst_port = self.ports[dst]
            self.ports[dst] += 1
            self.ports[src] += 1
            self.flows_file.write("%d %d %d %d\n" %
                                  (src, src_port, dst, dst_port))
        return


    def write_flow_arrivals(self, epoch, start_index, A):        
        nflows, nswitches = A.shape
        print nflows, " incast flows in epoch ", epoch
        for i in range(nflows):
            flow = start_index+i
            self.flow_arrivals_file.write("%d %d\n"%(epoch, flow))
        return

    def write_opt_rates(self, epoch, start_index, opt_rates):
        nflows = opt_rates.shape[0]
        for i in range(nflows):
            flow = start_index+i
            rate = opt_rates[i] * 10
            self.opt_rates_file.write("%d %d %f\n"%(epoch, flow, rate))
        return
    
    def write_flow_departures(self, epoch, start_index, nflows):
        for i in range(nflows):
            flow = start_index+i
            self.flow_departures_file.write("%d %d\n"%(epoch, flow))
        return

def main():
    gi = GenIncasts("/home/lavanyaj/ns-3-allinone/ns-3-dev/src/perc/examples/workload2",\
                    min_degree=int(sys.argv[1]), max_degree=int(sys.argv[1]))
    gi.run(1)
    return

main()
