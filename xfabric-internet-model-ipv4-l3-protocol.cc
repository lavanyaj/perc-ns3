// -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*-
//
// Copyright (c) 2006 Georgia Tech Research Corporation
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Author: George F. Riley<riley@ece.gatech.edu>
//

#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/callback.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/net-device.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/object-vector.h"
#include "ns3/ipv4-header.h"
#include "ns3/boolean.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "tcp-header.h"

#include "loopback-net-device.h"
#include "arp-l3-protocol.h"
#include "ipv4-l3-protocol.h"
#include "icmpv4-l4-protocol.h"
#include "ipv4-interface.h"
#include "ipv4-raw-socket-impl.h"
#include "prio-header.h"
#include "ns3/flow_utils.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/queue.h"
#include<string>


NS_LOG_COMPONENT_DEFINE ("Ipv4L3Protocol");

namespace ns3 {

const uint16_t Ipv4L3Protocol::PROT_NUMBER = 0x0800;
//static const uint32_t MSEC_WINDOW = 10000;
double MAX_DOUBLE = 10.0;

NS_OBJECT_ENSURE_REGISTERED (Ipv4L3Protocol);

TypeId 
Ipv4L3Protocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Ipv4L3Protocol")
    .SetParent<Ipv4> ()
    .AddConstructor<Ipv4L3Protocol> ()
    .AddAttribute("alpha_fair_rcp",
                  "Enable or disable alpha fair based like behavior",
                  BooleanValue(true),
                  MakeBooleanAccessor (&Ipv4L3Protocol::alpha_fair_rcp),
                  MakeBooleanChecker ())
    .AddAttribute("rate_based",
                  "Enable or disable rate based like behavior",
                  BooleanValue(false),
                  MakeBooleanAccessor (&Ipv4L3Protocol::rate_based),
                  MakeBooleanChecker ())
    .AddAttribute("host_compensate",
                  "Enable or disable host based compensation for network price",
                  BooleanValue(false),
                  MakeBooleanAccessor (&Ipv4L3Protocol::host_compensate),
                  MakeBooleanChecker ())
    .AddAttribute("wfq_testing",
                  "Enable or disable wfq like behavior",
                  BooleanValue(false),
                  MakeBooleanAccessor (&Ipv4L3Protocol::m_wfq),
                  MakeBooleanChecker ())
    .AddAttribute("m_pfabric",
                  "Enable or disable pfabric like behavior",
                  BooleanValue(false),
                  MakeBooleanAccessor (&Ipv4L3Protocol::m_pfabric),
                  MakeBooleanChecker ())
    .AddAttribute("m_pkt_tag",
                  "Enable or disable wfq like behavior",
                  BooleanValue(false),
                  MakeBooleanAccessor (&Ipv4L3Protocol::m_pkt_tag),
                  MakeBooleanChecker ())
    .AddAttribute ("DefaultTos", "The TOS value set by default on all outgoing packets generated on this node.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Ipv4L3Protocol::m_defaultTos),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("UtilFunction", "UtilFunction",
                   UintegerValue (1),
                   MakeUintegerAccessor (&Ipv4L3Protocol::m_method),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("DefaultTtl", "The TTL value set by default on all outgoing packets generated on this node.",
                   UintegerValue (64),
                   MakeUintegerAccessor (&Ipv4L3Protocol::m_defaultTtl),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("FragmentExpirationTimeout",
                   "When this timeout expires, the fragments will be cleared from the buffer.",
                   TimeValue (Seconds (30)),
                   MakeTimeAccessor (&Ipv4L3Protocol::m_fragmentExpirationTimeout),
                   MakeTimeChecker ())
    .AddTraceSource ("Tx", "Send ipv4 packet to outgoing interface.",
                     MakeTraceSourceAccessor (&Ipv4L3Protocol::m_txTrace))
    .AddTraceSource ("Rx", "Receive ipv4 packet from incoming interface.",
                     MakeTraceSourceAccessor (&Ipv4L3Protocol::m_rxTrace))
    .AddTraceSource ("Drop", "Drop ipv4 packet",
                     MakeTraceSourceAccessor (&Ipv4L3Protocol::m_dropTrace))
    .AddAttribute ("InterfaceList", "The set of Ipv4 interfaces associated to this Ipv4 stack.",
                   ObjectVectorValue (),
                   MakeObjectVectorAccessor (&Ipv4L3Protocol::m_interfaces),
                   MakeObjectVectorChecker<Ipv4Interface> ())

    .AddTraceSource ("SendOutgoing", "A newly-generated packet by this node is about to be queued for transmission",
                     MakeTraceSourceAccessor (&Ipv4L3Protocol::m_sendOutgoingTrace))
    .AddTraceSource ("UnicastForward", "A unicast IPv4 packet was received by this node and is being forwarded to another node",
                     MakeTraceSourceAccessor (&Ipv4L3Protocol::m_unicastForwardTrace))
    .AddTraceSource ("LocalDeliver", "An IPv4 packet was received by/for this node, and it is being forward up the stack",
                     MakeTraceSourceAccessor (&Ipv4L3Protocol::m_localDeliverTrace))

  ;
  return tid;
}

double Ipv4L3Protocol::SetTargetRateDGD(double current_netw_price, std::string flowkey)
{
   double link_rate = line_rate;
   double limit_tr = 1.0;
   double target_rate = limit_tr* link_rate;
   if(current_netw_price > 0.0) {
      if(m_method == 1) {
        target_rate = utilInverse(flowkey, current_netw_price, LOGUTILITY);
      } else if(m_method == 2) {
        target_rate = utilInverse(flowkey, current_netw_price, FCTUTILITY);
      } else if(m_method == 3) {
        target_rate = utilInverse(flowkey, current_netw_price, ALPHA1UTILITY);
      }
    }
  
  flow_target_rate[flowkey] = target_rate;
  if(alpha_fair_rcp) {
//	  flow_target_rate[flowkey] = 1.0/current_netw_price;
    flow_target_rate[flowkey] = pow(current_netw_price, -1.0/my_fct_alpha);
//    std::cout<<" alpha hrere "<<my_fct_alpha<<std::endl;
    
	  //flow_target_rate[flowkey] = current_netw_price;
    if(flow_target_rate[flowkey]*1000000.0 > line_rate) {
		  flow_target_rate[flowkey] = line_rate/1000000.0;
	  }
   }
  
   if(flow_target_rate[flowkey] < 120.0) {
      flow_target_rate[flowkey] = 120.0;
     }

  //std::cout<<Simulator::Now().GetSeconds()<<" flow "<<flowkey<<" price "<<current_netw_price<<" rate "<<flow_target_rate[flowkey]<<std::endl;

  return target_rate;
}

double Ipv4L3Protocol::SetFlowRtt(double rtt, std::string fkey)
{
	flow_rtt[fkey] = rtt;
	//std::cout<<" RTTUPDATE Node "<<m_node->GetId()<<" flow "<<fkey<<" time "<<Simulator::Now().GetSeconds()<<" rtt "<<rtt<<std::endl;
}


void Ipv4L3Protocol::updateRate(std::string fkey)
{
  
//  double deq_rate = deq_bytes * 8.0/(1000000.0 * QUERY_TIME);
  deq_bytes = 0;
  
  double instant_rate = totalbytes[fkey] * 8.0 /(1000000.0 *QUERY_TIME); // rate is in Mbps
  double cur_rate = (1 - alpha) * store_rate[fkey] + alpha * instant_rate;

  double dest_instant_rate = destination_bytes[fkey] * 8.0 /(1000000.0 *QUERY_TIME); // rate is in Mbps
  double dest_cur_rate = (1 - alpha) * store_dest_rate[fkey] + alpha * dest_instant_rate;

  /* re-init the totalbytes to zero */
  totalbytes[fkey] = 0;
  store_rate[fkey] = cur_rate;

  destination_bytes[fkey] = 0;
  store_dest_rate[fkey] = dest_cur_rate;

//  NS_LOG_LOGIC("updating rate "<<fkey<<" "<<Simulator::Now().GetSeconds()<<" drate "<<dest_cur_rate<<" srate "<<cur_rate<<" node "<<m_node->GetId()<<" "<<Simulator::Now().GetMicroSeconds());

}


void 
Ipv4L3Protocol::updateAllRates(void)
{
  std::map<std::string,uint32_t>::iterator it;
  for (std::map<std::string,uint32_t>::iterator it=flowids.begin(); it!=flowids.end(); ++it) 
  {
    updateRate(it->first);
  }
  Simulator::Schedule(Seconds (QUERY_TIME), &ns3::Ipv4L3Protocol::updateAllRates, this);
}


void
Ipv4L3Protocol::printSumThr(void)
{
  std::map<std::string,uint32_t>::iterator it;
  for (std::map<std::string,uint32_t>::iterator it=flowids.begin(); it!=flowids.end(); ++it) 
  {
    NS_LOG_LOGIC("TotalRecvd Node "<<m_node->GetId()<<" "<<flowids[it->first]<<" "<<data_recvd[it->first]);
  }
}
   
void
Ipv4L3Protocol::setSimTime(double sim_time)
{
  Simulator::Schedule( Seconds(sim_time), &ns3::Ipv4L3Protocol::printSumThr, this);
  NS_LOG_LOGIC(" setSimTime set to "<<sim_time);
}
 
void 
Ipv4L3Protocol::setFlowIdealRate(uint32_t fid, double rate)
{
  NS_LOG_LOGIC("node "<<m_node->GetId()<<" setting ideal rate "<<rate<<" for flow "<<fid);
  flow_idealrate[fid] = rate;
}
 
Ipv4L3Protocol::Ipv4L3Protocol()
{
  NS_LOG_FUNCTION (this);
  previousRate = 0.0;
  QUERY_TIME = 0.0001;
  alpha = 1.0/16.0;
    
  long_ewma_const = 50000;
  short_ewma_const = 10000; // KANTHI - TEST - REMOVE
  next_deadline = 0.0;
  last_deadline = 0.0;
  
  line_rate = 10000; //ticking bug. Replace it with config parameter

//  Simulator::Schedule(Seconds (QUERY_TIME), &ns3::Ipv4L3Protocol::updateAllRates, this);
  //Simulator::Schedule(Seconds (epoch_update_time), &ns3::Ipv4L3Protocol::updateCurrentEpoch, this);
//  Simulator::Schedule(Seconds (1.0), &ns3::Ipv4L3Protocol::updateAllRates, this);
//  Simulator::Schedule(Seconds (1.0), &ns3::Ipv4L3Protocol::updateCurrentEpoch, this);
//  Simulator::Schedule(Seconds (1.0), &ns3::Ipv4L3Protocol::CheckToSend, this);
  bytes_in_queue = 0;
}

void Ipv4L3Protocol::CheckToSend(std::string flowkey)
{

    if(our_packets[flowkey].empty()) {
//      std::cout<<"Node "<<m_node->GetId()<<" IP Q empty flow "<<flowkey<<" "<<Simulator::Now().GetSeconds()<<std::endl;
      return;
    }
    Ptr<Packet> p = (our_packets[flowkey]).front();
    (our_packets[flowkey]).pop();
    bytes_in_queue -= p->GetSize();

    Ipv4Address s = (src_address[flowkey]).front();
    (src_address[flowkey]).pop();
  
    Ipv4Address d = (dst_address[flowkey]).front();
    (dst_address[flowkey]).pop();

    uint8_t prot = (prot_q[flowkey]).front();
    (prot_q[flowkey]).pop();

    Ptr<Ipv4Route> r = (route_q[flowkey]).front();
    (route_q[flowkey]).pop();

    NS_LOG_LOGIC("Calling DoSend "<<p->GetUid());
  
    DoSend(p, s, d, prot, r);

    TcpHeader tcph;
    p->PeekHeader(tcph);

    double trate = flow_target_rate[flowkey]; // flow_target_rate is in Mbps
    uint32_t tcphsize = tcph.GetSerializedSize();
    if((p->GetSize() - tcphsize) == 0) {
        // it's an ack
        trate = line_rate;
    }
    
    if(trate == 0.0) {
      /* should not happen.. a known flow must have a rate assigned */
//      std::cout<<"ERROR "<<Simulator::Now().GetSeconds()<<" "<<m_node->GetId()<<" flow "<<flowkey<<" target rate is zero"<<std::endl;
      trate = line_rate; // line rate for acks
//      std::cout<<"node "<<m_node->GetId()<<" line_rate "<<line_rate<<" for flow "<<flowkey<<std::endl;
    }
//    std::cout<<" psize "<<p->GetSize()<<" tcp header size "<<tcphsize<<std::endl;
    double pkt_dur = ((p->GetSize() + 46) * 8.0 * 1000.0) /trate;  //in us since target_rate is in bps
   
    Time tNext (NanoSeconds (pkt_dur));
    m_sendEvent[flowkey] = Simulator::Schedule (tNext, &Ipv4L3Protocol::CheckToSend, this, flowkey);
}
  
void
Ipv4L3Protocol::QueueWithUs (Ptr<Packet> packet, 
                      Ipv4Address source,
                      Ipv4Address destination,
                      uint8_t protocol,
                      Ptr<Ipv4Route> route)
{
  NS_LOG_LOGIC("QueueWithUs pktid "<<packet->GetUid());
  TcpHeader tcph;
  packet->PeekHeader(tcph);
  uint16_t destPort = tcph.GetDestinationPort();
  std::stringstream ss;
  ss <<source<<":"<<destination<<":"<<destPort;
  std::string flowkey = ss.str(); 

  our_packets[flowkey].push(packet);
  src_address[flowkey].push(source);
  dst_address[flowkey].push(destination);
  prot_q[flowkey].push(protocol);
  route_q[flowkey].push(route);

  bytes_in_queue += packet->GetSize();


  if(m_sendEvent.find(flowkey) == m_sendEvent.end()) {
    m_sendEvent[flowkey] = EventId();
  }

  if(m_sendEvent[flowkey].IsRunning()) {
    /* nothing to do.. it will send when it can */
  } else {
    //Simulator::ScheduleNow(&Ipv4L3Protocol::CheckToSend, this, flowkey); kanthi testing alpha_fair
    CheckToSend(flowkey);
  }
  deq_bytes += packet->GetSize() + 46;
  
}




void Ipv4L3Protocol::setQueryTime(double qtime)
{
  QUERY_TIME = qtime;
} 

void Ipv4L3Protocol::setKay(double kvalue)
{
  // set default values - hardcoded for older code
  //
  setlong_ewma_const(50000);
  setshort_ewma_const(10000);
}

void Ipv4L3Protocol::setmeasurement_ewma_const(double kvalue)
{
//  std::cout<<"Setting long_ewma_const to "<<kvalue<<std::endl;
  measurement_ewma_const = kvalue;
}
void Ipv4L3Protocol::setlong_ewma_const(double kvalue)
{
//  std::cout<<"Setting long_ewma_const to "<<kvalue<<std::endl;
  long_ewma_const = kvalue;
}

void Ipv4L3Protocol::setshort_ewma_const(double kvalue)
{
//  std::cout<<"Setting short_ewma_const to "<<kvalue<<std::endl;
  short_ewma_const = kvalue;
}

void Ipv4L3Protocol::setLineRate(double d)
{
  line_rate = d;
  std::cout<<"Setlinerate to "<<d<<" node "<<m_node->GetId()<<std::endl;
}

void Ipv4L3Protocol::setAlpha(double qtime)
{
  alpha = qtime;
} 

void Ipv4L3Protocol::setMPTCP(bool mptcp)
{
   m_mptcp = mptcp;
}

void Ipv4L3Protocol::setEpochUpdate(double qtime)
{
  epoch_update_time = qtime;
  current_epoch = 1;
} 

Ipv4L3Protocol::~Ipv4L3Protocol ()
{
  NS_LOG_FUNCTION (this);
}

void
Ipv4L3Protocol::Insert (Ptr<IpL4Protocol> protocol)
{
  NS_LOG_FUNCTION (this << protocol);
  m_protocols.push_back (protocol);
}
Ptr<IpL4Protocol>
Ipv4L3Protocol::GetProtocol (int protocolNumber) const
{
  NS_LOG_FUNCTION (this << protocolNumber);
  for (L4List_t::const_iterator i = m_protocols.begin (); i != m_protocols.end (); ++i)
    {
      if ((*i)->GetProtocolNumber () == protocolNumber)
        {
          return *i;
        }
    }
  return 0;
}
void
Ipv4L3Protocol::Remove (Ptr<IpL4Protocol> protocol)
{
  NS_LOG_FUNCTION (this << protocol);
  m_protocols.remove (protocol);
}

void
Ipv4L3Protocol::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this << node);
  m_node = node;
  // Add a LoopbackNetDevice if needed, and an Ipv4Interface on top of it
  SetupLoopback ();
}

