/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
n2 -- n0 -- n1 -- n3
      
             
*/
// - CBR Traffic goes from the star "arms" to the "hub"




#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/prio-queue.h"
#include <ns3/node.h>
#include "declarations.h"
#include "sending_app.h"

#define SOME_LARGE_VALUE 1250000000

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("pfabric");
// Global variables
uint32_t global_flowid = 100;
const uint32_t max_flows = 10;
uint32_t flow_started[max_flows];
Ptr<MyApp> sending_apps[max_flows];
uint32_t num_flows;

// bottleNeckNode, allNodes, sourceNodes etc. defined in init_all.cc


// Function starts a flow from sourceN to sinkN and returns pointer to sendingApp
Ptr<MyApp> startFlow(uint32_t sourceN, uint32_t sinkN, double flow_start, uint32_t flow_size, uint32_t flow_id)
{
  assert(sourceN < sourceNodes.GetN());
  assert(sinkN < sinkNodes.GetN());
  uint32_t source_id = sourceNodes.Get(sourceN)->GetId();
  uint32_t sink_id = sinkNodes.Get(sinkN)->GetId();
  
  std::cout << "startFlow, startstopflow.cc: myApp about to startFlow from "
            << sourceNodes.Get(sourceN)->GetId()
            << " to " << sinkNodes.Get(sinkN)->GetId() << ".\n";
  ports[sink_id]++;
  std::cout << "ports[" << sink_id << "]: " << ports[sink_id] << "\n";
  // Socket at the source
  Ptr<Ipv4L3Protocol> sink_node_ipv4 = StaticCast<Ipv4L3Protocol> ((sinkNodes.Get(sinkN))->GetObject<Ipv4> ());
  Ipv4Address remoteIp = sink_node_ipv4->GetAddress (1,0).GetLocal();
  Address remoteAddress = (InetSocketAddress (remoteIp, ports[sink_id]));
  sinkInstallNode(sourceN, sinkN, ports[sink_id], flow_id, flow_start, flow_size, 1);

  // Get source address
  Ptr<Ipv4> source_node_ipv4 = (sourceNodes.Get(sourceN))->GetObject<Ipv4> (); 
  Ipv4Address sourceIp = source_node_ipv4->GetAddress (1,0).GetLocal();
  Address sourceAddress = (InetSocketAddress (sourceIp, ports[sink_id]));

  Ptr<MyApp> SendingApp = CreateObject<MyApp> ();
  SendingApp->Setup (remoteAddress, pkt_size, DataRate (link_rate_string), flow_size, flow_start, sourceAddress, sourceNodes.Get(sourceN), flow_id, sinkNodes.Get(sinkN), 1, 1, -1, 1);
  // calling Setup ( .. uint32_t tcp, uint32_t fknown, double stop_time=-1, uint32_t w=1);
  (sourceNodes.Get(sourceN))->AddApplication(SendingApp);
      
  Ptr<Ipv4L3Protocol> ipv4 = StaticCast<Ipv4L3Protocol> ((sourceNodes.Get(sourceN))->GetObject<Ipv4> ()); // Get Ipv4 instance of the node
  Ipv4Address addr = ipv4->GetAddress (1, 0).GetLocal();

  (source_flow[(sourceNodes.Get(sourceN))->GetId()]).push_back(flow_id);
  (dest_flow[(sinkNodes.Get(sinkN))->GetId()]).push_back(flow_id);
  std::stringstream ss;
  ss<<addr<<":"<<remoteIp<<":"<<ports[sink_id];
  std::string s = ss.str(); 
  flowids[s] = flow_id;

  ipv4->setFlow(s, flow_id, flow_size, 1.0);
  sink_node_ipv4->setFlow(s, flow_id, flow_size, 1.0);
  std::cout<<"startFlow, startstopflow.cc: FLOW_INFO source_node "<<(sourceNodes.Get(sourceN))->GetId()<<" sink_node "<<(sinkNodes.Get(sinkN))->GetId()<<" "<<addr<<":"<<remoteIp<<" flow_id "<<flow_id<<" start_time "<<flow_start<<" dest_port "<<ports[sink_id]<<" flow_size "<<flow_size<<" "<<std::endl;
  //flow_id++;
  return SendingApp;
}

  


// Function to configure link connecting nid to neighbor (must do for uplink and downlink)
void config_queue(Ptr<Queue> Q, uint32_t nid, uint32_t vpackets, std::string fkey1)
{
      Q->SetNodeID(nid);
      Q->SetLinkIDString(fkey1);
      Q->SetVPkts(vpackets);
}

