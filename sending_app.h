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


// Application that generates flow_bytes of data at CBR app_rate
// and sends to remote using TCP. If flow_bytes is 0, then sends
// data forever until StopApplication() is called. A PacketSink
// must be installed on remote node first.

class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();

  void Setup (uint32_t flow_id,  uint32_t flow_bytes, Ptr<Node> dest, Ptr<Node> source, double flow_start, DataRate app_rate, uint32_t packet_size);
  
  virtual void StartApplication (void);
  virtual void StopApplication (void);
 private:

  void SendPacket (void);

  // Fixed (f_), once initialized in Setup
  Address         f_local;
  Address         f_remote;
  uint16_t        f_dest_port;
  uint32_t        f_packet_size = -1;
  DataRate        f_app_rate = -1;
  uint32_t        f_flow_bytes = -1;
  Ptr<Node>       f_source_node;
  Ptr<Node>       f_dest_node;
  uint32_t        f_flow_id = -1;
  double          f_flow_start = -1;

  // there's only at most one send_event at any time  
  EventId         send_packet_event;
  EventId         start_application_event;
    
  bool            setup_done = false;
  bool            start_application_done = false;
  bool            stop_application_done = false;

  Ptr<Socket>     tcp_socket;
  uint32_t        bytes_sent = 0;
};