Ptr<Node>
Ipv4L3Protocol::GetNode(void)
{
  return m_node;
}

Ptr<Socket> 
Ipv4L3Protocol::CreateRawSocket (void)
{
  NS_LOG_FUNCTION (this);
  Ptr<Ipv4RawSocketImpl> socket = CreateObject<Ipv4RawSocketImpl> ();
  socket->SetNode (m_node);
  m_sockets.push_back (socket);
  return socket;
}
void 
Ipv4L3Protocol::DeleteRawSocket (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  for (SocketList::iterator i = m_sockets.begin (); i != m_sockets.end (); ++i)
    {
      if ((*i) == socket)
        {
          m_sockets.erase (i);
          return;
        }
    }
  return;
}
/*
 * This method is called by AddAgregate and completes the aggregation
 * by setting the node in the ipv4 stack
 */
void
Ipv4L3Protocol::NotifyNewAggregate ()
{
  NS_LOG_FUNCTION (this);
  if (m_node == 0)
    {
      Ptr<Node>node = this->GetObject<Node>();
      // verify that it's a valid node and that
      // the node has not been set before
      if (node != 0)
        {
          this->SetNode (node);
        }
    }
  Object::NotifyNewAggregate ();
}

void 
Ipv4L3Protocol::SetRoutingProtocol (Ptr<Ipv4RoutingProtocol> routingProtocol)
{
  NS_LOG_FUNCTION (this << routingProtocol);
  m_routingProtocol = routingProtocol;
  m_routingProtocol->SetIpv4 (this);
}


Ptr<Ipv4RoutingProtocol> 
Ipv4L3Protocol::GetRoutingProtocol (void) const
{
  NS_LOG_FUNCTION (this);
  return m_routingProtocol;
}

void 
Ipv4L3Protocol::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  for (L4List_t::iterator i = m_protocols.begin (); i != m_protocols.end (); ++i)
    {
      *i = 0;
    }
  m_protocols.clear ();

  for (Ipv4InterfaceList::iterator i = m_interfaces.begin (); i != m_interfaces.end (); ++i)
    {
      *i = 0;
    }
  m_interfaces.clear ();
  m_sockets.clear ();
  m_node = 0;
  m_routingProtocol = 0;

  for (MapFragments_t::iterator it = m_fragments.begin (); it != m_fragments.end (); it++)
    {
      it->second = 0;
    }

  for (MapFragmentsTimers_t::iterator it = m_fragmentsTimers.begin (); it != m_fragmentsTimers.end (); it++)
    {
      if (it->second.IsRunning ())
        {
          it->second.Cancel ();
        }
    }

  m_fragments.clear ();
  m_fragmentsTimers.clear ();

  Object::DoDispose ();
}

void
Ipv4L3Protocol::SetupLoopback (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<Ipv4Interface> interface = CreateObject<Ipv4Interface> ();
  Ptr<LoopbackNetDevice> device = 0;
  // First check whether an existing LoopbackNetDevice exists on the node
  for (uint32_t i = 0; i < m_node->GetNDevices (); i++)
    {
      if ((device = DynamicCast<LoopbackNetDevice> (m_node->GetDevice (i))))
        {
          break;
        }
    }
  if (device == 0)
    {
      device = CreateObject<LoopbackNetDevice> (); 
      m_node->AddDevice (device);
    }
  interface->SetDevice (device);
  interface->SetNode (m_node);
  Ipv4InterfaceAddress ifaceAddr = Ipv4InterfaceAddress (Ipv4Address::GetLoopback (), Ipv4Mask::GetLoopback ());
  interface->AddAddress (ifaceAddr);
  uint32_t index = AddIpv4Interface (interface);
  Ptr<Node> node = GetObject<Node> ();
  node->RegisterProtocolHandler (MakeCallback (&Ipv4L3Protocol::Receive, this), 
                                 Ipv4L3Protocol::PROT_NUMBER, device);
  interface->SetUp ();
  if (m_routingProtocol != 0)
    {
      m_routingProtocol->NotifyInterfaceUp (index);
    }
}

void 
Ipv4L3Protocol::SetDefaultTtl (uint8_t ttl)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (ttl));
  m_defaultTtl = ttl;
}

uint32_t 
Ipv4L3Protocol::AddInterface (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (this << device);

  Ptr<Node> node = GetObject<Node> ();
  node->RegisterProtocolHandler (MakeCallback (&Ipv4L3Protocol::Receive, this), 
                                 Ipv4L3Protocol::PROT_NUMBER, device);
  node->RegisterProtocolHandler (MakeCallback (&ArpL3Protocol::Receive, PeekPointer (GetObject<ArpL3Protocol> ())),
                                 ArpL3Protocol::PROT_NUMBER, device);

  Ptr<Ipv4Interface> interface = CreateObject<Ipv4Interface> ();
  interface->SetNode (m_node);
  interface->SetDevice (device);
  interface->SetForwarding (m_ipForward);
  return AddIpv4Interface (interface);
}

uint32_t 
Ipv4L3Protocol::AddIpv4Interface (Ptr<Ipv4Interface>interface)
{
  NS_LOG_FUNCTION (this << interface);
  uint32_t index = m_interfaces.size ();
  m_interfaces.push_back (interface);
  return index;
}

Ptr<Ipv4Interface>
Ipv4L3Protocol::GetInterface (uint32_t index) const
{
  NS_LOG_FUNCTION (this << index);
  if (index < m_interfaces.size ())
    {
      return m_interfaces[index];
    }
  return 0;
}

uint32_t 
Ipv4L3Protocol::GetNInterfaces (void) const
{
  NS_LOG_FUNCTION (this);
  return m_interfaces.size ();
}

int32_t 
Ipv4L3Protocol::GetInterfaceForAddress (
  Ipv4Address address) const
{
  NS_LOG_FUNCTION (this << address);
  int32_t interface = 0;
  for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); 
       i != m_interfaces.end (); 
       i++, interface++)
    {
      for (uint32_t j = 0; j < (*i)->GetNAddresses (); j++)
        {
          if ((*i)->GetAddress (j).GetLocal () == address)
            {
              return interface;
            }
        }
    }

  return -1;
}

int32_t 
Ipv4L3Protocol::GetInterfaceForPrefix (
  Ipv4Address address, 
  Ipv4Mask mask) const
{
  NS_LOG_FUNCTION (this << address << mask);
  int32_t interface = 0;
  for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); 
       i != m_interfaces.end (); 
       i++, interface++)
    {
      for (uint32_t j = 0; j < (*i)->GetNAddresses (); j++)
        {
          if ((*i)->GetAddress (j).GetLocal ().CombineMask (mask) == address.CombineMask (mask))
            {
              return interface;
            }
        }
    }

  return -1;
}

int32_t 
Ipv4L3Protocol::GetInterfaceForDevice (
  Ptr<const NetDevice> device) const
{
  NS_LOG_FUNCTION (this << device);
  int32_t interface = 0;
  for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); 
       i != m_interfaces.end (); 
       i++, interface++)
    {
      if ((*i)->GetDevice () == device)
        {
          return interface;
        }
    }

  return -1;
}

bool
Ipv4L3Protocol::IsDestinationAddress (Ipv4Address address, uint32_t iif) const
{
  NS_LOG_FUNCTION (this << address << iif);
  // First check the incoming interface for a unicast address match
  for (uint32_t i = 0; i < GetNAddresses (iif); i++)
    {
      Ipv4InterfaceAddress iaddr = GetAddress (iif, i);
      if (address == iaddr.GetLocal ())
        {
          NS_LOG_LOGIC ("For me (destination " << address << " match)");
          return true;
        }
      if (address == iaddr.GetBroadcast ())
        {
          NS_LOG_LOGIC ("For me (interface broadcast address)");
          return true;
        }
    }

  if (address.IsMulticast ())
    {
#ifdef NOTYET
      if (MulticastCheckGroup (iif, address ))
#endif
      if (true)
        {
          NS_LOG_LOGIC ("For me (Ipv4Addr multicast address");
          return true;
        }
    }

  if (address.IsBroadcast ())
    {
      NS_LOG_LOGIC ("For me (Ipv4Addr broadcast address)");
      return true;
    }

  if (GetWeakEsModel ())  // Check other interfaces
    { 
      for (uint32_t j = 0; j < GetNInterfaces (); j++)
        {
          if (j == uint32_t (iif)) continue;
          for (uint32_t i = 0; i < GetNAddresses (j); i++)
            {
              Ipv4InterfaceAddress iaddr = GetAddress (j, i);
              if (address == iaddr.GetLocal ())
                {
                  NS_LOG_LOGIC ("For me (destination " << address << " match) on another interface");
                  return true;
                }
              //  This is a small corner case:  match another interface's broadcast address
              if (address == iaddr.GetBroadcast ())
                {
                  NS_LOG_LOGIC ("For me (interface broadcast address on another interface)");
                  return true;
                }
            }
        }
    }
  return false;
}