// Function to create topology
void createTopology()
{


  //NodeContainer bottleNeckNode;
  bottleNeckNode.Create (2); // two nodes for one bottleneck link
  // Not sure if a node can be both source and sink node
  clientNodes.Create (2);

  std::cout << "create topology with " << clientNodes.GetN()
            << " client nodes and " << bottleNeckNode.GetN()
            << " bottleneck nodes.\n";
  //NodeContainer
  allNodes = NodeContainer (bottleNeckNode, clientNodes);
  
  N = allNodes.GetN();

  // Install network stacks on the nodes
  InternetStackHelper internet;
  internet.Install (allNodes);

  // We create the channels first without any IP addressing information
  //
  // Queue, Channel and link characteristics
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2paccess;
  p2paccess.SetDeviceAttribute ("DataRate", StringValue (link_rate_string));
  p2paccess.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(link_delay)));
  p2paccess.SetQueue("ns3::PrioQueue", "pFabric", StringValue("1"), "DataRate", StringValue(link_rate_string));

  PointToPointHelper p2pbottleneck;
  p2pbottleneck.SetDeviceAttribute ("DataRate", StringValue (link_rate_string));
  p2pbottleneck.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(link_delay)));
  p2pbottleneck.SetQueue("ns3::PrioQueue", "pFabric", StringValue("1"),"DataRate", StringValue(link_rate_string));



  std::vector<NetDeviceContainer> bdevice;
  std::vector<NetDeviceContainer> access;

  // Bottleneck links L0 <-> L1
  bdevice.push_back(p2pbottleneck.Install(bottleNeckNode.Get(0), bottleNeckNode.Get(1)));
  printlink(bottleNeckNode.Get(0), bottleNeckNode.Get(1));
  // Attach other nodes to bottleneck links
  access.push_back(p2paccess.Install(bottleNeckNode.Get(0), clientNodes.Get(0)));
  printlink (bottleNeckNode.Get(0), clientNodes.Get(0));
  access.push_back(p2paccess.Install(bottleNeckNode.Get(1), clientNodes.Get(1)));
  printlink( bottleNeckNode.Get(1), clientNodes.Get(1));
  //access.push_back(p2paccess.Install(bottleNeckNode.Get(1), clientNodes.Get(2)));
  //printlink( bottleNeckNode.Get(1), clientNodes.Get(2));

  std::vector<Ipv4InterfaceContainer> bAdj (bdevice.size());
  std::vector<Ipv4InterfaceContainer> aAdj (access.size());

  uint32_t cur_subnet = 0;

  for(uint32_t i=0; i < bdevice.size(); ++i)
  {
    // set it as switch
    Ptr<PointToPointNetDevice> nd = StaticCast<PointToPointNetDevice> ((bdevice[i]).Get(0));
    Ptr<Queue> queue = nd->GetQueue ();
    uint32_t nid = (nd->GetNode())->GetId(); 
    Ptr<PointToPointNetDevice> nd1 = StaticCast<PointToPointNetDevice> ((bdevice[i]).Get(1));
    Ptr<Queue> queue1 = nd1->GetQueue ();
    uint32_t nid1 = (nd1->GetNode())->GetId(); 

     // get the string version of names of the queues 
     std::stringstream ss;
     ss<<nid<<"_"<<nid<<"_"<<nid1;
     std::string fkey1 = ss.str(); 

     std::cout<<"fkey1 "<<fkey1<<std::endl;

     std::stringstream ss1;
     ss1<<nid1<<"_"<<nid<<"_"<<nid1;
     std::string fkey2 = ss1.str(); 
     std::cout<<"fkey2 "<<fkey2<<std::endl;

     config_queue(queue, nid, vpackets, fkey1);
     config_queue(queue1, nid1, vpackets, fkey2);

     // Checking up link and downlink on L0 - L1
     //Simulator::Schedule (Seconds (1.0), &CheckQueueSize, queue);
     //Simulator::Schedule (Seconds (1.0), &CheckQueueSize, queue1);
    // assign ip address    
     bAdj[i] = assignAddress(bdevice[i], cur_subnet);
     cur_subnet++;
  }

  for(uint32_t i=0; i < access.size(); ++i)
  {
    // set it as switch
    Ptr<PointToPointNetDevice> nd = StaticCast<PointToPointNetDevice> ((access[i]).Get(0));
    Ptr<Queue> queue = nd->GetQueue ();
    uint32_t nid = (nd->GetNode())->GetId(); 
    Ptr<PointToPointNetDevice> nd1 = StaticCast<PointToPointNetDevice> ((access[i]).Get(1));
    Ptr<Queue> queue1 = nd1->GetQueue ();
    uint32_t nid1 = (nd1->GetNode())->GetId(); 

     // get the string version of names of the queues 
     std::stringstream ss;
     ss<<nid<<"_"<<nid<<"_"<<nid1;
     std::string fkey1 = ss.str(); 

     std::cout<<"fkey1 "<<fkey1<<std::endl;

     std::stringstream ss1;
     ss1<<nid1<<"_"<<nid<<"_"<<nid1;
     std::string fkey2 = ss1.str(); 
     std::cout<<"fkey2 "<<fkey2<<std::endl;

     // Configuring up link and downlink on C0 - L0 etc.
     config_queue(queue, nid, vpackets, fkey1);
     config_queue(queue1, nid1, vpackets, fkey2);
  }

  // differen subnet for every link??
  for(uint32_t i=0; i < access.size(); ++i)
  {
    aAdj[i] = assignAddress(access[i], cur_subnet);
    cur_subnet++;
  }

  std::cout << " setting up 1 port  variable for each of " << clientNodes.GetN() << " client nodes";
  ports = new uint16_t [clientNodes.GetN()];
  for (uint32_t i=0; i <clientNodes.GetN(); i++) {
    ports[i] = 1;
   }
  
  //Turn on global static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

}

