/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 Stanford University
 *
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
 * Author: Lavanya Jose (lavanyaj@cs.stanford.edu)
 * Based on xfabric experiments at https://bitbucket.org/knagaraj/numfabric
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "convergence-experiments.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ConvergenceExperiments");

void
TcPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "TcPacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

void
DevicePacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "DevicePacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

void ConvergenceExperiments::run() {
  createTopology();
  setupApps();
  setConfigDefaults();
  setupFlowMonitor();
  
  Simulator::Stop (Seconds (simulationTime + 5));
  Simulator::Run ();
  // Not sure what 1 flow is, maybe distinguished by 5 tuple
  printExperimentStats();  
  // Note that this also destroys all queues of ptpnd (but not qdiscs)
  Simulator::Destroy ();
}

void ConvergenceExperiments::parseCmdConfig(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, Udp", transportProt);
  cmd.Parse (argc, argv);
}

ConvergenceExperiments::ConvergenceExperiments() {
}

void ConvergenceExperiments::createTopology() {  
  if (transportProt.compare ("Tcp") == 0)
    {
      socketType = "ns3::TcpSocketFactory";
    }
  else
    {
      socketType = "ns3::UdpSocketFactory";
    }
  hosts.Create (4);
  leafnodes.Create (1);
  
  PointToPointHelper edgep2p;
  edgep2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  edgep2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  edgep2p.SetQueue ("ns3::DropTailQueue", "Mode", StringValue ("QUEUE_MODE_PACKETS"), "MaxPackets", UintegerValue (1));

  for (uint32_t hostno = 0; hostno < hosts.GetN(); hostno++) {
    // install will return a container comprising
    // 1) the net device installed at host towards leafnode !! So edgedevices.Get(l).Get(0) to get NIC
    // 2) the net device installed at leafnode towards host
    edgedevices.Add(edgep2p.Install (hosts.Get(hostno), leafnodes.Get(0)));
  }

 
  // NetDeviceContainer nics; // nics[i] is interface at host i towards leafnode
  // NetDeviceContainer tordowns; // interface at leafnode towards host
  // for (uint32_t edgedeviceno = 0; edgedeviceno < edgedevices.GetN(); edgedeviceno++) {
  //   if (edgedeviceno % 2 == 0)
  //     nics.Add(edgedevices.Get(edgedeviceno));
  //   else
  //     tordowns.Add(edgedevices.Get(edgedeviceno));
  // }
  
  // config up link and down link for each link
  //  (edgedevice[l].Get(0) and .Get(1) for each l)
  // TODO

  // assign IP address for each link
  // TODO
  InternetStackHelper stack;
  stack.Install(hosts);
  stack.Install(leafnodes);

  // TODO: IP -> L2 for downlinks and IP -> TC -> L2 for uplinks
  
  // config down link queues for each link?
  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  qdiscs = tch.Install (edgedevices);
  // can also uninstall after assigning IP address, see traffic-control.cc.1
  
  // TODO: Queue discs everywhere even switch links (maybe use this for control/ data)
  for (uint32_t edgedeviceno = 0; edgedeviceno < edgedevices.GetN(); edgedeviceno++) {
    // This will segfault if you didn't add an internal queue and root queue disc
    Ptr<QueueDisc> q = qdiscs.Get (edgedeviceno);
    q->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&TcPacketsInQueueTrace));
    // Alternatively:
    // Config::ConnectWithoutContext ("/NodeList/1/$ns3::TrafficControlLayer/RootQueueDiscList/0/PacketsInQueue",
    //                                MakeCallback (&TcPacketsInQueueTrace));
  }

  // Queue at all interfaces
  for (uint32_t edgedeviceno = 0; edgedeviceno < edgedevices.GetN(); edgedeviceno++) {
    // TODO what if .Get(0)
    // This will segfault if you didn't add a root queue disc (?)
    Ptr<NetDevice> nd = edgedevices.Get (edgedeviceno);
    Ptr<PointToPointNetDevice> ptpnd = DynamicCast<PointToPointNetDevice> (nd);
    Ptr<Queue> queue = ptpnd->GetQueue ();
    NS_ASSERT(queue);
    queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&DevicePacketsInQueueTrace));
  }

  // All interfaces have address
  for (uint32_t edgedeviceno = 0; edgedeviceno < edgedevices.GetN(); edgedeviceno++) {
    // link l has base 10.1.l.0, copying Kanthi's format
    Ipv4AddressHelper address;
    std::ostringstream subnet; subnet << "10.1."<<edgedeviceno<<".0";
    address.SetBase (subnet.str().c_str(), "255.255.255.0");
    // assign returns two interface addrs for
    //  the net device from host to leaf
    //  and the netdevice from leaf to host
    // index into downlink to host i using i*2+ 1 (i from 0, ..)
    interfaces.Add(address.Assign (edgedevices.Get(edgedeviceno)));
  }
  //tch.Uninstall (tordowns);
  // Turn on global static routing, copying from K
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
}