void 
Ipv4L3Protocol::Receive ( Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from,
                          const Address &to, NetDevice::PacketType packetType)
{
  NS_LOG_FUNCTION (this << device << p << protocol << from << to << packetType);

  NS_LOG_LOGIC ("Packet from " << from << " received on node " << 
                m_node->GetId ());


  uint32_t interface = 0;
  Ptr<Packet> packet = p->Copy ();

  Ptr<Ipv4Interface> ipv4Interface;
  for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); 
       i != m_interfaces.end (); 
       i++, interface++)
    {
      ipv4Interface = *i;
      if (ipv4Interface->GetDevice () == device)
        {
          if (ipv4Interface->IsUp ())
            {
              m_rxTrace (packet, m_node->GetObject<Ipv4> (), interface);
              break;
            }
          else
            {
              NS_LOG_LOGIC ("Dropping received packet -- interface is down");
              Ipv4Header ipHeader;
              packet->RemoveHeader (ipHeader);
              m_dropTrace (ipHeader, packet, DROP_INTERFACE_DOWN, m_node->GetObject<Ipv4> (), interface);
              return;
            }
        }
    }

  Ipv4Header ipHeader;
  if (Node::ChecksumEnabled ())
    {
      ipHeader.EnableChecksum ();
    }
  PrioHeader prioheader;              // kanthi header change
  packet->RemoveHeader (prioheader); //kanthi change
  PriHeader preset = prioheader.GetData();
  pre_set_wfq_weight = preset.wfq_weight;
  pre_set_residue = preset.residue;
  pre_set_netw_price = preset.netw_price;


  //NS_LOG_UNCOND("pre_set_priority is "<<pre_set_priority <<" source "<<ipHeader.GetSource()<<" destination "<<ipHeader.GetDestination()<<" at node "<<m_node->GetId()<<" GetData "<<prioheader.GetData());

  packet->RemoveHeader (ipHeader);


  // Trim any residual frame padding from underlying devices
  if (ipHeader.GetPayloadSize () < packet->GetSize ())
    {
      packet->RemoveAtEnd (packet->GetSize () - ipHeader.GetPayloadSize ());
    }

  if (!ipHeader.IsChecksumOk ()) 
    {
      NS_LOG_LOGIC ("Dropping received packet -- checksum not ok");
      m_dropTrace (ipHeader, packet, DROP_BAD_CHECKSUM, m_node->GetObject<Ipv4> (), interface);
      return;
    }

  for (SocketList::iterator i = m_sockets.begin (); i != m_sockets.end (); ++i)
    {
      Ptr<Ipv4RawSocketImpl> socket = *i;
      socket->ForwardUp (packet, ipHeader, ipv4Interface);
    }

  NS_ASSERT_MSG (m_routingProtocol != 0, "Need a routing protocol object to process packets");
  if (!m_routingProtocol->RouteInput (packet, ipHeader, device,
                                      MakeCallback (&Ipv4L3Protocol::IpForward, this),
                                      MakeCallback (&Ipv4L3Protocol::IpMulticastForward, this),
                                      MakeCallback (&Ipv4L3Protocol::LocalDeliver, this),
                                      MakeCallback (&Ipv4L3Protocol::RouteInputError, this)
                                      ))
    {
      NS_LOG_WARN ("No route found for forwarding packet.  Drop.");
      m_dropTrace (ipHeader, packet, DROP_NO_ROUTE, m_node->GetObject<Ipv4> (), interface);
    }
}

Ptr<Icmpv4L4Protocol> 
Ipv4L3Protocol::GetIcmp (void) const
{
  NS_LOG_FUNCTION (this);
  Ptr<IpL4Protocol> prot = GetProtocol (Icmpv4L4Protocol::GetStaticProtocolNumber ());
  if (prot != 0)
    {
      return prot->GetObject<Icmpv4L4Protocol> ();
    }
  else
    {
      return 0;
    }
}

bool
Ipv4L3Protocol::IsUnicast (Ipv4Address ad) const
{
  NS_LOG_FUNCTION (this << ad);

  if (ad.IsBroadcast () || ad.IsMulticast ())
    {
      return false;
    }
  else
    {
      // check for subnet-broadcast
      for (uint32_t ifaceIndex = 0; ifaceIndex < GetNInterfaces (); ifaceIndex++)
        {
          for (uint32_t j = 0; j < GetNAddresses (ifaceIndex); j++)
            {
              Ipv4InterfaceAddress ifAddr = GetAddress (ifaceIndex, j);
              NS_LOG_LOGIC ("Testing address " << ad << " with subnet-directed broadcast " << ifAddr.GetBroadcast () );
              if (ad == ifAddr.GetBroadcast () )
                {
                  return false;
                }
            }
        }
    }

  return true;
}

bool
Ipv4L3Protocol::IsUnicast (Ipv4Address ad, Ipv4Mask interfaceMask) const
{
  NS_LOG_FUNCTION (this << ad << interfaceMask);
  return !ad.IsMulticast () && !ad.IsSubnetDirectedBroadcast (interfaceMask);
}

void 
Ipv4L3Protocol::SendWithHeader (Ptr<Packet> packet, 
                                Ipv4Header ipHeader,
                                Ptr<Ipv4Route> route)
{
  //NS_LOG_FUNCTION (this << packet << ipHeader << route);
  NS_LOG_FUNCTION ("SendWithHeader" << this << packet << ipHeader << route);
  if (Node::ChecksumEnabled ())
    {
      ipHeader.EnableChecksum ();
    }
  SendRealOut (route, packet, ipHeader);
}

bool
Ipv4L3Protocol::known_flow(std::string flowkey)
{
  
  uint32_t fid = flowids[flowkey];
  if (flow_idealrate.find(fid) == flow_idealrate.end()) {
//    NS_LOG_LOGIC("unknown flow "<<flowkey<<" "<<fid<<" not found "<<m_node->GetId());
    return false;
  }
//  NS_LOG_LOGIC("known flow "<<flowkey<<" "<<fid<<" found "<<m_node->GetId());
  return true;
}
   
bool
Ipv4L3Protocol::issource(Ipv4Address source)
{
  if (GetInterfaceForAddress(source) != -1) {
    return true;
  }
  return false;
}
 
   
void 
Ipv4L3Protocol::Send (Ptr<Packet> packet, 
                      Ipv4Address source,
                      Ipv4Address destination,
                      uint8_t protocol,
                      Ptr<Ipv4Route> route)
{
  //NS_LOG_FUNCTION (this << packet << source << destination << uint32_t (protocol) << route);
  NS_LOG_FUNCTION ("Ipv4L3Protocol::Send" << this << packet << source << destination << uint32_t (protocol) << route);
//  NS_LOG_LOGIC("Ipv4L3Protocol::Send" << this << packet << source << destination << uint32_t (protocol) << route<<" packetid "<<packet->GetUid());
  
//  if(m_node->GetId() == 3 || m_node->GetId() == 2) {
//    rate_based = false;
//  }
 
   TcpHeader tcph; 
   packet->PeekHeader(tcph);
   uint16_t destPort = tcph.GetDestinationPort();

   std::stringstream ss;
   ss <<source<<":"<<destination<<":"<<destPort;
   std::string flowkey = ss.str(); 


  if(rate_based && issource(source)) {
    QueueWithUs(packet, source, destination, protocol, route);
  } else {
    DoSend(packet, source, destination, protocol, route);
  }
  return;

}

void
Ipv4L3Protocol::DoSend (Ptr<Packet> packet, 
                      Ipv4Address source,
                      Ipv4Address destination,
                      uint8_t protocol,
                      Ptr<Ipv4Route> route)
{
  NS_LOG_LOGIC("DoSend pktid "<<packet->GetUid());
  Ipv4Header ipHeader;
  bool mayFragment = true;
  uint8_t ttl = m_defaultTtl;
  SocketIpTtlTag tag;
  bool found = packet->RemovePacketTag (tag);
  if (found)
    {
      ttl = tag.GetTtl ();
    }

  uint8_t tos = m_defaultTos;
  SocketIpTosTag ipTosTag;
  found = packet->RemovePacketTag (ipTosTag);
  if (found)
    {
      tos = ipTosTag.GetTos ();
    }

  // Handle a few cases:
  // 1) packet is destined to limited broadcast address
  // 2) packet is destined to a subnet-directed broadcast address
  // 3) packet is not broadcast, and is passed in with a route entry
  // 4) packet is not broadcast, and is passed in with a route entry but route->GetGateway is not set (e.g., on-demand)
  // 5) packet is not broadcast, and route is NULL (e.g., a raw socket call, or ICMP)

  // 1) packet is destined to limited broadcast address or link-local multicast address
  if (destination.IsBroadcast () || destination.IsLocalMulticast ())
    {
      NS_LOG_LOGIC ("Ipv4L3Protocol::Send case 1:  limited broadcast");
      ipHeader = BuildHeader (source, destination, protocol, packet->GetSize (), ttl, tos, mayFragment);
      uint32_t ifaceIndex = 0;
      for (Ipv4InterfaceList::iterator ifaceIter = m_interfaces.begin ();
           ifaceIter != m_interfaces.end (); ifaceIter++, ifaceIndex++)
        {
          Ptr<Ipv4Interface> outInterface = *ifaceIter;
          Ptr<Packet> packetCopy = packet->Copy ();

          NS_ASSERT (packetCopy->GetSize () <= outInterface->GetDevice ()->GetMtu ());

          m_sendOutgoingTrace (ipHeader, packetCopy, ifaceIndex);
          packetCopy->AddHeader (ipHeader);
          m_txTrace (packetCopy, m_node->GetObject<Ipv4> (), ifaceIndex);
          outInterface->Send (packetCopy, destination);
        }
      return;
    }

  // 2) check: packet is destined to a subnet-directed broadcast address
  uint32_t ifaceIndex = 0;
  for (Ipv4InterfaceList::iterator ifaceIter = m_interfaces.begin ();
       ifaceIter != m_interfaces.end (); ifaceIter++, ifaceIndex++)
    {
      Ptr<Ipv4Interface> outInterface = *ifaceIter;
      for (uint32_t j = 0; j < GetNAddresses (ifaceIndex); j++)
        {
          Ipv4InterfaceAddress ifAddr = GetAddress (ifaceIndex, j);
          NS_LOG_LOGIC ("Testing address " << ifAddr.GetLocal () << " with mask " << ifAddr.GetMask ());
          if (destination.IsSubnetDirectedBroadcast (ifAddr.GetMask ()) && 
              destination.CombineMask (ifAddr.GetMask ()) == ifAddr.GetLocal ().CombineMask (ifAddr.GetMask ())   )
            {
              NS_LOG_LOGIC ("Ipv4L3Protocol::Send case 2:  subnet directed bcast to " << ifAddr.GetLocal ());
              ipHeader = BuildHeader (source, destination, protocol, packet->GetSize (), ttl, tos, mayFragment);
              Ptr<Packet> packetCopy = packet->Copy ();
              m_sendOutgoingTrace (ipHeader, packetCopy, ifaceIndex);
              packetCopy->AddHeader (ipHeader);
              m_txTrace (packetCopy, m_node->GetObject<Ipv4> (), ifaceIndex);
              outInterface->Send (packetCopy, destination);
              return;
            }
        }
    }

  // 3) packet is not broadcast, and is passed in with a route entry
  //    with a valid Ipv4Address as the gateway
  if (route && route->GetGateway () != Ipv4Address ())
    {
      NS_LOG_LOGIC ("Ipv4L3Protocol::Send case 3:  passed in with route");
      ipHeader = BuildHeader (source, destination, protocol, packet->GetSize (), ttl, tos, mayFragment);
      int32_t interface = GetInterfaceForDevice (route->GetOutputDevice ());
      m_sendOutgoingTrace (ipHeader, packet, interface);
      SendRealOut (route, packet->Copy (), ipHeader);
      return; 
    } 
  // 4) packet is not broadcast, and is passed in with a route entry but route->GetGateway is not set (e.g., on-demand)
  if (route && route->GetGateway () == Ipv4Address ())
    {
      // This could arise because the synchronous RouteOutput() call
      // returned to the transport protocol with a source address but
      // there was no next hop available yet (since a route may need
      // to be queried).
      NS_FATAL_ERROR ("Ipv4L3Protocol::Send case 4: This case not yet implemented");
    }
  // 5) packet is not broadcast, and route is NULL (e.g., a raw socket call)
  NS_LOG_LOGIC ("Ipv4L3Protocol::Send case 5:  passed in with no route " << destination);
  Socket::SocketErrno errno_; 
  Ptr<NetDevice> oif (0); // unused for now
  ipHeader = BuildHeader (source, destination, protocol, packet->GetSize (), ttl, tos, mayFragment);
  Ptr<Ipv4Route> newRoute;
  if (m_routingProtocol != 0)
    {
      newRoute = m_routingProtocol->RouteOutput (packet, ipHeader, oif, errno_);
    }
  else
    {
      NS_LOG_ERROR ("Ipv4L3Protocol::Send: m_routingProtocol == 0");
    }
  if (newRoute)
    {
      int32_t interface = GetInterfaceForDevice (newRoute->GetOutputDevice ());
      m_sendOutgoingTrace (ipHeader, packet, interface);
      SendRealOut (newRoute, packet->Copy (), ipHeader);
    }
  else
    {

      NS_LOG_LOGIC(" node "<<m_node->GetId()<<"No route to host.  Drop.");
      NS_LOG_WARN ("No route to host.  Drop.");
      m_dropTrace (ipHeader, packet, DROP_NO_ROUTE, m_node->GetObject<Ipv4> (), 0);
    }
}

