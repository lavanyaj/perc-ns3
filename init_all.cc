#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

const std::string g_link_rate_string = "10Gbps";
const double g_link_delay_us = 2.0; //in microseconds

NodeContainer g_toy_server_nodes, g_toy_switch_nodes;

uint16_t* g_server_next_port = NULL;

// assignAddress