void ConvergenceExperiments::setupApps() {
    // I want 3 hosts sending to one until convergence/ max time
  // each flow from a host starts from a new port at src host
  // and sends to a fixed port at dst host (7)
  // packetSinkHelper address and port at construct used for what!?
  
  uint32_t incastdstno = hosts.GetN()-1;
  
  // Install packet sink at incast destination host  
  uint16_t port = 7;
  Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper (socketType, localAddress);
  sinkApp = packetSinkHelper.Install(hosts.Get (incastdstno));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 0.1));    

  // Install packet generator at all other hosts
  AddressValue remoteAddress (InetSocketAddress (interfaces.GetAddress (incastdstno*2), port));


  for (uint32_t hostno = 0; hostno < hosts.GetN(); hostno++) {
    if (hostno == incastdstno) continue;
    // I think need a separate helper for each flow since
    // TCP source address is going to be the IP address supplied here (?)
    // And TCP source port is going to be ??
    OnOffHelper onoff (socketType, Ipv4Address::GetAny ());
    onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    onoff.SetAttribute ("DataRate", StringValue ("50Mbps")); //bit/s
    if (hostno == 0)
      onoff.SetAttribute ("HighPriority", BooleanValue (true)); //bit/s
    // Remote attribute bound to m_peer, used when creating socket
    onoff.SetAttribute ("Remote", remoteAddress);
    apps.Add (onoff.Install (hosts.Get (hostno)));
    apps.Start (Seconds (1.0));
    apps.Stop (Seconds (simulationTime + 0.1));

  }
  //Flow
  // uint16_t port = 7;
  // Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  // // application that receives PacketSinkHelper: public Application TODO: lav adapt this?
  // PacketSinkHelper packetSinkHelper (socketType, localAddress);
  // ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (0));

  // sinkApp.Start (Seconds (0.0));
  // sinkApp.Stop (Seconds (simulationTime + 0.1));


  // QUESTION: three IP addresses?on for sinkApp one for contructing onoff app and remote for onoff
  // I think the contructor address is ignored (basically used when sending packets)
  // OnOffHelper onoff (socketType, Ipv4Address::GetAny ());
  // onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  // onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  // onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  // onoff.SetAttribute ("DataRate", StringValue ("50Mbps")); //bit/s
  // ApplicationContainer apps;

  // Remote attribute bound to m_peer in onoff application used when creating socket
  // AddressValue remoteAddress (InetSocketAddress (interfaces.GetAddress (0), port));
  // onoff.SetAttribute ("Remote", remoteAddress);
  // apps.Add (onoff.Install (nodes.Get (1)));
  // apps.Start (Seconds (1.0));
  // apps.Stop (Seconds (simulationTime + 0.1));
}

void ConvergenceExperiments::printExperimentStats() {
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    std::cout << "Flow " << i->first - 2 << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    
    std::cout << "  Tx Packets:   " << i->second.txPackets << std::endl;
    std::cout << "  Tx Bytes:   " << i->second.txBytes << std::endl;
    std::cout << "  Offered Load: " << i->second.txBytes * 8.0 / (i->second.timeLastTxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
    std::cout << "  Rx Packets:   " << i->second.rxPackets << std::endl;
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << std::endl;

    // This segfaults maybe cuz there's many queue discs, one per node?
    // std::cout << "  Packets Dropped by Queue Disc:   " << i->second.packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC] << std::endl;
    // std::cout << "  Bytes Dropped by Queue Disc:   " << i->second.bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC] << std::endl;
    // std::cout << "  Packets Dropped by NetDevice:   " << i->second.packetsDropped[Ipv4FlowProbe::DROP_QUEUE] << std::endl;
    // std::cout << "  Bytes Dropped by NetDevice:   " << i->second.bytesDropped[Ipv4FlowProbe::DROP_QUEUE] << std::endl;
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
    std::cout << "  Mean delay:   " << i->second.delaySum.GetSeconds () / i->second.rxPackets << std::endl;
    std::cout << "  Mean jitter:   " << i->second.jitterSum.GetSeconds () / (i->second.rxPackets - 1) << std::endl;
  }

  std::cout << std::endl << "*** Application statistics ***" << std::endl;
  double thr = 0;
  uint32_t totalPacketsThr = DynamicCast<PacketSink> (sinkApp.Get (0))->GetTotalRx ();
  thr = totalPacketsThr * 8 / (simulationTime * 1000000.0); //Mbit/s
  std::cout << "  Rx Bytes: " << totalPacketsThr << std::endl;
  std::cout << "  Average Goodput: " << thr << " Mbit/s" << std::endl;

  std::cout << std::endl << "*** TC Layer statistics ***" << std::endl;

  for (uint32_t  edgedeviceno = 0; edgedeviceno < edgedevices.GetN(); edgedeviceno++) {
    std::cout << "At edge device num " << edgedeviceno << std::endl;
      Ptr<QueueDisc> q = qdiscs.Get (edgedeviceno);
      if (q) {        
        std::cout << "  Packets dropped by the TC layer: " << q->GetTotalDroppedPackets () << std::endl;
        std::cout << "  Bytes dropped by the TC layer: " << q->GetTotalDroppedBytes () << std::endl;
        std::cout << "  Packets requeued by the TC layer: " << q->GetTotalRequeuedPackets () << std::endl;
      } else {
        std::cout << "  Couldn't access the TC layer for edgedevice no " << edgedeviceno << std::endl;
      }
    
    Ptr<NetDevice> nd = edgedevices.Get (edgedeviceno);
    Ptr<PointToPointNetDevice> ptpnd = DynamicCast<PointToPointNetDevice> (nd);
    Ptr<Queue> queue = ptpnd->GetQueue ();
    if (queue)
      std::cout << "  Packets dropped by the netdevice: " << queue->GetTotalDroppedPackets () << std::endl;
    else
      std::cout << "  Couldn't access queue for edgedevice no " << edgedeviceno << std::endl;
  }
}


void ConvergenceExperiments::setConfigDefaults() {
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));
}

void ConvergenceExperiments::setupFlowMonitor() {
  monitor = flowmon.InstallAll();
}