// \todo when should we set ip_id?   check whether we are incrementing
// m_identification on packets that may later be dropped in this stack
// and whether that deviates from Linux
Ipv4Header
Ipv4L3Protocol::BuildHeader (
  Ipv4Address source,
  Ipv4Address destination,
  uint8_t protocol,
  uint16_t payloadSize,
  uint8_t ttl,
  uint8_t tos,
  bool mayFragment)
{
  NS_LOG_FUNCTION (this << source << destination << (uint16_t)protocol << payloadSize << (uint16_t)ttl << (uint16_t)tos << mayFragment);
  Ipv4Header ipHeader;
  ipHeader.SetSource (source);
  ipHeader.SetDestination (destination);
  ipHeader.SetProtocol (protocol);
  ipHeader.SetPayloadSize (payloadSize);
  ipHeader.SetTtl (ttl);
  ipHeader.SetTos (tos);

  uint64_t src = source.Get ();
  uint64_t dst = destination.Get ();
  uint64_t srcDst = dst | (src << 32);
  std::pair<uint64_t, uint8_t> key = std::make_pair (srcDst, protocol);

  if (mayFragment == true)
    {
      ipHeader.SetMayFragment ();
      ipHeader.SetIdentification (m_identification[key]);
      m_identification[key]++;
    }
  else
    {
      ipHeader.SetDontFragment ();
      // RFC 6864 does not state anything about atomic datagrams
      // identification requirement:
      // >> Originating sources MAY set the IPv4 ID field of atomic datagrams
      //    to any value.
      ipHeader.SetIdentification (m_identification[key]);
      m_identification[key]++;
    }
  if (Node::ChecksumEnabled ())
    {
      ipHeader.EnableChecksum ();
    }
  return ipHeader;
}

double Ipv4L3Protocol::GetStoreRate(std::string fkey)
{
  //NS_LOG_UNCOND("GetStoreRate: for key "<<fkey);
  return store_rate[fkey];
}

double Ipv4L3Protocol::GetShortTermRate(std::string fkey)
{
  return GetRate(fkey, SHORTER);
}

double Ipv4L3Protocol::GetMeasurementRate(std::string fkey)
{
  return GetRate(fkey, MEASUREMENT);
}

double Ipv4L3Protocol::GetRate(std::string fkey, Term term)
{
  if(term == LONGER) { 
    if (long_term_ewma_rate.find(fkey) == long_term_ewma_rate.end()) {
      return -1;
    }
    return long_term_ewma_rate[fkey];
  }
  if(term == SHORTER) {
    if (short_term_ewma_rate.find(fkey) == short_term_ewma_rate.end()) {
      return -1;
    }
    return short_term_ewma_rate[fkey];
  }
  if(term == MEASUREMENT) {
    if (measurement_rate.find(fkey) == measurement_rate.end()) {
      return -1;
    }
    return measurement_rate[fkey];
  }
  return -1;
    
}

double Ipv4L3Protocol::GetCSFQRate(std::string fkey)
{
  return long_term_ewma_rate[fkey];
}

double Ipv4L3Protocol::GetShortRate(std::string fkey)
{
  return short_term_ewma_rate[fkey];
}
double Ipv4L3Protocol::GetStoreDestRate(std::string fkey)
{
  //NS_LOG_UNCOND("GetStoreRate: for key "<<fkey);
  return store_dest_rate[fkey];
}
    
double Ipv4L3Protocol::GetStorePrio(std::string fkey)
{
  //NS_LOG_UNCOND("GetStorePrio: for key "<<fkey);
  return store_prio[fkey];
}

double getInterpolation(double fvalue, double x_vec[], double y_vec[], uint32_t num_points ){

    int i=0;
    double output;
     while( fvalue > x_vec[i]){
        i++;
        if (i > num_points){
            i = num_points-1;
            break;
        }
     }
     if (i==0){
      return 0.0;
    }
    if (fvalue > x_vec[num_points-1]){
      return y_vec[num_points-1];
    }
    output=(fvalue - x_vec[i-1])*(y_vec[i] - y_vec[i-1])/(x_vec[i] - x_vec[i-1]) + y_vec[i-1];
    return output;
}

double Bf(uint32_t fid, double fvalue){

    double output;
    
    const int num_points=5;
    double  x_vec1 []= {0, 1.0,   2.0, 3.0 , 5.0};
    double  y_vec1 []=  {0, 10, 10.5, 20.0 , 20.5};
    double  x_vec2 []= {0, 2.0,  2.5 , 3.5, 5.0};
    double  y_vec2 []= {0, 0.5,  10.0 , 15.0, 15.5};
    
    /*const int num_points=4;
    double  x_vec1 []= {0, 0.25,  .75 , 1.0000};
    double  y_vec1 []=  {0, 0.25,  .75 , 1.0000};
    double  x_vec2 []= {0, 0.25,  .75 , 1.0000};
    double  y_vec2 []= {0, 0.5,  1.5 , 2.0000};
    */
    
    /*
    double  x_vec1 []= {0, 0.999900000000000 ,  1.999900000000000 , 99.999899999999997};
    double  y_vec1 []= {0,  0.004999500000000 ,  0.014999000000000,  9.814990000000000};
    double  x_vec2 []= {0, 0.999900000000000 ,  1.999900000000000 , 99.999899999999997};
    double  y_vec2 []= {0,  0.004999500000000 ,  0.014999000000000,  9.814990000000000};
    */
    for(uint32_t i=0; i<num_points; i++) {
        y_vec1[i] = y_vec1[i]*1e+03;
        y_vec2[i] = y_vec2[i]*1e+03;
    }

    int i;
    if (fid ==101){
        output= getInterpolation(fvalue,x_vec1,y_vec1,num_points);
        std::cout<<"BF OUTPUT fid "<<fid<<" output "<<output<<" fvalue "<<fvalue<<std::endl;
    }

    if (fid ==201){
        output= getInterpolation(fvalue,x_vec2,y_vec2,num_points);
        std::cout<<"BF OUTPUT fid "<<fid<<" output "<<output<<" fvalue "<<fvalue<<std::endl;
    }

    return output;

}


double Bf_inverse(uint32_t fid, double fvalue){

    double output;
    const int num_points=5;
    double  y_vec1 []= {0, 1.0,   2.0, 3.0 , 5.0};
    double  x_vec1 []=  {0, 10, 10.5, 20.0 , 20.5};
    double  y_vec2 []= {0, 2.0,  2.5 , 3.5, 5.0};
    double  x_vec2 []= {0, 0.5,  10.0 , 15, 15.5};
    
    /* 
    const int num_points=4;

    double  x_vec1 []= {0, 0.25,  .75 , 1.0000};
    double  y_vec1 []=  {0, 0.25,  .75 , 1.0000};
    double  x_vec2 []= {0, 0.5,  1.5 , 2.0000};
    double  y_vec2 []= {0, 0.25,  .75 , 1.0000};
    */

/*    double  x_vec1 []= {0, 0.999900000000000 ,  1.999900000000000 , 99.999899999999997};
    double  y_vec1 []= {0,  0.004999500000000 ,  0.014999000000000,  9.814990000000000};
    double  x_vec2 []= {0, 0.999900000000000 ,  1.999900000000000 , 99.999899999999997};
    double  y_vec2 []= {0,  0.004999500000000 ,  0.014999000000000,  9.814990000000000};
  */  
    for(uint32_t i=0; i<num_points; i++) {
        x_vec1[i] = x_vec1[i]*1e+03;
        x_vec2[i] = x_vec2[i]*1e+03;
    }

    int i;
    if (fid ==101){
        output= getInterpolation(fvalue,x_vec1,y_vec1,num_points);
        std::cout<<"BFOUTPUT_INVERSE fid "<<fid<<" output "<<output<<" fvalue "<<fvalue<<std::endl;
    }

    if (fid ==201){
        output= getInterpolation(fvalue,x_vec2,y_vec2,num_points);
       std::cout<<"BFOUTPUT_INVERSE fid "<<fid<<" output "<<output<<" fvalue "<<fvalue<<std::endl;
    }

     return output;
}


void Ipv4L3Protocol::updateMarginalUtility(std::string fkey, double cur_rate1)
{
  uint32_t fid = 0;
  fid = flowids[fkey];
  double pri = 0.0;

  double cur_rate = 0.0;
  if(m_mptcp) {
    std::map<std::string, uint32_t>::iterator it;
    for (it=flowids.begin(); it!=flowids.end(); ++it) {
       //if(it->second != 0 && it->second != fid) {
       if(it->second != 0) {
     //     std::cout<<" Marginal utility adding subflow "<<it->first<<" with id "<<it->second<<" on node "<<m_node->GetId()<<" my id "<<fid<<" my rate "<<cur_rate1<<" adding "<<GetRate(it->first, SHORTER)<<" my key "<<fkey<<std::endl;
         cur_rate += GetRate(it->first, SHORTER);
       }
    }
  } else {
    cur_rate = cur_rate1;
  }

  /* This check to update priorities only at the source.. Not at a forwarding node */
    if(fid != 0) {
      if(m_method == 1) { //kn - this is the default
        pri = flowutil.getPrioByFlowID(fid, cur_rate);
      } else if(m_method == 2) {
        //pri = flowutil.getFCTUtilByFlowID(fid, cur_rate);
        pri = flowutil.getFCTUtilDerivative(fid, cur_rate);
      } else if(m_method == 3) {
        pri = flowutil.getAlpha1UtilByFlowID(fid, cur_rate);
      } else if(m_method == 4) {
        pri = flowutil.getAlpha1UtilByFlowID(fid, Bf_inverse( fid, cur_rate));
      }
        
    } else {
      // this seems to be an ACK packet
      pri = MAX_DOUBLE;
    }


/* if(pri > 1.0) {
    pri = 1.0;
 } // we don't care about rates lower than 1Mbps - kn - is this right?
*/
//  NS_LOG_LOGIC("UpdateMarginalUtility node "<<m_node->GetId()<<" flow "<<fkey<<" flow "<<fid<<" priority "<<pri<<" rate "<<cur_rate);
  store_prio[fkey] = pri;
}		

double Ipv4L3Protocol::GetStoreDestPrio(std::string fkey)
{
  //NS_LOG_UNCOND("GetStorePrio: for key "<<fkey);
  return store_dest_prio[fkey];
}    

bool Ipv4L3Protocol::isDestination(std::string node)
{
  std::vector<std::string>::iterator it;
  for (it=flow_dests.begin(); it!=flow_dests.end(); ++it) 
  {
    if(*it == node) {
      //NS_LOG_UNCOND("node "<<node<<" is a dest returning true");
      return true;
    }
  }
  return false;
}

void Ipv4L3Protocol::removeFromDropList(uint32_t id)
{
  drop_list[id] = 0;
}
  

void Ipv4L3Protocol::addToDropList(uint32_t id)
{
  NS_LOG_LOGIC(Simulator::Now().GetSeconds()<<" nodeid "<<m_node->GetId()<<" added flow "<<id<<" to drop list");
  drop_list[id] = 1;
}


double Ipv4L3Protocol::getflowsize(std::string flowkey)
{
  uint32_t fid = flowids[flowkey];
  return fsizes_copy[fid];
}

double Ipv4L3Protocol::utilInverse(std::string s, double link_price)
{
  return utilInverse(s, link_price, m_method);
}
     

