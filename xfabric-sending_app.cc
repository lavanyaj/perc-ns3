#include "sending_app.h"
#include "declarations.h"

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0),
    m_maxBytes (0)
{
  m_totBytes = 0;
  
}

MyApp::~MyApp ()
{
  m_socket = 0;
}

uint32_t
MyApp::getFlowId(void)
{
  return m_fid;
}

void
MyApp::ChangeRate (DataRate passed_in_rate)
{
  std::cout<<"ChangeRate called flow "<<m_fid<<" rate "<<passed_in_rate<<std::endl;
  m_dataRate = passed_in_rate; 
}

//MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate, Ptr<RandomVariableStream> interArrival)
//MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate, uint32_t maxBytes, double start_time, Address ownaddress, Ptr<Node> sNode)
  
void 
MyApp::Setup (Address address, uint32_t packetSize, DataRate dataRate, uint32_t maxbytes, double flow_start, Address address1, Ptr<Node> pnode, uint32_t fid, Ptr<Node> dnode, uint32_t w)
{
  std::cout << "sending_app.cc: Setup (.. line 42)\n";
  Setup(address, packetSize, dataRate, maxbytes, flow_start, address1, pnode, fid, dnode, 1,1,-1,w);
}

void
MyApp::Setup (Address address, uint32_t packetSize, DataRate dataRate, uint32_t maxBytes, double start_time, Address ownaddress, Ptr<Node> sNode, uint32_t fid, Ptr<Node> dNode, uint32_t tcp, uint32_t fknown, double stop_time, uint32_t weight)
{
  std::cout << "sending_app.cc: Setup (.. line 49) for flow " << fid
	    << ", dataRate " << dataRate
	    << ", stop_time " << stop_time << " \n";
  //m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_dataRate = dataRate;
  m_totBytes = 0;
  m_maxBytes = maxBytes; // assuming we are getting this in bits
  m_startTime = start_time;
  m_fid = fid;

  if(tcp == 0) {
    m_udp = 1;
  } else {
    m_udp = 0;
  }
  flow_known = fknown;
  
 Time tNext = Time(Seconds(m_startTime));
  myAddress = ownaddress;
  srcNode = sNode;
  destNode = dNode;

  m_stoptime = stop_time;
  m_weight = weight;

  //NS_LOG_UNCOND("Scheduling start of flow "<<fid<<" at time "<<Time(tNext).GetSeconds());
 // m_startEvent = Simulator::Schedule (tNext, &MyApp::StartApplication, this); //bug fix 1/14
  if(m_maxBytes == 0) {
    // static case
    std::cout << "Setup, sending_app.cc: calling StartApplication() for static case\n";
    StartApplication(); //static case
  }  else {
    std::cout << "Setup, sending_app.cc: scheduling StartApplication for dynamic case\n";
    m_startEvent = Simulator::Schedule (tNext, &MyApp::StartApplication, this); //dynamic case
  }
}

void
MyApp::StartApplication (void)
{

  if(Simulator::Now().GetNanoSeconds() < Time(Seconds(m_startTime)).GetNanoSeconds()) {
    std::cout<<"Time "<<Simulator::Now().GetNanoSeconds()<<" spurious call flowid "<<m_fid<<" returning before start_time "<<  Time(Seconds(m_startTime)).GetNanoSeconds()<<std::endl;
    if(Simulator::IsExpired(m_startEvent)) {
      Time tNext = Time(Seconds(m_startTime));
      m_startEvent = Simulator::Schedule (tNext, &MyApp::StartApplication, this);
      std::cout<<"Time "<<Simulator::Now().GetSeconds()<<" spurious call flowid "<<m_fid<<" rescheduling at  "<<tNext.GetSeconds()<<std::endl;
      
    }
      
    return;

  } 

  std::cout<<"StartApplication for fid "<<m_fid<<" called at "<<Simulator::Now().GetSeconds()<<std::endl; 
  m_running = true;
  m_packetsSent = 0;
  m_totBytes = 0;

  Ptr<Socket> ns3TcpSocket;
  if(m_udp) {
    ns3TcpSocket = Socket::CreateSocket (srcNode, UdpSocketFactory::GetTypeId());
  } else {
    ns3TcpSocket = Socket::CreateSocket (srcNode, TcpSocketFactory::GetTypeId ());
    Ptr<TcpNewReno> nReno = StaticCast<TcpNewReno> (ns3TcpSocket);
  }
  //setuptracing(m_fid, ns3TcpSocket);
  m_socket = ns3TcpSocket;
  if (InetSocketAddress::IsMatchingType (m_peer))
  { 
    //NS_LOG_UNCOND("flow_start "<<m_fid<<" time "<<(Simulator::Now()).GetSeconds());
    //m_socket->Bind (myAddress);
    m_socket->Bind ();
  }
  else
  {
    m_socket->Bind6 ();
  }
  m_socket->Connect (m_peer);
    

  uint16_t local_port = StaticCast<TcpSocketBase>(ns3TcpSocket)->m_endPoint->GetLocalPort();

  uint32_t tcp_protocol_number = 6;

  std::cout<<InetSocketAddress::ConvertFrom(myAddress).GetIpv4().Get() <<" "<< InetSocketAddress::ConvertFrom(m_peer).GetIpv4().Get() <<" "<< InetSocketAddress::ConvertFrom(m_peer).GetPort() <<" "<<  InetSocketAddress::ConvertFrom(myAddress).GetPort() <<" "<< tcp_protocol_number <<" ecmp_arguments"<<std::endl;
 
  uint32_t tuplevalue = InetSocketAddress::ConvertFrom(myAddress).GetIpv4().Get() + InetSocketAddress::ConvertFrom(m_peer).GetIpv4().Get() + InetSocketAddress::ConvertFrom(m_peer).GetPort() + local_port + tcp_protocol_number;


  uint32_t ecmp_hash_value = Ipv4GlobalRouting::ecmp_hash(tuplevalue);
//  std::cout<<" fid "<<m_fid<<" ecmp_hash "<<ecmp_hash_value<<std::endl;

//std::cout<<"flow_start "<<m_fid<<" start_time "<<Simulator::Now().GetNanoSeconds()<<" flow_size "<<m_maxBytes<<" "<<srcNode->GetId()<<" "<<destNode->GetId()<<" port "<< InetSocketAddress::ConvertFrom (m_peer).GetPort () <<" "<<m_weight<<" "<<ecmp_hash_value<<" "<<std::endl;
  std::cout<<"sending_app.cc: flow_start "<<m_fid<<" start_time "<<Simulator::Now().GetNanoSeconds()<<" flow_size "<<m_maxBytes<<" "<<srcNode->GetId()<<" "<<destNode->GetId() <<" "<<m_weight<<" "<<ecmp_hash_value<<" "<<std::endl;
  
  SendPacket ();
  //FlowData dt(m_fid, m_maxBytes, flow_known, srcNode->GetId(), destNode->GetId(), fweight);
  //flowTracker->registerEvent(1);
}