// Function to set up source and sink nodes for traffic and start all flows
void setUpTraffic()      
{

  // max_flows must match up with this array below
  static const uint32_t source_arr[] = {0, 0,0,0,0,0,0,0,0,0};
  static const uint32_t sink_arr[] = {1,1,1,1,1,1,1,1,1,1};
  size_t flow_arr_size = sizeof(source_arr)/sizeof(uint32_t);
  assert(flow_arr_size == max_flows);
  std::cout << "setupTraffic for " << flow_arr_size << " flows";
  
  std::cout << "clientNodes has " << clientNodes.GetN() << " nodes.\n";
  std::cout << "sourceNodes has " << sourceNodes.GetN() << " nodes.\n";
  std::cout << "sinkNodes has " << sinkNodes.GetN() << " nodes.\n";
  std::cout << "num_flows is " << num_flows << "\n";
  // assert(clientNodes.GetN() > max_flows);

  // Can two flows start from same source node? Then sourceNode just reference?
  for ( unsigned int i = 0; i < max_flows; i++ )
  {
    std::cout << " adding pointer to clientNodes # " << source_arr[i] << " to sourceNodes\n";
    std::cout << " adding pointer to clientNodes # " << sink_arr[i] << " to sinkNodes\n";
    
    sourceNodes.Add(clientNodes.Get(source_arr[i]));
    sinkNodes.Add(clientNodes.Get(sink_arr[i]));
  }
  std::cout << "clientNodes has " << clientNodes.GetN() << " nodes.\n";
  std::cout << "sourceNodes has " << sourceNodes.GetN() << " nodes.\n";
  std::cout << "sinkNodes has " << sinkNodes.GetN() << " nodes.\n";
  std::cout << "num_flows is " << num_flows << "\n";

  for (unsigned int i = 0; i < max_flows; i++) {
    
    uint32_t source_node =  (sourceNodes.Get(i))->GetId();
    // TODO: which is true?
    // THIS ONE IS TRUE // assert(source_arr[i] == source_node);
    // NOT THIS // assert(i == source_node);
    uint32_t sink_node = (sinkNodes.Get(i))->GetId();
    double flow_size = 1446 * 10; //SOME_LARGE_VALUE;
    double flow_start = Simulator::Now().GetSeconds(); // + (i * 10e-6);

    std::cout << " sendingApp is about to start flow from source node " << source_node
              << " to sink node " << sink_node << " of size " << flow_size << " at time now "
              << flow_start << "\n";

    // functions use sourceN and sinkN to index into source_node and sink_node resepctively
    // so using i, instead of clientNodes index ??
    // For some reason flow 1 never stops?
    Ptr<MyApp> sendingApp = startFlow(i, i, flow_start,
                                      flow_size, global_flowid);
    sending_apps[i] = sendingApp;

    global_flowid++;
    flow_started[i] = 1;
    num_flows++;
    std::cout<<Simulator::Now().GetSeconds()<<" starting flow "<<i<<std::endl;
  }
  assert(num_flows == max_flows);

}

// Function main
int 
main (int argc, char *argv[])
{
  CommandLine cmd = addCmdOptions();
  cmd.Parse (argc, argv);
  common_config(); 
  std::cout << "createTopology\n";
  createTopology();
  std::cout << "setUpMonitoring\n";
  setUpMonitoring();
  std::cout << "setUpTraffic\n";
  setUpTraffic();


  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}

// called each time an epoch completes, in this case we do nothing
// in the convergence time epxeriments, new flows are added/ removed
void startflowwrapper( std::vector<uint32_t> sourcenodes, std::vector<uint32_t> sinknodes) {};