double Ipv4L3Protocol::utilInverse(std::string s, double link_price, int method)
{
  uint32_t fid = 0;
  fid = flowids[s];
  double targetRate = 0.0;
  switch (method)
  {
    case LOGUTILITY: 
      targetRate = flowutil.getUtilInverseByFlowId(fid, link_price);
      break;
    case FCTUTILITY:
      //targetRate = flowutil.getFCTInverseByFlowId(fid, link_price);
      targetRate = flowutil.getFCTUtilDerivativeInverse(fid, link_price);
//      NS_LOG_LOGIC(Simulator::Now().GetSeconds()<<" node "<<m_node->GetId()<<" getting fct utility fid "<<fid<<" price "<<link_price<<" targetrate "<<targetRate);
      break;
    case ALPHA1UTILITY:
      targetRate = flowutil.getAlpha1InverseByFlowId(fid, link_price);
      break;
  } 
  return targetRate;
}


double Ipv4L3Protocol::getVirtualPktLength(Ptr<Packet> packet, Ipv4Header &ipHeader)
{
   /* What flow does this packet represnt ?*/
   TcpHeader tcph;
   uint16_t sourcePort, destPort;  
   Ipv4Address source = ipHeader.GetSource();
   Ipv4Address destination = ipHeader.GetDestination();

   packet->PeekHeader(tcph);
   sourcePort = tcph.GetSourcePort();
   destPort = tcph.GetDestinationPort();
   
   double current_netw_price =  tcph.GetPrice();
   //uint32_t current_pathprice = tcph.GetPP();

   std::stringstream ss;
   ss <<source<<":"<<destination<<":"<<destPort;
   std::string flowkey = ss.str(); 
  
   // Calculate the rate corresponding to this price
   // the price was calculated using rates that were in Mbps. So, this rate is in Mbps
   // TODO: this number is link_rate
   double link_rate = line_rate;
   //double limit_tr = 10.0;
   double limit_tr = 1.5; //change !!!
   double target_rate = limit_tr* link_rate;
   if(current_netw_price > 0.0) {
      if(m_method == 1) {
        target_rate = utilInverse(flowkey, current_netw_price, LOGUTILITY);
      } else if(m_method == 2) {
        target_rate = utilInverse(flowkey, current_netw_price, FCTUTILITY);
      } else if(m_method == 3) {
        target_rate = utilInverse(flowkey, current_netw_price, ALPHA1UTILITY);
      } else if(m_method == 4) {
        target_rate =  Bf( flowids[flowkey]  ,utilInverse(flowkey, current_netw_price, ALPHA1UTILITY));
      }
    }


	if(m_mptcp) {
       /* sum up rates of all other subflows */
       /* this flow's id */
      uint32_t own_id = flowids[flowkey];
      double subflow_sum = 0.0;
      std::map<std::string, uint32_t>::iterator it;
      for (it=flowids.begin(); it!=flowids.end(); ++it) {
 //        if(own_id == it->second || it->second == 0) {
         if(it->second == 0) {
           continue;
         }
     //   std::cout<<" Target rate adding subflow "<<it->first<<" with id "<<it->second<<" on node "<<m_node->GetId()<<" my id "<<own_id<<std::endl;
        subflow_sum += GetRate(it->first, SHORTER);
       }

       // subtract this from target rate
     //  double final_rate = target_rate - subflow_sum;
      // target_rate -= subflow_sum;
      double final_rate = target_rate;
       if(subflow_sum > 0.0)
         final_rate = GetRate(flowkey, SHORTER) * (target_rate / subflow_sum);
//       std::cout<<" target_rate_debug node "<<m_node->GetId()<<" "<<Simulator::Now().GetSeconds     ()<<" flow "<<own_id<<" target_rate "<<target_rate<<" final_rate "<<final_rate<<" subflow_sum      "<<subflow_sum<<std::endl;
       target_rate = final_rate;
     }
    
    // Do we need to cap this ? 
    if(target_rate > (limit_tr* link_rate)) {
      target_rate = limit_tr* link_rate;
    } 


    if(target_rate <= 0.0) {
      /* This happens for unknown flows - currently for the ACK packets. They must get the highest priority
       * So, their deadlines are closest - in the past ! */
      return 0.0;
    }


    uint32_t pkt_dur = ((packet->GetSize() + 46) * 8.0 * 1000.0) / target_rate;  //in us since target_rate is in Mbps - multiplying by 1000 to get it in nanoseconds
    double current_deadline = pkt_dur; 
    uint32_t tcphsize = tcph.GetSerializedSize();
    if((packet->GetSize() - tcphsize) == 0) {
        // it's an ack
        current_deadline = 0;
        target_rate = line_rate;
    }
    if(m_pfabric) {
      current_deadline = getflowsize(flowkey);
      //std::cout<<"pfabric true - flowid "<<flowids[flowkey]<<" flowsize "<<current_deadline<<std::endl;
    }

    last_deadline = current_deadline;
    sample_deadline = current_deadline;
    //flow_target_rate[flowkey] = target_rate;

    if(m_wfq) {
      uint32_t fid = 0;
      fid = flowids[flowkey];
      double fweight = 1.0; //futils_copy[fid];
      return ((packet->GetSize()+46)*8.0) / fweight;
    } 

//    std::cout<<Simulator::Now().GetSeconds()<<" node "<<m_node->GetId()<<" fid "<<flowids[flowkey]<<" pkt_dur "<<pkt_dur<<" flow "<<flowkey<<" pkt size "<<8.0*(packet->GetSize()+46)<<" target_rate "<<target_rate<<" current_deadline "<<current_deadline<<" current_netw_price "<<current_netw_price<<std::endl; 
    return current_deadline * 1.0;
}

double Ipv4L3Protocol::get_wfq_weight(Ptr<Packet> packet, Ipv4Header &ipHeader)
{
   /* What flow does this packet represnt ?*/
   TcpHeader tcph;
   uint16_t sourcePort, destPort;  
   Ipv4Address source = ipHeader.GetSource();
   Ipv4Address destination = ipHeader.GetDestination();

   packet->PeekHeader(tcph);
   sourcePort = tcph.GetSourcePort();
   destPort = tcph.GetDestinationPort();
   
   std::stringstream ss;
   ss <<source<<":"<<destination<<":"<<destPort;
   std::string flowkey = ss.str(); 

   uint32_t fid = flowids[flowkey];


   double fweight = fweights_copy[fid];
   if(fweight == 0.0) {
    fweight = 1000.0; //highest weight for unknown flows == acks
   }
   
  // NS_LOG_LOGIC(" node "<<m_node->GetId()<<" weight "<<fweight<<" returning "<<((packet->GetSize()+46)*8.0)/fweight<<" "<<flowkey<<" "<<Simulator::Now().GetSeconds());  
   return fweight;
//   return ((packet->GetSize()+30.0)*8.0) / fweight;
}

void Ipv4L3Protocol::updateAverages(std::string flowkey, double inter_arrival, double pktsize)
{

  double pkt_rate = 10000.0;
  if(inter_arrival == -1) {  //invalid
//    std::cout<<Simulator::Now().GetSeconds()<<" invalid inter-arrival - returning node "<<m_node->GetId()<<std::endl;
 /*   long_term_ewma_rate[flowkey] = line_rate/1000000.0;
    short_term_ewma_rate[flowkey] = line_rate/1000000.0;
    measurement_rate[flowkey] = line_rate/1000000.0; */
    return;
  }
//    std::cout<<Simulator::Now().GetSeconds()<<" updating average node "<<m_node->GetId()<<" flowkey "<<flowkey<<" inter_arrival "<<inter_arrival<<std::endl;

/*  if(inter_arrival == 0.0) {
//    std::cout<<Simulator::Now().GetSeconds()<<" invalid inter-arrival - returning node "<<m_node->GetId()<<" flowkey "<<flowkey<<std::endl;
   inter_arrival = 0.0000000001; //shouldn't happen - fixing something I don't understand
   return;
  }
*/

  // else calculate instant rate
  if(inter_arrival > 0.000000000001) { // bug fix - verify later
    pkt_rate = (pktsize * 1.0 * 8.0) / (inter_arrival * 1.0e-9 * 1.0e+6);
  }
  instant_rate_store[flowkey] = pkt_rate;

  // first time we got a rate feedback

  // print all long_term_ewma entries here

  if (long_term_ewma_rate.find(flowkey) == long_term_ewma_rate.end() || long_term_ewma_rate[flowkey] == 0.0) { // first time
//    std::cout<<Simulator::Now().GetSeconds()<<" node "<<m_node->GetId()<<" first ewma update for flow "<<flowkey<<" with pkt rate "<<pkt_rate<<std::endl;
    long_term_ewma_rate[flowkey] = pkt_rate;
    short_term_ewma_rate[flowkey] = pkt_rate;
    measurement_rate[flowkey] = pkt_rate;
    return;
  }
    

  if(inter_arrival > 8000000) { // for a flow that was inactive for along time - 8 ms
//    std::cout<<Simulator::Now().GetSeconds()<<" node "<<m_node->GetId()<<" first ewma update for flow "<<flowkey<<" with pkt rate "<<pkt_rate<<" inter_arrival "<<inter_arrival<<std::endl;
    long_term_ewma_rate[flowkey] = pkt_rate;
    short_term_ewma_rate[flowkey] = pkt_rate;
    measurement_rate[flowkey] = pkt_rate;
    return;
  }

  /* if(m_node->GetId() == 2 || m_node->GetId() == 3) {
    std::cout<<"instant_rate "<<Simulator::Now().GetSeconds()<<" "<<flowids[flowkey]<<" "<<pkt_rate<<std::endl;
  } */
//  if(flowids[flowkey] == 8)
//  std::cout<<"ratesample "<<pkt_rate<<" node "<<m_node->GetId()<<" flow "<<flowids[flowkey]<<" time "<<Simulator::Now().GetSeconds()<<" inter_arrival "<<inter_arrival<<" pktsize "<<pktsize<<std::endl;

//  std::cout<<Simulator::Now().GetSeconds()<<" ewma update for flow "<<flowkey<<" with pkt rate "<<pkt_rate<<" current ewma "<<long_term_ewma_rate[flowkey]<<" short term "<<short_term_ewma_rate[flowkey]<<std::endl;
  double epower = exp((-1.0*inter_arrival)/long_ewma_const);
  double first_term = (1.0 - epower)*pkt_rate;
  double second_term = epower * long_term_ewma_rate[flowkey];
  double new_rate = first_term + second_term;
  long_term_ewma_rate[flowkey] = new_rate;


  // calculate short term ewma
  //
  epower = exp((-1.0*inter_arrival)/short_ewma_const);
  first_term = (1.0 - epower)*pkt_rate;
  second_term = epower * short_term_ewma_rate[flowkey];
  short_term_ewma_rate[flowkey] = first_term + second_term;

  epower = exp((-1.0*inter_arrival)/measurement_ewma_const);
  first_term = (1.0 - epower)*pkt_rate;
  second_term = epower * measurement_rate[flowkey];
  measurement_rate[flowkey] = first_term + second_term;
}


void Ipv4L3Protocol::updateInterArrival(std::string flowkey)
{

  double inter_arr = -1.0;

  if(last_arrival.find(flowkey) != last_arrival.end()) { //not first packet of the flow
      inter_arr = Simulator::Now().GetNanoSeconds() - last_arrival[flowkey];
  }

/* if(inter_arr == 0.0) {
  std::cout<<"Inter_arrival is zer "<<flowkey<<" node "<<m_node->GetId()<<" "<<Simulator::Now().GetSeconds()<<std::endl;
  }*/

  /* debug info */
  if(inter_arr == -1.0) {
  //  std::cout<<Simulator::Now().GetSeconds()<<" node "<<m_node->GetId()<<" inter_arr -1"<<std::endl;
  }
    
/*  if ((last_arrival.find(flowkey) == last_arrival.end()) || ((Simulator::Now().GetNanoSeconds() - last_arrival[flowkey]) > 1000000000))  {
  //if (last_arrival.find(flowkey) == last_arrival.end()){
    // first packet of the flow
    last_arrival[flowkey] = Simulator::Now().GetNanoSeconds() - 1.0;
//    long_term_ewma_rate[flowkey] = 0.0;
 //  short_term_ewma_rate[flowkey] = 0.0;
  } */

  inter_arrival[flowkey]  = inter_arr;
  last_arrival[flowkey] = Simulator::Now().GetNanoSeconds();
}

double Ipv4L3Protocol::getInterArrival(std::string fkey)
{
  if(last_arrival.find(fkey) != last_arrival.end()) {
	  return inter_arrival[fkey];
  }
  // did not find any packet - so return -1
  return -1; 
}
  