void
MyApp::StopApplication (void)
{
  std::cout << "sending_app.cc MyApp::StopApplication\n";
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }

  // Note: this statement is here and works for the case when we are starting and stopping 
  // same flows 
  // When we have dynamic traffic, the stop has to be declared from the destination
  // So, remove it when using dynamic traffic
//   std::cout<<"flow_stop "<<m_fid<<" start_time "<<Simulator::Now().GetNanoSeconds()<<" flow_size "<<m_maxBytes<<" "<<srcNode->GetId()<<" "<<destNode->GetId() <<" "<<m_weight<<" "<<ecmp_hash_value<<" "<<std::endl;
  std::cout<<"sending_app.cc: flow_stop "<<m_fid<<" "<<srcNode->GetId()<<" "<<destNode->GetId()<<" at "<<(Simulator::Now()).GetNanoSeconds()<<" "<<m_maxBytes<<" port "<< InetSocketAddress::ConvertFrom (m_peer).GetPort () <<" weight "<<m_weight<<std::endl;
//  Ptr<Ipv4L3Protocol> ipv4 = StaticCast<Ipv4L3Protocol> (srcNode->GetObject<Ipv4> ());
//  ipv4->addToDropList(m_fid);

  std::cout<<Simulator::Now().GetSeconds()<<" flowid "<<m_fid<<" stopped sending after sending "<<m_totBytes<<std::endl;
}

bool
MyApp::keepSending(void)
{
  if(m_stoptime != -1 && (Simulator::Now().GetSeconds() >= m_stoptime)) {
    std::cout<<" stoptime reached "<<m_stoptime<<std::endl;
    return false;
  }
  if(m_maxBytes != 0 && (m_totBytes >= m_maxBytes)) {
    return false;
  } 

  return true;
}
  

void
MyApp::SendPacket (void)
{
  uint32_t pktsize = m_packetSize;
  // std::cout << "MyApp::SendPacket for flow_id " << m_fid
  // 	    << ", bytes sent, " <<  m_totBytes
  // 	    << ", m_maxBytes was: " << m_maxBytes << "\n";
  if(m_maxBytes > 0) {
    uint32_t bytes_remaining = m_maxBytes - m_totBytes;
    if(bytes_remaining < m_packetSize) {
      pktsize = bytes_remaining;
    }
  }
  
  Ptr<Packet> packet = Create<Packet> (pktsize);

  int ret_val = m_socket->Send( packet ); 
  std::cout<<"***  "<<Simulator::Now().GetSeconds()<<" sent packet with id "<<packet->GetUid()<<" size "<<packet->GetSize()<<" flowid "<<m_fid<<" source "<<srcNode->GetId()<<" destNode "<<destNode->GetId()<<" myaddress "<<myAddress<<" peeraddress "<<m_peer<<" *** "<<std::endl;  
  if(ret_val != -1) {
    m_totBytes += packet->GetSize();
  } else {
    std::cout << "couldn't send, socket buffer full.\n";
    // couldn't send - socket buffer full
  }

  //if (m_totBytes < m_maxBytes)
  //  {
    if(keepSending()) {
      ScheduleTx ();
    } else if(m_maxBytes == 0) {
      // for a finite flow, we close the socket from the peer side
      std::cout << "myApp::SendPacket for flow " << m_fid
		<< " calling StopApplication after sending " << m_packetsSent << "\n";
      StopApplication();
    } else {
      std::cout << "keepSending returned false for flow " << m_fid
		<< ", m_maxBytes is " << m_maxBytes
		<< " and m_totBytes is " << m_totBytes << "\n";
      StopApplication();
    }
  
}

void
MyApp::ScheduleTx (void)
{
  // std::cout << "MyApp::ScheduleTx() for "
  // 	    << " flow "<< m_fid
  // 	    << ", has sent bytes " << m_totBytes << "\n";
  //if (m_running)
  if ((m_maxBytes == 0) || (m_totBytes < m_maxBytes))
    {
      //Time tNext (Seconds ((m_packetSize+38) * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      Time tNext (Seconds (1500 * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
      std::cout << "MyApp::ScheduleTx() for " << " flow "<< m_fid
       		<< " scheduling next SendPacket for rate " << m_dataRate << "\n";
    } else {
    std::cout << "myApp ScheduleTx for flow " << m_fid
	      << " calling StopApplication after sending " << m_packetsSent << "\n";
      StopApplication();

      //StopApplication();
    }
}
