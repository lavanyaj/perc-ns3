import sys
import random
#import matplotlib.pyplot as plt
import numpy as np
import time
#import mpmath

###################### Global constants ########################
num_instances = 200000
max_iterations = 400
max_capacity = 100
nflows_per_link = 100
nswitches = 20
np.random.seed(241)
random.seed(241)
#mpmath.dps = 100
np.set_printoptions(precision=200)
################################################################

class MPMaxMin:
    def __init__(self, routes, c):
        
        ########################## Inputs ##############################        
        (self.num_flows, self.num_links) = routes.shape
        self.routes = routes  # incidence matrix for flows / links
        self.c = c            # link capacities
        ################################################################
        # max-min rates
        self.maxmin_level = -1
        self.maxmin_x = self.water_filling()
        # iteration
        self.t = 0                                            
        self.stable_links = np.zeros((self.num_links,1), dtype=float)
        self.stable_flows = np.zeros((self.num_flows,1), dtype=float)
        self.rates = np.ones((self.num_links,1), dtype=float) * max_capacity
        self.demands = np.ones((self.num_flows,1), dtype=float) * max_capacity
        
                      
    def water_filling(self): 
        weights = np.ones((self.num_flows, 1), dtype=float)
        x = np.zeros((self.num_flows,1), dtype=float)
        rem_flows = np.array(range(self.num_flows))
        rem_cap = np.array(self.c, copy=True)
        level = 0
        while rem_flows.size != 0:
            level += 1
            link_weights = self.routes.T.dot(weights)
            with np.errstate(divide='ignore', invalid='ignore'):
                bl = np.argmax(np.where(link_weights>0.0, link_weights/rem_cap, -1))
            inc = rem_cap[bl]/link_weights[bl]
            x[rem_flows] = x[rem_flows] + inc*weights[rem_flows]                
            rem_cap = rem_cap - inc*link_weights
            rem_cap = np.where(rem_cap>0.0, rem_cap, 0.0)       
            bf = np.nonzero(self.routes[:,bl])[0]
            rem_bf = np.array([f for f in rem_flows if f in bf])
            ##print "level ", level, " bottleneck link is ", bl,\
            ##    ", bottleneck flow has rate ", x[rem_bf[0]]

            rem_flows = np.array([f for f in rem_flows if f not in bf])
            weights[bf] = 0
        self.maxmin_level = level
        ##print("finished waterfilling")
        return x
                                                                           
    def step(self):
        self.t += 1
        
        ##print '$$$$$$$$ t = ', self.t
        #print 'flow_to_link_demands='
        #print self.demands.T
        self.update_link_to_flow_rates()
        #print 'link_to_flow_rates='
        #print self.rates.T
        self.update_flow_to_link_demands()
        self.update_link_to_flow_stable()
        self.update_flow_to_link_stable()
        #self.print_details()        



    def update_link_to_flow_rates(self):
        for l in range(self.num_links):
            flow_indices = np.nonzero(self.routes[:,l])[0]
            stable_flows = [f for f in flow_indices if self.stable_flows[f] == 1]
            nflows = len(flow_indices)
            # only look at demands of stable flows
            demands = self.demands[stable_flows]
            if nflows == 0:
                continue
            sumsat = np.sum(np.where(demands<self.rates[l], demands, 0))
            numsat = np.sum(np.where(demands<self.rates[l], 1, 0))
            #print "sumsat " + str(sumsat)
            #print "numsat " + str(numsat)
            #print "maxsat " + str(maxsat)
            assert(numsat < nflows)
            self.rates[l] = (self.c[l]-sumsat)/(nflows - numsat)
            #print "rate of link ", l, " is ", self.rates[l]
                    
    def update_flow_to_link_demands(self):
        for f in range(self.num_flows):
            link_indices = np.nonzero(self.routes[f,:])[0]
            self.demands[f] = min(self.rates[link_indices])
            #print "demand of flow ", f, " is ", self.demands[f]

    def update_link_to_flow_stable(self):        
        for l in range(self.num_links):
            flow_indices = np.nonzero(self.routes[:,l])[0]
            unstable_flows = [f for f in flow_indices if self.stable_flows[f] == 0]
            if len(unstable_flows) == 0:
                self.stable_links[l] = 1
            else:
                min_unstable_demand = min(self.demands[unstable_flows])
                if min_unstable_demand >= self.rates[l]:
                    self.stable_links[l] = 1

    def update_flow_to_link_stable(self):
        for l in range(self.num_links):
            if self.stable_links[l] == 1:
                flow_indices = np.nonzero(self.routes[:,l])[0]
                unstable_flows = [f for f in flow_indices if self.stable_flows[f] == 0]
                newly_stable_flows = len(unstable_flows)
                if newly_stable_flows > 0:                    
                    self.stable_flows[unstable_flows] = 1
                    ##print newly_stable_flows, " got newly stable at ", self.rates[l], " cuz of link ", l                    

    def fair_share(self, demands, cap):
        demands = sorted(demands)
        demands.append(max_capacity)
        nflows = len(demands)
        level = 0.0
        mycap = float(cap)
        for f in range(nflows):
            rem_share = mycap / (nflows - f)
            inc = np.min([rem_share, demands[f]])
            level += inc
            mycap -= inc*(nflows-f)
            demands = demands - inc
        return level
                                        
    def print_details(self):
        ##print 'iteration=', self.t
        ##print 'demands=', self.demands.T
        #print 'maxmin=', self.maxmin_x.T
        print 'l2 error=', np.linalg.norm(self.demands - self.maxmin_x)
        print 'linf error=', max(np.abs(self.demands - self.maxmin_x))
        #print 'x.shape', self.x.shape
        print 'max(demands)', max(self.demands)
        print 'max(maxmin)', max(self.maxmin_x)
        print 'min(demands)', min(self.demands)
        print 'min(maxmin)', min(self.maxmin_x)