PriHeader Ipv4L3Protocol::AddPrioHeader(Ptr<Packet> packet, Ipv4Header &ipHeader)
{

  /* What flow does this packet represnt ?*/
  TcpHeader tcph;
  uint16_t sourcePort, destPort;  
  Ipv4Address source = ipHeader.GetSource();
  Ipv4Address destination = ipHeader.GetDestination();

  packet->PeekHeader(tcph);
  sourcePort = tcph.GetSourcePort();
  destPort = tcph.GetDestinationPort();
  double current_netw_price = tcph.GetPrice();

  std::stringstream ss;
  ss <<source<<":"<<destination<<":"<<destPort;
  std::string flowkey = ss.str(); 


  // Calculate the rate corresponding to this price
  // the price was calculated using rates that were in Mbps. So, this rate is in Mbps
  //uint32_t pktsize = packet->GetSize() + 46; //adding IP + PrioHeader + ppp header sizes
  uint32_t pktsize = packet->GetSize(); //adding IP + PrioHeader + ppp header sizes

  // Commenting this out KN - no need to measure rate at sender - seems to be of no use
 // KN - instead let's update priority here from the rate recvd from recvr

   totalbytes[flowkey] += pktsize;
  

   double wfqw = 1.0;
   double virtual_pkt_length = 0.0; //current deadline
   double netw_price_init = 0.0;

   //if(alpha_fair_rcp) {
//		netw_price_init = 10000.0;
//	}
   PriHeader priheader = PriHeader(wfqw, virtual_pkt_length, netw_price_init);

  if (GetInterfaceForAddress(source) != -1) {
 	  updateMarginalUtility(flowkey, GetRate(flowkey, LONGER));
	  virtual_pkt_length = getVirtualPktLength(packet, ipHeader);

    NS_LOG_LOGIC("NODE "<<m_node->GetId()<<" getVirtualPacketLength returned "<<virtual_pkt_length);


    priheader.wfq_weight = virtual_pkt_length;
   
    if(alpha_fair_rcp) {
		priheader.residue = flow_rtt[flowkey];
	} else {
	    priheader.residue = (store_prio[flowkey] - current_netw_price);
	}

    

 /*   if(flowids[flowkey] == 8) {
       std::cout<<Simulator::Now().GetSeconds()<<" virtual_pkt_length "<<virtual_pkt_length<<" "<<flowkey<<" "<<pktsize<<std::endl;
    } */


    uint32_t flow_num_hops = 1;
 //  if(host_compensate) {
	 if(!alpha_fair_rcp) {
      if(num_hops.find(flowkey) != num_hops.end()) {
      	flow_num_hops = num_hops[flowkey];
      } else {
   	    flow_num_hops = 4;
     }	
	 } 
   //} 

    if(price_valid[flowkey]) {
      priheader.residue = priheader.residue / (1.0*flow_num_hops);
    } else {
//      std::cout<<"putting highest residue in pkt since we don't yet know the rates "<<Simulator::Now().GetSeconds()<<" node "<<m_node->GetId()<<" flow "<<flowkey<<std::endl;
      priheader.residue = 50000.0; // basically invalid value
    }
    priheader.netw_price = netw_price_init;  // start the network price at zero
//  std::cout<<"NETW_PRICE "<<Simulator::Now().GetSeconds()<<" AddPrioHeader node "<<m_node->GetId()<<" flowid "<<flowids[flowkey] <<" store_prio "<<store_prio[flowkey]<<" current_netw_price "<<current_netw_price<<" margin_util "<<priheader.residue<<" wfq_weight "<<priheader.wfq_weight<<" flowkey "<<flowkey<<" host_compensate "<<host_compensate<<" "<<flowkey<<std::endl; 

 
  } else {
    /* Store this every packet */
    /* This is just forwarding the flow */
    priheader.wfq_weight = pre_set_wfq_weight;
	  priheader.residue = pre_set_residue;
	  priheader.netw_price = pre_set_netw_price;
  }

  return priheader;
}

void
Ipv4L3Protocol::setFlowWeight(uint32_t fid, double weight)
{
  fweights_copy[fid] = weight;
  flowutil.setflowweight(fid, weight);
}
 
void
Ipv4L3Protocol::setFlowUtils(std::vector<double> futils)
{
  for (uint32_t i=0; i<futils.size(); i++) {
    fweights_copy[i] = futils[i];
  }
  flowutil.setWeights(fweights_copy); 
}


void
Ipv4L3Protocol::setFlowSizes(std::map<uint32_t, double> fsizes)
{
  std::map<uint32_t, double>::iterator it;
  for (std::map<uint32_t, double>::iterator it=fsizes.begin(); it!=fsizes.end(); ++it) {
    fsizes_copy[it->first] = it->second;
  }

  flowutil.setSizes(fsizes); 
}

void Ipv4L3Protocol::removeFlow(uint32_t fid)
{
  std::map<std::string, uint32_t>::iterator it;
  for (it=flowids.begin(); it!=flowids.end(); ++it) {
	if(it->second == fid) {
		std::cout<<"removing flowid "<<fid<<" with key "<<it->first<<" from node "<<m_node->GetId()<<std::endl;
	        flowids.erase(it);
		return;
	}
  }
}	

void Ipv4L3Protocol::setPriceValid(std::string flow)
{
  price_valid[flow] = true;
}

void Ipv4L3Protocol::setNumHops(std::string flow, uint32_t nh)
{
  num_hops[flow] = nh;
}

  
void Ipv4L3Protocol::setFlow(std::string flow, uint32_t flowid, double fsize, uint32_t weight)
{

    std::cout<<" Ipv4L3Protocol::SetFlow "<<m_node->GetId()<<" flowid "<<flowid<<" flow "<<flow<<" size "<<fsize<<" weight "<<weight<<std::endl;

    flowids[flow] = flowid;
    price_valid[flow] = false;
    fsizes_copy[flowid] = fsize;
    fweights_copy[flowid] = weight;

    std::string delimiter = ":";
    const std::string dest = (flow).substr(1, (flow).find(delimiter));

    std::size_t pos = (flow).find(":");      // position of "live" in str
    std::string str3 = (flow).substr ((pos+1));
    const std::string real_dest = str3.substr(0, str3.find(delimiter));
    
    NS_LOG_UNCOND("Pushing "<<real_dest<<" into destinations list");
    flow_dests.push_back(real_dest);
    flowutil.SetFlow(flow, flowid, fsize, weight);
  
    data_recvd[flow] = 0.0;
}

void Ipv4L3Protocol::setfctAlpha(double fct_alpha)
{
  flowutil.SetFCTAlpha(fct_alpha);
  my_fct_alpha = fct_alpha;
}

void
Ipv4L3Protocol::setFlows(std::map<std::string, uint32_t> flowid_set)
{
  std::map<std::string,uint32_t>::iterator it;
  for (std::map<std::string,uint32_t>::iterator it=flowid_set.begin(); it!=flowid_set.end(); ++it) 
  {
//    NS_LOG_LOGIC(Simulator::Now().GetSeconds()<< " Node "<<m_node->GetId()<<" Set flow key "<<it->first<<" flow id "<<it->second);
    flowids[it->first] = it->second;
    last_residue[it->first] = 0.0;
    total_samples[it->first] = 0;
    current_residue[it->first] = 0.0;
    // get destinations
    std::string delimiter = ":";
    const std::string dest = (it->first).substr(1, (it->first).find(delimiter));

    std::size_t pos = (it->first).find(":");      // position of "live" in str
    std::string str3 = (it->first).substr ((pos+1));
    const std::string real_dest = str3.substr(0, str3.find(delimiter));
    
    NS_LOG_UNCOND("Pushing "<<real_dest<<" into destinations list");
    flow_dests.push_back(real_dest);

  }
}
 

void
Ipv4L3Protocol::SendRealOut (Ptr<Ipv4Route> route,
                             Ptr<Packet> packet,
                             Ipv4Header &ipHeader)
{
  //NS_LOG_FUNCTION (this << route << packet << &ipHeader);
   TcpHeader tcph;
   uint16_t sourcePort, destPort;  
   Ipv4Address source = ipHeader.GetSource();
   Ipv4Address destination = ipHeader.GetDestination();

   packet->PeekHeader(tcph);
   sourcePort = tcph.GetSourcePort();
   destPort = tcph.GetDestinationPort();
   
   std::stringstream ss;
   ss <<source<<":"<<destination<<":"<<destPort;
   std::string flowkey = ss.str(); 

   uint32_t fid = flowids[flowkey];

   NS_LOG_LOGIC("SendRealOut" <<this << route << packet << &ipHeader);
   if (route == 0 || (fid != 0 && (drop_list.find(fid) != drop_list.end()) && drop_list[fid]==1))
    {
//      std::cout<<" DROPPING AT IP "<<Simulator::Now().GetSeconds()<<" "<<m_node->GetId()<<std::endl;
      NS_LOG_WARN ("No route to host.  Drop.");
      m_dropTrace (ipHeader, packet, DROP_NO_ROUTE, m_node->GetObject<Ipv4> (), 0);
      return;
    }
  /* kanthi */



  Ptr<NetDevice> outDev = route->GetOutputDevice ();
  PriHeader priheader = AddPrioHeader(packet, ipHeader);
  packet->AddHeader (ipHeader);

  PrioHeader prioheader;
  prioheader.SetData(priheader);
  packet->AddHeader (prioheader);

  NS_LOG_LOGIC("for flow "<<flowkey<<" sending wfq_weight "<<priheader.wfq_weight<<" residue "<<priheader.residue<<" netw_price "<<priheader.netw_price<<" node "<<m_node->GetId()<<" time "<<Simulator::Now().GetSeconds());

 /* kanthi end */

  int32_t interface = GetInterfaceForDevice (outDev);
  NS_ASSERT (interface >= 0);
  Ptr<Ipv4Interface> outInterface = GetInterface (interface);
  //NS_LOG_LOGIC ("Send via NetDevice ifIndex " << outDev->GetIfIndex () << " ipv4InterfaceIndex " << interface);
  //NS_LOG_UNCOND ("Node id "<<m_node->GetId()<<"Send via NetDevice ifIndex " << outDev->GetIfIndex () << " ipv4InterfaceIndex " << interface<<" packet size "<<packet->GetSize());

  if (!route->GetGateway ().IsEqual (Ipv4Address ("0.0.0.0")))
    {
      if (outInterface->IsUp ())
        {
          if ( packet->GetSize () > outInterface->GetDevice ()->GetMtu () )
            {
              std::list<Ptr<Packet> > listFragments;
              DoFragmentation (packet, outInterface->GetDevice ()->GetMtu (), listFragments);
              for ( std::list<Ptr<Packet> >::iterator it = listFragments.begin (); it != listFragments.end (); it++ )
                {
                  m_txTrace (*it, m_node->GetObject<Ipv4> (), interface);
                  outInterface->Send (*it, route->GetGateway ());
                }
            }
          else
            {
              m_txTrace (packet, m_node->GetObject<Ipv4> (), interface);
              outInterface->Send (packet, route->GetGateway ());
            }
        }
      else
        {
          NS_LOG_LOGIC ("Dropping -- outgoing interface is down: " << route->GetGateway ());
          Ipv4Header ipHeader;
          packet->RemoveHeader (ipHeader);
          m_dropTrace (ipHeader, packet, DROP_INTERFACE_DOWN, m_node->GetObject<Ipv4> (), interface);
        }
    } 
  else 
    {
      if (outInterface->IsUp ())
        {
          if ( packet->GetSize () > outInterface->GetDevice ()->GetMtu () )
            {
              std::list<Ptr<Packet> > listFragments;
              DoFragmentation (packet, outInterface->GetDevice ()->GetMtu (), listFragments);
              for ( std::list<Ptr<Packet> >::iterator it = listFragments.begin (); it != listFragments.end (); it++ )
                {
                  NS_LOG_LOGIC ("Sending fragment " << **it );
                  m_txTrace (*it, m_node->GetObject<Ipv4> (), interface);
                  outInterface->Send (*it, ipHeader.GetDestination ());
                }
            }
          else
            {
              m_txTrace (packet, m_node->GetObject<Ipv4> (), interface);
              outInterface->Send (packet, ipHeader.GetDestination ());
            }
        }
      else
        {
          NS_LOG_LOGIC ("Dropping -- outgoing interface is down: " << ipHeader.GetDestination ());
          Ipv4Header ipHeader;
          packet->RemoveHeader (ipHeader);
          m_dropTrace (ipHeader, packet, DROP_INTERFACE_DOWN, m_node->GetObject<Ipv4> (), interface);
        }
    }
}

