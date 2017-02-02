#include "sending_app.h"
#include <assert.h>

#define SENDING_APP_CC_PACKET_SIZE 1500
#define SENDING_APP_CC_BITS_PER_BYTE 8

MyApp::MyApp () {};

MyApp::~MyApp()
{
  // Just Like That?
  f_socket = 0;
}

// Sets up source and destination node to send, receive packets
// fills per-flow variables in source and dest nodes and in global
void
MyApp::Setup (uint32_t flow_id,  uint32_t flow_bytes, Ptr<Node> dest_node, Ptr<Node> source_node, double flow_start, DataRate app_rate,  uint32_t packet_size)
{
  assert(!setup_done);
  setup_done = true;

  f_flow_id = flow_id; f_flow_bytes = flow_bytes; f_dest_node = dest_node;
  f_source_node = source_node; f_flow_start = flow_start; f_app_rate = app_rate;
  f_remote = remote; f_local = local; f_packet_size = packet_size;

  assert(f_flow_id > 0);
  assert(f_flow_bytes > 0 || f_flow_bytes == -1);
  assert(f_packet_size > 0);

  // Address remote, Address local;
  uint32_t source_node_id = source_node->GetId();
  uint32_t dest_node_id = dest_node->GetId();
  f_dest_port = ports[dest_node_id]++;
  Ipv4Address remote_ipv4_addr = dest_node->Getaddress(1,0).GetLocal();
  f_remote = InetSocketAddress(remote_ipv4_addr, f_dest_port);
  Ipv4Address local_ipv4_addr = source_node->Getaddress(1,0).GetLocal();
  f_local = InetSocketAddress(local_ipv4_addr, f_dest_port); // TODO: check port?
  
  // Get destination node ready to receive packets from sending app

  // TODO
  // Fill in per-flow state in IPv4 Object at source, dest, for Rate Limiting
    Ptr<Ipv4L3Protocol> source_ipv4 = StaticCast<Ipv4L3Protocol>
      (source_node->GetObject<Ipv4> ());
    Ptr<Ipv4L3Protocol> dest_ipv4 = StaticCast<Ipv4L3Protocol>
      (dest_node->GetObject<Ipv4> ());
    
  // Fill in global variables that track if a node is source/ dest for a flow
  g_source_flow[source_node_id].push_back(f_flow_id);
  g_dest_flow[dest_node_id].push_back(f_flow_id);
  
  if (f_flow_start == -1) {
    StartApplication();
  } else {
    start_application_event =
      Simulator::Schedule (Time(Seconds(f_flow_start)),
			   &MyApp::StartApplication, this);
  }
}

void
MyApp::StartApplication (void)
{
  assert(setup_done);
  assert(!start_application_event.IsRunning());
  assert(!start_application_done);
  start_application_done = true;  

  assert(f_flow_start == -1 ||
	 Simulator::Now() >= Time(Seconds(f_flow_start)));
  assert(bytes_sent == 0);

  // initialize socket
  tcp_socket = Socket::CreateSocket (f_source_node,
				     TcpSocketFactory::GetTypeId ());
  tcp_socket->Bind ();
  tcp_socket->Connect(f_remote);
  // TODO: log socket connection info
  SendPacket();
}

void
MyApp::StopApplication (void)
{
  assert(start_application_done);
  assert(!stop_application_done);
  assert(!start_application_event.IsRunning());
  
  assert(tcp_socket);
  if (send_packet_event.IsRunning()) Simulator::Cancel(send_packet_event);
  tcp_socket->Close();
}

void
MyApp::SendPacket (void)
{

  assert(!send_packet_event.IsRunning());
  assert(f_app_rate > 0);    
  assert(f_packet_size > 0);  

  // Last packet size might be smaller than default
  uint32_t tmp = f_packet_size;
  if (f_flow_bytes > 0 and bytes_sent + tmp > f_flow_bytes)
    tmp = f_flow_bytes - bytes_sent;

  // Send Packet
  Ptr<Packet> packet = Create<Packet> ();
  assert(tcp_socket);
  assert (tcp_socket->Send(packet) != -1);
  bytes_sent += packet->GetSize();
  
  // Stop if flow_bytes was given and we have no more bytes to sent
  // Otherwise re-schedule according to app_rate
  if (f_flow_bytes > 0 and bytes_sent >= f_flow_bytes) {
    assert(bytes_sent == f_flow_bytes);
    StopApplication();
  } else {    
    Time next_send_packet_time
      (Seconds (SENDING_APP_CC_APP_PACKET_SIZE * SENDING_APP_CC_BITS_PER_BYTE /
		static_cast<double> (f_app_rate.GetBitRate ())));
    send_packet_event =
      Simulator::Schedule(next_send_packet_time, &MyApp::SendPacket, this);    
  }
}