def gen_random_instance(nswitches, nflows_per_link, safe=True):
    nlinks = nswitches * (nswitches - 1) 
    nflows = nflows_per_link * (nswitches - 1) * 2
    # nflows * path_length = flows_per_link * nlinks

    if (safe):
        A = np.zeros((nflows+nlinks, nlinks), dtype=float)
        for j in range(nlinks):
            flow_index = nflows+j
            A[flow_index, j] = 1
    else:
        A = np.zeros((nflows, nlinks), dtype=float)

    #A = np.zeros((nflows, nlinks))
        
    for i in range(nflows):        
        path_length = random.randint(2, nswitches)
        # lo and hi included
        #print "path length is " + str(path_length)
        #print "nswitches is " + str(nswitches)
        path = np.random.choice(nswitches, path_length, replace=False)
        #print path
        for k in range(path_length-1):
            s1 = path[k]
            s2 = path[k+1]

            j = s1 * (nswitches-1) + s2
            if (s2 > s1):
                # shift down by one cuz we
                # don't have index links from s1 to s1
                j -= 1
            
           # print "link from " + str(s1) + " to " + str(s2) + " is " + str(j)
            A[i, j] = 1

    nflows_by_link = np.sum(A, axis=0)
    #print nflows_by_link
    used_links = np.where(nflows_by_link>0.0)[0]
    #print used_links
    num_used_links = len(used_links)

    ##print "nflows=", nflows
    ##print "original num_links", nlinks
    ##print "num used links", num_used_links

    # If A is sparse, many links don't have flows
    # make a new matrix B with only used links
    # all used links have at least one flow
    # we also want to add a one link only flow
    # for each link
    # if (safe):
    #     B = np.zeros((nflows+num_used_links, num_used_links))
    #     for j in range(num_used_links):
    #         flow_index = nflows+j
    #         B[flow_index, j] = 1
    #         old_index = used_links[j]
    #         for i in range(nflows):
    #             B[i, j] = A[i, old_index]
    # else:
    #     B = np.zeros((nflows, num_used_links))
    #     for j in range(num_used_links):
    #         old_index = used_links[j]
    #         B[:, j] = A[:, old_index]


    # print "shape of original matrix ", A.shape()
    # print "shape of new matrix ", B.shape()
    # A = B
    
    c = np.ones((num_used_links, 1), dtype=float)
    return A,c

def gen_random_bipartite_instance(nports, nflows):
    A = np.zeros((nflows, 2*nports))
    for i in range(nflows):
        src = np.random.randint(nports)
        dst = np.random.randint(nports)
        A[i, src] = 1
        A[i, nports+dst] = 1
    c = np.ones((2*nports, 1))
    return A,c

def gen_large_chain(nports):
    nflows = nports*(nports+1)/2
    A = np.zeros((nflows, 2*nports))
    f = 0
    for j in range(nports):
        for i in range(j,nports):
            A[f,i] = 1
            A[f,nports+j] = 1
            f += 1
    c = np.ones((2*nports, 1))
    return A,c

def main1():
    np.random.seed(241)
    plt.close("all")
    
    print("10 steps of max min for random_instance(5)")
    A,c = gen_random_instance(nswitches=3, nflows_per_link=1)
    print A
    print c
    wf_maxmin = MPMaxMin(A, c)
    print wf_maxmin.maxmin_x
    print wf_maxmin.maxmin_level
    for i in range(10):
        wf_maxmin.step()    
    #error5 = wf_maxmin.errors
    
def main2():
    #f = open("results.txt", "w")
    #A = np.array([[1,0],
    #              [1,1],
    #              [1,0],
    #              [0,1]])       
    #c = np.array([[1.0, 1.0]]).T
    ##print max_iterations, " steps of max min for ", num_instances,\
    ##    " random instance with ", nflows_per_link,\
    ##    " flows/ link, ", nswitches, " switches"
    for i in range(num_instances):
        
        A,c = gen_random_instance(nswitches=nswitches,\
                                  nflows_per_link=nflows_per_link, safe=True)
        if i < 2:
            continue
        wf_maxmin = MPMaxMin(A, c)
        steps_to_converge = -1
        start_time = time.time()
        for j in range(1, max_iterations+1):
            wf_maxmin.step()
            linf_err = max(np.abs(wf_maxmin.demands - wf_maxmin.maxmin_x))
            l2_err = np.linalg.norm(wf_maxmin.demands - wf_maxmin.maxmin_x)
            ##if j%10 == 0 or linf_err < 1e-8:
            ##    print "instance ", i, "step ", j, ", linf_err ",\
            ##        linf_err, ", l2_err ", l2_err,\
            ##        ", elapsed time", (time.time()-start_time)
            sys.stdout.flush()
            if linf_err < 1e-10:
                steps_to_converge = j
                break
                
        if (steps_to_converge > 0):
            #f.write("instance %d converged after %d steps.\n"%(i, steps_to_converge))
            print "instance ", i, "converged after ", steps_to_converge, " steps."
        else:
            #f.write("instance %d did not converge after %d steps.\n"%(i, max_iterations))
            print "instance ", i, "did not converge after ", max_iterations, " steps."
        wf_maxmin.print_details()
    #f.close()   
                    
if __name__ == '__main__':
    main()