// This function analogous to Linux ip_mr_forward()
void
Ipv4L3Protocol::IpMulticastForward (Ptr<Ipv4MulticastRoute> mrtentry, Ptr<const Packet> p, const Ipv4Header &header)
{
  NS_LOG_FUNCTION (this << mrtentry << p << header);
  NS_LOG_LOGIC ("Multicast forwarding logic for node: " << m_node->GetId ());

  std::map<uint32_t, uint32_t> ttlMap = mrtentry->GetOutputTtlMap ();
  std::map<uint32_t, uint32_t>::iterator mapIter;

  for (mapIter = ttlMap.begin (); mapIter != ttlMap.end (); mapIter++)
    {
      uint32_t interfaceId = mapIter->first;
      //uint32_t outputTtl = mapIter->second;  // Unused for now

      Ptr<Packet> packet = p->Copy ();
      Ipv4Header h = header;
      h.SetTtl (header.GetTtl () - 1);
      if (h.GetTtl () == 0)
        {
          NS_LOG_WARN ("TTL exceeded.  Drop.");
          m_dropTrace (header, packet, DROP_TTL_EXPIRED, m_node->GetObject<Ipv4> (), interfaceId);
          return;
        }
      NS_LOG_LOGIC ("Forward multicast via interface " << interfaceId);
      Ptr<Ipv4Route> rtentry = Create<Ipv4Route> ();
      rtentry->SetSource (h.GetSource ());
      rtentry->SetDestination (h.GetDestination ());
      rtentry->SetGateway (Ipv4Address::GetAny ());
      rtentry->SetOutputDevice (GetNetDevice (interfaceId));
      SendRealOut (rtentry, packet, h);
      continue;
    }
}

// This function analogous to Linux ip_forward()
void
Ipv4L3Protocol::IpForward (Ptr<Ipv4Route> rtentry, Ptr<const Packet> p, const Ipv4Header &header)
{
  NS_LOG_FUNCTION (this << rtentry << p << header);
  NS_LOG_LOGIC ("Forwarding logic for node: " << m_node->GetId ());
  // Forwarding
  Ipv4Header ipHeader = header;
  Ptr<Packet> packet = p->Copy ();
  int32_t interface = GetInterfaceForDevice (rtentry->GetOutputDevice ());
  ipHeader.SetTtl (ipHeader.GetTtl () - 1);
  if (ipHeader.GetTtl () == 0)
    {
      // Do not reply to ICMP or to multicast/broadcast IP address 
      if (ipHeader.GetProtocol () != Icmpv4L4Protocol::PROT_NUMBER && 
          ipHeader.GetDestination ().IsBroadcast () == false &&
          ipHeader.GetDestination ().IsMulticast () == false)
        {
          Ptr<Icmpv4L4Protocol> icmp = GetIcmp ();
          icmp->SendTimeExceededTtl (ipHeader, packet);
        }
      NS_LOG_WARN ("TTL exceeded.  Drop.");
      m_dropTrace (header, packet, DROP_TTL_EXPIRED, m_node->GetObject<Ipv4> (), interface);
      return;
    }
  m_unicastForwardTrace (ipHeader, packet, interface);
  SendRealOut (rtentry, packet, ipHeader);
}

void
Ipv4L3Protocol::LocalDeliver (Ptr<const Packet> packet, Ipv4Header const&ip, uint32_t iif)
{
  NS_LOG_FUNCTION (this << packet << &ip << iif);
  Ptr<Packet> p = packet->Copy (); // need to pass a non-const packet up
  uint32_t pktsize = p->GetSize() + 46;
  //uint32_t pktsize = p->GetSize();
  Ipv4Header ipHeader = ip;

  // incoming packets priority and remaining priority
  TcpHeader tcp_header;
  p->RemoveHeader(tcp_header);
  if((p->GetSize() > 0) || 
       ((tcp_header.GetFlags() & TcpHeader::SYN) && !(tcp_header.GetFlags() & TcpHeader::ACK))) {
  //if(p->GetSize() > 0) {
    TcpHeader tcph;
    uint16_t sourcePort, destPort;  
    Ipv4Address source = ipHeader.GetSource();
    Ipv4Address destination = ipHeader.GetDestination();

    packet->PeekHeader(tcph);
    sourcePort = tcph.GetSourcePort();
    destPort = tcph.GetDestinationPort();

    std::stringstream ss;
    ss <<source<<":"<<destination<<":"<<destPort;
    std::string flowkey = ss.str(); 
    
    destination_bytes[flowkey] += pktsize;
    data_recvd[flowkey] += pktsize;
/*
	if(m_node->GetId() == 31) {
		std::cout<<" flowkey "<<flowkey<<" "<<Simulator::Now().GetSeconds()<<" pkt delivered"<<std::endl;
	}
*/

    tcp_header.SetPrice(pre_set_netw_price);
//    std::cout<<" TCPPRICECOPY "<<Simulator::Now().GetSeconds()<<" node "<<m_node->GetId()<<" Flow "<<flowkey<<" Price "<<pre_set_netw_price<<" TCPFLAGS "<<(tcp_header.GetFlags()& TcpHeader::SYN)<<" ACKFLAG "<<(tcp_header.GetFlags() &TcpHeader::ACK)<<std::endl;

    if(p->GetSize() > 0) {
      updateInterArrival(flowkey);
    }
    
  } else {
 //   NS_LOG_UNCOND("LocalDeliver "<<Simulator::Now().GetSeconds()<<" node id "<<m_node->GetId()<<" Looks like an ACK packet.. leave it alone "<<tcp_header.GetCWR());
  }
  p->AddHeader(tcp_header);
  
  //TcpHeader tc1;
  //p->PeekHeader(tc1);
   
  // Attached header end 

  if ( !ipHeader.IsLastFragment () || ipHeader.GetFragmentOffset () != 0 )
    {
      NS_LOG_LOGIC ("Received a fragment, processing " << *p );
      bool isPacketComplete;
      isPacketComplete = ProcessFragment (p, ipHeader, iif);
      if ( isPacketComplete == false)
        {
          return;
        }
      NS_LOG_LOGIC ("Got last fragment, Packet is complete " << *p );
      ipHeader.SetFragmentOffset (0);
      ipHeader.SetPayloadSize (p->GetSize () + ipHeader.GetSerializedSize ());
    }

  m_localDeliverTrace (ipHeader, p, iif);

  Ptr<IpL4Protocol> protocol = GetProtocol (ipHeader.GetProtocol ());
  if (protocol != 0)
    {
      // we need to make a copy in the unlikely event we hit the
      // RX_ENDPOINT_UNREACH codepath
      Ptr<Packet> copy = p->Copy ();
      enum IpL4Protocol::RxStatus status = 
        protocol->Receive (p, ipHeader, GetInterface (iif));
      switch (status) {
        case IpL4Protocol::RX_OK:
        // fall through
        case IpL4Protocol::RX_ENDPOINT_CLOSED:
        // fall through
        case IpL4Protocol::RX_CSUM_FAILED:
          break;
        case IpL4Protocol::RX_ENDPOINT_UNREACH:
          if (ipHeader.GetDestination ().IsBroadcast () == true ||
              ipHeader.GetDestination ().IsMulticast () == true)
            {
              break; // Do not reply to broadcast or multicast
            }
          // Another case to suppress ICMP is a subnet-directed broadcast
          bool subnetDirected = false;
          for (uint32_t i = 0; i < GetNAddresses (iif); i++)
            {
              Ipv4InterfaceAddress addr = GetAddress (iif, i);
              if (addr.GetLocal ().CombineMask (addr.GetMask ()) == ipHeader.GetDestination ().CombineMask (addr.GetMask ()) &&
                  ipHeader.GetDestination ().IsSubnetDirectedBroadcast (addr.GetMask ()))
                {
                  subnetDirected = true;
                }
            }
          if (subnetDirected == false)
            {
              GetIcmp ()->SendDestUnreachPort (ipHeader, copy);
            }
        }
    }
}

bool
Ipv4L3Protocol::AddAddress (uint32_t i, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << i << address);
  Ptr<Ipv4Interface> interface = GetInterface (i);
  bool retVal = interface->AddAddress (address);
  if (m_routingProtocol != 0)
    {
      m_routingProtocol->NotifyAddAddress (i, address);
    }
  return retVal;
}

Ipv4InterfaceAddress 
Ipv4L3Protocol::GetAddress (uint32_t interfaceIndex, uint32_t addressIndex) const
{
  NS_LOG_FUNCTION (this << interfaceIndex << addressIndex);
  Ptr<Ipv4Interface> interface = GetInterface (interfaceIndex);
  return interface->GetAddress (addressIndex);
}

uint32_t 
Ipv4L3Protocol::GetNAddresses (uint32_t interface) const
{
  NS_LOG_FUNCTION (this << interface);
  Ptr<Ipv4Interface> iface = GetInterface (interface);
  return iface->GetNAddresses ();
}

bool
Ipv4L3Protocol::RemoveAddress (uint32_t i, uint32_t addressIndex)
{
  NS_LOG_FUNCTION (this << i << addressIndex);
  Ptr<Ipv4Interface> interface = GetInterface (i);
  Ipv4InterfaceAddress address = interface->RemoveAddress (addressIndex);
  if (address != Ipv4InterfaceAddress ())
    {
      if (m_routingProtocol != 0)
        {
          m_routingProtocol->NotifyRemoveAddress (i, address);
        }
      return true;
    }
  return false;
}

bool
Ipv4L3Protocol::RemoveAddress (uint32_t i, Ipv4Address address)
{
  NS_LOG_FUNCTION (this << i << address);

  if (address == Ipv4Address::GetLoopback())
    {
      NS_LOG_WARN ("Cannot remove loopback address.");
      return false;
    }
  Ptr<Ipv4Interface> interface = GetInterface (i);
  Ipv4InterfaceAddress ifAddr = interface->RemoveAddress (address);
  if (ifAddr != Ipv4InterfaceAddress ())
    {
      if (m_routingProtocol != 0)
        {
          m_routingProtocol->NotifyRemoveAddress (i, ifAddr);
        }
      return true;
    }
  return false;
}

Ipv4Address 
Ipv4L3Protocol::SelectSourceAddress (Ptr<const NetDevice> device,
                                     Ipv4Address dst, Ipv4InterfaceAddress::InterfaceAddressScope_e scope)
{
  NS_LOG_FUNCTION (this << device << dst << scope);
  Ipv4Address addr ("0.0.0.0");
  Ipv4InterfaceAddress iaddr; 
  bool found = false;

  if (device != 0)
    {
      int32_t i = GetInterfaceForDevice (device);
      NS_ASSERT_MSG (i >= 0, "No device found on node");
      for (uint32_t j = 0; j < GetNAddresses (i); j++)
        {
          iaddr = GetAddress (i, j);
          if (iaddr.IsSecondary ()) continue;
          if (iaddr.GetScope () > scope) continue; 
          if (dst.CombineMask (iaddr.GetMask ())  == iaddr.GetLocal ().CombineMask (iaddr.GetMask ()) )
            {
              return iaddr.GetLocal ();
            }
          if (!found)
            {
              addr = iaddr.GetLocal ();
              found = true;
            }
        }
    }
  if (found)
    {
      return addr;
    }

  // Iterate among all interfaces
  for (uint32_t i = 0; i < GetNInterfaces (); i++)
    {
      for (uint32_t j = 0; j < GetNAddresses (i); j++)
        {
          iaddr = GetAddress (i, j);
          if (iaddr.IsSecondary ()) continue;
          if (iaddr.GetScope () != Ipv4InterfaceAddress::LINK 
              && iaddr.GetScope () <= scope) 
            {
              return iaddr.GetLocal ();
            }
        }
    }
  NS_LOG_WARN ("Could not find source address for " << dst << " and scope " 
                                                    << scope << ", returning 0");
  return addr;
}

void 
Ipv4L3Protocol::SetMetric (uint32_t i, uint16_t metric)
{
  NS_LOG_FUNCTION (this << i << metric);
  Ptr<Ipv4Interface> interface = GetInterface (i);
  interface->SetMetric (metric);
}

uint16_t
Ipv4L3Protocol::GetMetric (uint32_t i) const
{
  NS_LOG_FUNCTION (this << i);
  Ptr<Ipv4Interface> interface = GetInterface (i);
  return interface->GetMetric ();
}

uint16_t 
Ipv4L3Protocol::GetMtu (uint32_t i) const
{
  NS_LOG_FUNCTION (this << i);
  Ptr<Ipv4Interface> interface = GetInterface (i);
  return interface->GetDevice ()->GetMtu ();
}

bool 
Ipv4L3Protocol::IsUp (uint32_t i) const
{
  NS_LOG_FUNCTION (this << i);
  Ptr<Ipv4Interface> interface = GetInterface (i);
  return interface->IsUp ();
}

void 
Ipv4L3Protocol::SetUp (uint32_t i)
{
  NS_LOG_FUNCTION (this << i);
  Ptr<Ipv4Interface> interface = GetInterface (i);

  // RFC 791, pg.25:
  //  Every internet module must be able to forward a datagram of 68
  //  octets without further fragmentation.  This is because an internet
  //  header may be up to 60 octets, and the minimum fragment is 8 octets.
  if (interface->GetDevice ()->GetMtu () >= 68)
    {
      interface->SetUp ();

      if (m_routingProtocol != 0)
        {
          m_routingProtocol->NotifyInterfaceUp (i);
        }
    }
  else
    {
      NS_LOG_LOGIC ("Interface " << int(i) << " is set to be down for IPv4. Reason: not respecting minimum IPv4 MTU (68 octects)");
    }
}

