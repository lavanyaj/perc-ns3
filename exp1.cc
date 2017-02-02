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
#include <ns3/node.h>
#include "sending_app.h"

using namespace ns3; // Just Like That?

// Global variables
const uint32_t max_flows = 1000;
Ptr<MyApp> sending_apps[max_flows+1];

// Set up one 1-packet flow between two nodes to start immediately
setUpTraffic()
{
  Ptr<Node> source_node = g_toy_server_nodes.Get(0);
  Ptr<Node> dest_node = g_toy_server_nodes.Get(1);

  Ptr<MyApp> app = CreateObject<MyApp>();
  uint32_t flow_id = 0;
  uint32_t flow_bytes = 1440;
  double flow_start = -1;
  app->Setup(flow_id, flow_bytes, dest_node, source_node, flow_start,
	     DataRate(link_rate_string), packetSize);
  source_node->AddApplication(SendingApp);
  sending_apps[0] = app;
  
}

// Two server nodes directly connected to each other
void
createTopology()
{
  g_toy_server_nodes.Create(2);

  // Install network stacks on the nodes
  InternetStackHelper internet;
  internet.Install (g_toy_server_nodes);

  // Channel, Queue Properties
  PointToPointHelper link_helper;
  link_helper.SetDeviceAttribute ("DataRate", StringValue (g_link_rate_string));
  link_helper.SetChannelAttribute ("Delay",
				  TimeValue(MicroSeconds(g_link_delay_us)));
  std::vector<NetDeviceContainer> links;
  links.push_back(link_helper.Install(g_toy_server_nodes.Get(0),
				     g_toy_server_nodes.Get(1)));

  // TODO: configure queues and generate names for all links
  std::vector<Ipv4InterfaceContainer> interfaces (links.size());
  int index = 0;
  for (const auto& link : links)
    interfaces.push_back(assignAddress(link, index++));

  // Initialize next available port (for applications to send to)
  // on all servers.
  g_server_next_port = new uint16_t [g_toy_server_nodes.GetN()];
  for (uint32_t i = 0; i < g_toy_server_nodes.GetN(), i++)
    g_server_next_port[i] = 1;

  //Turn on global static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
}

int
main (int argc, char *argv[])
{
  createTopology();  
  setupTraffic();

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}
