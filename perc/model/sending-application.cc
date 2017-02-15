/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Copyright (c) 2016 Stanford University
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
// Author: Lavanya Jose (lavanyaj@cs.stanford.edu)
//

// ns3 - Sending Data Source Application class
// 
// Adapted from On/Off Application in ns3 and SendingApplication in xfabric

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "sending-application.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/pointer.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SendingApplication");

NS_OBJECT_ENSURE_REGISTERED (SendingApplication);

TypeId
SendingApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SendingApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<SendingApplication> ()    
    .AddAttribute ("InitialDataRate", "The data rate in on state.",
                   DataRateValue (DataRate ("500kb/s")),
                   MakeDataRateAccessor (&SendingApplication::m_dataRate),
                   MakeDataRateChecker ())
    .AddAttribute ("PacketSize", "The size of packets sent in on state",
                   UintegerValue (512),
                   MakeUintegerAccessor (&SendingApplication::m_pktSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Local",
                   "The Address on which to Bind the tx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&SendingApplication::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&SendingApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("MaxBytes", 
                   "The total number of bytes to send. Once these bytes are sent, "
                   "no packet is sent again, even in on state. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&SendingApplication::m_maxBytes),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&SendingApplication::m_tid),
                   MakeTypeIdChecker ())
    .AddAttribute("HighPriority", "True if all packets sent are high priority",
                  BooleanValue(false),
                  MakeBooleanAccessor (&SendingApplication::m_hiPrio),
                  MakeBooleanChecker())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&SendingApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}


SendingApplication::SendingApplication ()
  : m_socket (0),
    m_connected (false),
    m_residualBits (0),
    m_lastStartTime (Seconds (0)),
    m_totBytes (0)
{
  NS_LOG_FUNCTION (this);
}

SendingApplication::~SendingApplication()
{
  NS_LOG_FUNCTION (this);
}

void 
SendingApplication::SetMaxBytes (uint64_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

Ptr<Socket>
SendingApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

int64_t 
SendingApplication::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  // No random variables are used by this sending application
  // m_onTime->SetStream (stream);
  // m_offTime->SetStream (stream + 1);
  return 0;
}

void
SendingApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void SendingApplication::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // >>> for i in range(64):
  // ...     if ((i & 0x1e) >> 1) == 8:
  // ...         print i
  // ... 
  // 16
  // 17
  // 48
  // 49

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      // TCP Bind and Connect overwrite IpTos ?? http://www.mehic.info/2016/11/socketsetiptos-in-ns-3-ver-3-26/
      if (m_hiPrio) m_socket->SetIpTos(16);

      if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind6 ();
        }
      else if (InetSocketAddress::IsMatchingType (m_peer) ||
               PacketSocketAddress::IsMatchingType (m_peer))
        {
          if (!m_local.IsInvalid())
            m_socket->Bind (m_local);
          else
            m_socket->Bind();
        }
      m_socket->Connect (m_peer);
      // TCP Bind and Connect overwrite IpTos ?? http://www.mehic.info/2016/11/socketsetiptos-in-ns-3-ver-3-26/
      if (m_hiPrio) m_socket->SetIpTos(16);

      m_socket->SetAllowBroadcast (true);
      m_socket->ShutdownRecv ();

      // TODO(lav): set this callback before calling connect!?
      m_socket->SetConnectCallback (
        MakeCallback (&SendingApplication::ConnectionSucceeded, this),
        MakeCallback (&SendingApplication::ConnectionFailed, this));
    }
  m_dataRateFailSafe = m_dataRate;
  // Insure no pending event TODO(lav): why??
  CancelEvents ();

  // If we are not yet connected, there is nothing to do here
  // The ConnectionComplete upcall will start timers at that time
  //if (!m_connected) return;
  // ScheduleStartEvent ();
}

void SendingApplication::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  CancelEvents ();
  if(m_socket != 0)
    {
      m_socket->Close ();
    }
  else
    {
      NS_LOG_WARN ("SendingApplication found null socket to close in StopApplication");
    }
}

void SendingApplication::CancelEvents ()
{
  NS_LOG_FUNCTION (this);

  if (m_sendEvent.IsRunning () && m_dataRateFailSafe == m_dataRate )
    { // Cancel the pending send packet event
      // Calculate residual bits since last packet sent
      Time delta (Simulator::Now () - m_lastStartTime);
      int64x64_t bits = delta.To (Time::S) * m_dataRate.GetBitRate ();
      m_residualBits += bits.GetHigh ();
    }
  m_dataRateFailSafe = m_dataRate;
  Simulator::Cancel (m_sendEvent);
}


// Private helpers
void SendingApplication::ScheduleNextTx ()
{
  NS_LOG_FUNCTION (this);

  if (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
      uint32_t bits = m_pktSize * 8 - m_residualBits;
      NS_LOG_LOGIC ("bits = " << bits);
      Time nextTime (Seconds (bits /
                              static_cast<double>(m_dataRate.GetBitRate ()))); // Time till next packet
      NS_LOG_LOGIC ("nextTime = " << nextTime);
      m_sendEvent = Simulator::Schedule (nextTime,
                                         &SendingApplication::SendPacket, this);
    }
  else
    { // All done, cancel any pending events
      StopApplication ();
    }
}

void SendingApplication::SendPacket ()
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_connected);
  NS_ASSERT (m_sendEvent.IsExpired ());

  Ptr<Packet> packet = Create<Packet> (m_pktSize);
  m_txTrace (packet);
  m_socket->Send (packet);
  m_totBytes += m_pktSize;
  if (InetSocketAddress::IsMatchingType (m_peer))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                   << "s on-off application sent "
                   <<  packet->GetSize () << " bytes to "
                   << InetSocketAddress::ConvertFrom(m_peer).GetIpv4 ()
                   << " port " << InetSocketAddress::ConvertFrom (m_peer).GetPort ()
                   << " total Tx " << m_totBytes << " bytes");
    }
  else if (Inet6SocketAddress::IsMatchingType (m_peer))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                   << "s on-off application sent "
                   <<  packet->GetSize () << " bytes to "
                   << Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6 ()
                   << " port " << Inet6SocketAddress::ConvertFrom (m_peer).GetPort ()
                   << " total Tx " << m_totBytes << " bytes");
    }
  m_lastStartTime = Simulator::Now ();
  m_residualBits = 0;
  ScheduleNextTx ();
}


void SendingApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_ASSERT (!m_connected);
  m_connected = true;

  if (m_maxBytes == 0 || m_totBytes < m_maxBytes) {
    m_connectTime = Simulator::Now();
    NS_LOG_LOGIC ("start at " << m_connectTime);
    m_sendEvent = Simulator::Schedule (m_connectTime,
                                       &SendingApplication::SendPacket, this);
  }
}

void SendingApplication::ChangeRate(DataRate rate) {
  NS_LOG_FUNCTION (this);
  m_dataRate = rate;
}

void SendingApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}


} // Namespace ns3