void 
Ipv4L3Protocol::SetDown (uint32_t ifaceIndex)
{
  NS_LOG_FUNCTION (this << ifaceIndex);
  Ptr<Ipv4Interface> interface = GetInterface (ifaceIndex);
  interface->SetDown ();

  if (m_routingProtocol != 0)
    {
      m_routingProtocol->NotifyInterfaceDown (ifaceIndex);
    }
}

bool 
Ipv4L3Protocol::IsForwarding (uint32_t i) const
{
  NS_LOG_FUNCTION (this << i);
  Ptr<Ipv4Interface> interface = GetInterface (i);
  NS_LOG_LOGIC ("Forwarding state: " << interface->IsForwarding ());
  return interface->IsForwarding ();
}

void 
Ipv4L3Protocol::SetForwarding (uint32_t i, bool val)
{
  NS_LOG_FUNCTION (this << i);
  Ptr<Ipv4Interface> interface = GetInterface (i);
  interface->SetForwarding (val);
}

Ptr<NetDevice>
Ipv4L3Protocol::GetNetDevice (uint32_t i)
{
  NS_LOG_FUNCTION (this << i);
  return GetInterface (i)->GetDevice ();
}

void 
Ipv4L3Protocol::SetIpForward (bool forward) 
{
  NS_LOG_FUNCTION (this << forward);
  m_ipForward = forward;
  for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); i != m_interfaces.end (); i++)
    {
      (*i)->SetForwarding (forward);
    }
}

bool 
Ipv4L3Protocol::GetIpForward (void) const
{
  NS_LOG_FUNCTION (this);
  return m_ipForward;
}

void 
Ipv4L3Protocol::SetWeakEsModel (bool model)
{
  NS_LOG_FUNCTION (this << model);
  m_weakEsModel = model;
}

bool 
Ipv4L3Protocol::GetWeakEsModel (void) const
{
  NS_LOG_FUNCTION (this);
  return m_weakEsModel;
}

void
Ipv4L3Protocol::RouteInputError (Ptr<const Packet> p, const Ipv4Header & ipHeader, Socket::SocketErrno sockErrno)
{
  NS_LOG_FUNCTION (this << p << ipHeader << sockErrno);
  NS_LOG_LOGIC ("Route input failure-- dropping packet to " << ipHeader << " with errno " << sockErrno); 
  m_dropTrace (ipHeader, p, DROP_ROUTE_ERROR, m_node->GetObject<Ipv4> (), 0);
}

void
Ipv4L3Protocol::DoFragmentation (Ptr<Packet> packet, uint32_t outIfaceMtu, std::list<Ptr<Packet> >& listFragments)
{
  // BEWARE: here we do assume that the header options are not present.
  // a much more complex handling is necessary in case there are options.
  // If (when) IPv4 option headers will be implemented, the following code shall be changed.
  // Of course also the reassemby code shall be changed as well.

  NS_LOG_FUNCTION (this << *packet << outIfaceMtu << &listFragments);

  Ptr<Packet> p = packet->Copy ();

  Ipv4Header ipv4Header;
  p->RemoveHeader (ipv4Header);

  NS_ASSERT_MSG( (ipv4Header.GetSerializedSize() == 5*4),
                 "IPv4 fragmentation implementation only works without option headers." );

  uint16_t offset = 0;
  bool moreFragment = true;
  uint16_t originalOffset = 0;
  bool alreadyFragmented = false;
  uint32_t currentFragmentablePartSize = 0;

  if (!ipv4Header.IsLastFragment())
    {
      alreadyFragmented = true;
      originalOffset = ipv4Header.GetFragmentOffset();
    }

  // IPv4 fragments are all 8 bytes aligned but the last.
  // The IP payload size is:
  // floor( ( outIfaceMtu - ipv4Header.GetSerializedSize() ) /8 ) *8
  uint32_t fragmentSize = (outIfaceMtu - ipv4Header.GetSerializedSize () ) & ~uint32_t (0x7);

  NS_LOG_LOGIC ("Fragmenting - Target Size: " << fragmentSize );

  do
    {
      Ipv4Header fragmentHeader = ipv4Header;

      if (p->GetSize () > offset + fragmentSize )
        {
          moreFragment = true;
          currentFragmentablePartSize = fragmentSize;
          fragmentHeader.SetMoreFragments ();
        }
      else
        {
          moreFragment = false;
          currentFragmentablePartSize = p->GetSize () - offset;
          if (alreadyFragmented)
            {
              fragmentHeader.SetMoreFragments ();
            }
          else
            {
              fragmentHeader.SetLastFragment ();
            }
        }

      NS_LOG_LOGIC ("Fragment creation - " << offset << ", " << currentFragmentablePartSize  );
      Ptr<Packet> fragment = p->CreateFragment (offset, currentFragmentablePartSize);
      NS_LOG_LOGIC ("Fragment created - " << offset << ", " << fragment->GetSize ()  );

      fragmentHeader.SetFragmentOffset (offset+originalOffset);
      fragmentHeader.SetPayloadSize (currentFragmentablePartSize);

      if (Node::ChecksumEnabled ())
        {
          fragmentHeader.EnableChecksum ();
        }

      NS_LOG_LOGIC ("Fragment check - " << fragmentHeader.GetFragmentOffset ()  );

      NS_LOG_LOGIC ("New fragment Header " << fragmentHeader);
      fragment->AddHeader (fragmentHeader);

      std::ostringstream oss;
      fragment->Print (oss);

      NS_LOG_LOGIC ("New fragment " << *fragment);

      listFragments.push_back (fragment);

      offset += currentFragmentablePartSize;

    }
  while (moreFragment);

  return;
}

bool
Ipv4L3Protocol::ProcessFragment (Ptr<Packet>& packet, Ipv4Header& ipHeader, uint32_t iif)
{
  NS_LOG_FUNCTION (this << packet << ipHeader << iif);

  uint64_t addressCombination = uint64_t (ipHeader.GetSource ().Get ()) << 32 | uint64_t (ipHeader.GetDestination ().Get ());
  uint32_t idProto = uint32_t (ipHeader.GetIdentification ()) << 16 | uint32_t (ipHeader.GetProtocol ());
  std::pair<uint64_t, uint32_t> key;
  bool ret = false;
  Ptr<Packet> p = packet->Copy ();

  key.first = addressCombination;
  key.second = idProto;

  Ptr<Fragments> fragments;

  MapFragments_t::iterator it = m_fragments.find (key);
  if (it == m_fragments.end ())
    {
      fragments = Create<Fragments> ();
      m_fragments.insert (std::make_pair (key, fragments));
      m_fragmentsTimers[key] = Simulator::Schedule (m_fragmentExpirationTimeout,
                                                    &Ipv4L3Protocol::HandleFragmentsTimeout, this,
                                                    key, ipHeader, iif);
    }
  else
    {
      fragments = it->second;
    }

  NS_LOG_LOGIC ("Adding fragment - Size: " << packet->GetSize ( ) << " - Offset: " << (ipHeader.GetFragmentOffset ()) );

  fragments->AddFragment (p, ipHeader.GetFragmentOffset (), !ipHeader.IsLastFragment () );

  if ( fragments->IsEntire () )
    {
      packet = fragments->GetPacket ();
      fragments = 0;
      m_fragments.erase (key);
      if (m_fragmentsTimers[key].IsRunning ())
        {
          NS_LOG_LOGIC ("Stopping WaitFragmentsTimer at " << Simulator::Now ().GetSeconds () << " due to complete packet");
          m_fragmentsTimers[key].Cancel ();
        }
      m_fragmentsTimers.erase (key);
      ret = true;
    }

  return ret;
}

Ipv4L3Protocol::Fragments::Fragments ()
  : m_moreFragment (0)
{
  NS_LOG_FUNCTION (this);
}

Ipv4L3Protocol::Fragments::~Fragments ()
{
  NS_LOG_FUNCTION (this);
}

void
Ipv4L3Protocol::Fragments::AddFragment (Ptr<Packet> fragment, uint16_t fragmentOffset, bool moreFragment)
{
  NS_LOG_FUNCTION (this << fragment << fragmentOffset << moreFragment);

  std::list<std::pair<Ptr<Packet>, uint16_t> >::iterator it;

  for (it = m_fragments.begin (); it != m_fragments.end (); it++)
    {
      if (it->second > fragmentOffset)
        {
          break;
        }
    }

  if (it == m_fragments.end ())
    {
      m_moreFragment = moreFragment;
    }

  m_fragments.insert (it, std::pair<Ptr<Packet>, uint16_t> (fragment, fragmentOffset));
}

bool
Ipv4L3Protocol::Fragments::IsEntire () const
{
  NS_LOG_FUNCTION (this);

  bool ret = !m_moreFragment && m_fragments.size () > 0;

  if (ret)
    {
      uint16_t lastEndOffset = 0;

      for (std::list<std::pair<Ptr<Packet>, uint16_t> >::const_iterator it = m_fragments.begin (); it != m_fragments.end (); it++)
        {
          // overlapping fragments do exist
          NS_LOG_LOGIC ("Checking overlaps " << lastEndOffset << " - " << it->second );

          if (lastEndOffset < it->second)
            {
              ret = false;
              break;
            }
          // fragments might overlap in strange ways
          uint16_t fragmentEnd = it->first->GetSize () + it->second;
          lastEndOffset = std::max ( lastEndOffset, fragmentEnd );
        }
    }

  return ret;
}

Ptr<Packet>
Ipv4L3Protocol::Fragments::GetPacket () const
{
  NS_LOG_FUNCTION (this);

  std::list<std::pair<Ptr<Packet>, uint16_t> >::const_iterator it = m_fragments.begin ();

  Ptr<Packet> p = it->first->Copy ();
  uint16_t lastEndOffset = p->GetSize ();
  it++;

  for ( ; it != m_fragments.end (); it++)
    {
      if ( lastEndOffset > it->second )
        {
          // The fragments are overlapping.
          // We do not overwrite the "old" with the "new" because we do not know when each arrived.
          // This is different from what Linux does.
          // It is not possible to emulate a fragmentation attack.
          uint32_t newStart = lastEndOffset - it->second;
          if ( it->first->GetSize () > newStart )
            {
              uint32_t newSize = it->first->GetSize () - newStart;
              Ptr<Packet> tempFragment = it->first->CreateFragment (newStart, newSize);
              p->AddAtEnd (tempFragment);
            }
        }
      else
        {
          NS_LOG_LOGIC ("Adding: " << *(it->first) );
          p->AddAtEnd (it->first);
        }
      lastEndOffset = p->GetSize ();
    }

  return p;
}

Ptr<Packet>
Ipv4L3Protocol::Fragments::GetPartialPacket () const
{
  NS_LOG_FUNCTION (this);
  
  std::list<std::pair<Ptr<Packet>, uint16_t> >::const_iterator it = m_fragments.begin ();

  Ptr<Packet> p = Create<Packet> ();
  uint16_t lastEndOffset = 0;

  if ( m_fragments.begin ()->second > 0 )
    {
      return p;
    }

  for ( it = m_fragments.begin (); it != m_fragments.end (); it++)
    {
      if ( lastEndOffset > it->second )
        {
          uint32_t newStart = lastEndOffset - it->second;
          uint32_t newSize = it->first->GetSize () - newStart;
          Ptr<Packet> tempFragment = it->first->CreateFragment (newStart, newSize);
          p->AddAtEnd (tempFragment);
        }
      else if ( lastEndOffset == it->second )
        {
          NS_LOG_LOGIC ("Adding: " << *(it->first) );
          p->AddAtEnd (it->first);
        }
      lastEndOffset = p->GetSize ();
    }

  return p;
}

void
Ipv4L3Protocol::HandleFragmentsTimeout (std::pair<uint64_t, uint32_t> key, Ipv4Header & ipHeader, uint32_t iif)
{
  NS_LOG_FUNCTION (this << &key << &ipHeader << iif);

  MapFragments_t::iterator it = m_fragments.find (key);
  Ptr<Packet> packet = it->second->GetPartialPacket ();

  // if we have at least 8 bytes, we can send an ICMP.
  if ( packet->GetSize () > 8 )
    {
      Ptr<Icmpv4L4Protocol> icmp = GetIcmp ();
      icmp->SendTimeExceededTtl (ipHeader, packet);
    }
  m_dropTrace (ipHeader, packet, DROP_FRAGMENT_TIMEOUT, m_node->GetObject<Ipv4> (), iif);

  // clear the buffers
  it->second = 0;

  m_fragments.erase (key);
  m_fragmentsTimers.erase (key);
}
} // namespace ns3
