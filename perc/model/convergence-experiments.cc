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

#include <iostream>
#include <sstream>
#include <map>
#include <utility> // pair
#include <vector>
#include <list>
#include <unordered_set>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "../helper/sending-helper.h"
#include "sending-application.h"
#include "convergence-experiments.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ConvergenceExperiments");

void
TcPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  //std::cout << "TcPacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

void
DevicePacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  //std::cout << "DevicePacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

void ConvergenceExperiments::run() {
  NS_LOG_FUNCTION (this);
  createTopology();
  if (event_list.size() > 0) startNextEpoch(false);
  setConfigDefaults();
  setupFlowMonitor();
  
  Simulator::Stop (Seconds (simulationTime + 5));
  Simulator::Run ();
  printExperimentStatsForWorkload();  
  // Note that this also destroys all queues of ptpnd (but not qdiscs)
  Simulator::Destroy ();
}

void ConvergenceExperiments::parseCmdConfig(int argc, char *argv[]) {
  NS_LOG_FUNCTION (this);
  CommandLine cmd;
  cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, Udp", transportProt);
  cmd.Parse (argc, argv);
}

ConvergenceExperiments::ConvergenceExperiments() {
  NS_LOG_FUNCTION (this);
}

ConvergenceExperiments::ConvergenceExperiments(
                                               const std::string& flow_arrivals_filename,
                                               const std::string& flow_departures_filename,
                                               const std::string& events_filename,
                                               const std::string& flows_filename,
                                               const std::string& opt_rates_filename) :
  flow_arrivals_filename(flow_arrivals_filename), flow_departures_filename(flow_departures_filename), events_filename(events_filename), flows_filename(flows_filename), opt_rates_filename(opt_rates_filename) {
  NS_LOG_FUNCTION (this);
  loadWorkloadFromFiles();
  showWorkloadFromFiles();
}

void ConvergenceExperiments::createTopology() {
  NS_LOG_FUNCTION (this);
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
    //  NOTE!! Ipv4 address of NIC at host h = interfaces.GetAddress(host*2);
    //  TODO(lav): make function to get this, since it depends on topology setup
    interfaces.Add(address.Assign (edgedevices.Get(edgedeviceno)));
  }
  //tch.Uninstall (tordowns);
  // Turn on global static routing, copying from K
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
}

void ConvergenceExperiments::stopApp(uint32_t flow_id) {
  NS_LOG_FUNCTION (this << flow_id);
  uint32_t app_index = flowToAppIndex.at(flow_id);
  // since StopApplication is private I have to put app in container and call stop
  ApplicationContainer sending_app(sending_apps.Get(app_index));
  //sending_app.Stop(Seconds(0.0));
  //ApplicationContainer sink_app(sink_apps.Get(app_index));
  //sink_app.Stop(Seconds(0.0));
  // maybe remove the sink_apps object also
}

// will add a new app/ sink app for given flow info
void ConvergenceExperiments::startApp(uint16_t source_port, uint16_t destination_port,
                                      uint32_t source_host, uint32_t destination_host, Time start_time) {
  NS_LOG_FUNCTION (this << source_port << destination_port << source_host << destination_host << start_time);
  // Install packet sink at destination host    
  Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), destination_port));
  PacketSinkHelper packetSinkHelper (socketType, localAddress);

  // apparently packetSinkHelper.Install returns ApplicationContainer
  ApplicationContainer sink_app = packetSinkHelper.Install(hosts.Get (destination_host));
  sink_apps.Add(sink_app);
  sink_app.Start(start_time);

  // Install packet generator at all other hosts
  AddressValue remoteAddress (InetSocketAddress (interfaces.GetAddress (destination_host*2), destination_port));

  localAddress =  Address(InetSocketAddress (interfaces.GetAddress (source_host*2), source_port));

  SendingHelper sending (socketType, remoteAddress);
  sending.SetAttribute ("MaxBytes", UintegerValue (0)); //10 * payloadSize));
  sending.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  sending.SetAttribute ("InitialDataRate", StringValue ("5Mbps")); //bit/s
  sending.SetAttribute ("Local", AddressValue(localAddress)); //bit/s
  //if (hostno == 0)
  //  sending.SetAttribute ("HighPriority", BooleanValue (true)); //bit/s
  // Remote attribute bound to m_peer, used when creating socket
  // And sending.Install returns ApplicationContainer
  ApplicationContainer sending_app = sending.Install (hosts.Get (source_host));
  sending_apps.Add(sending_app);  
  sending_app.Start(start_time);
}

// Gets next event, schedules start or stop flows events and reschedules itself
// note that checkRates might re-reschedule the next startNextEpoch if flows
// converge early
void ConvergenceExperiments::startNextEpoch(bool converged_in_previous) {
  NS_LOG_FUNCTION (this);
  
  // NS_ASSERT(event_list.size() > 0);
  bool start_flows_next = event_list.front();
  event_list.pop_front();
  NS_ASSERT(next_epoch > 0);
  if (start_flows_next) {
    // vector, indexing start at 0
    const auto& flows = flows_to_start.at(flows_to_start_next++);    
    for (const auto& f : flows) {
      uint32_t source = all_flows.at(f).first;
      uint32_t destination = all_flows.at(f).second;
      uint16_t source_port = all_flows_ports.at(f).first;
      uint16_t destination_port = all_flows_ports.at(f).second;
      // for logging
      Ipv4Address source_address = interfaces.GetAddress(source*2);
      Ipv4Address destination_address = interfaces.GetAddress(destination*2);
      Ipv4FlowClassifier::FiveTuple t =
        { source_address, destination_address, 6, source_port, destination_port};
      std::cout << "Starting flow # " << f << ": "
                << " sourceAddress=" << t.sourceAddress << "/"
                << " destinationAddress=" << t.destinationAddress << "/"
                << " protocol=" << t.protocol << "/"
                << " sourcePort=" << t.sourcePort << "/"
                << " destinationPort=" << t.destinationPort
                << " at time " << Simulator::Now() << "\n";
      startApp(source_port, destination_port, source, destination, Simulator::Now());
      // NS_ASSERT(flowToAppIndex.find(f) == flowToAppIndex::end)
      flowToAppIndex[f] = sending_apps.GetN()-1;      
      fiveTupleToFlow[t] = f;
      flowToFiveTuple[f] = t;
      active_flows.insert(f);
      std::cout << "inserted flow " << f << " in active_flows.\n";
      // NS_ASSERT(sending_apps.GetN() == sink_apps.GetN())
    }
  } else {
    // vector, indexing start at 0
    const auto& flows = flows_to_stop.at(flows_to_stop_next++);
    for (const auto& f : flows) {
      stopApp(f);
      NS_ASSERT(active_flows.find(f) != active_flows.end());
      active_flows.erase(f);
      std::cout << "removed flow " << f << " from active_flows.\n";
    }    
  }
  last_epoch_time = Simulator::Now(); // TODO(lav): not used?
  
  if (event_list.size() > 0) {
    std::cout << "scheduling next_epoch_event from startNextEpoch in " << max_epoch_seconds.GetSeconds() << "s.\n";
    next_epoch_event = Simulator::Schedule(max_epoch_seconds,
                                           &ConvergenceExperiments::startNextEpoch, this, false);
  }
  // increment next_epoch since we just added/ removed flows
  // for a new epoch
  
  next_epoch++;
  std::cout << "next_epoch is now " << next_epoch << (converged_in_previous ? ", didn't converge" : ", converged")
            << " in previous epoch. \n";
   // else {
  //   Simulator::Stop();
  // }

  // start checking rates right after adding (check) flows
  // in first epoch
  if (true and next_epoch == 2) {
    std::cout << "scheduling checkRates.\n";
    check_rates_event =
      Simulator::Schedule(
                          sampling_interval,
                          &ConvergenceExperiments::checkRates, this);
  }

}

// similar to xfabric's CheckIpv4 rates except we iterate
// through FlowMonitor's flows
void ConvergenceExperiments::checkRates() {
  // std::cout << Simulator::Now().GetSeconds()
  //           << " in checkRates\n";
  
  double current_total_rate = 0.0;
  std::vector<double> small_errors;
  std::vector<double> other_errors;

  Ptr<Ipv4FlowClassifier> classifier =
   DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  // std::map<FlowId, FlowMonitor::FlowStats> stats =
  //   monitor->GetFlowStats ();

  //  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
  for (const auto& flow_id : active_flows) {
    FlowId fm_index;// = ((FlowId) flow_id+1);
        bool found = false;
    if (flowToFlowMonitorIndex.find(flow_id)
        != flowToFlowMonitorIndex.end()) {
      fm_index = flowToFlowMonitorIndex.at(flow_id);
      found = true;
    } else if (flowToFiveTuple.find(flow_id)
               != flowToFiveTuple.end()
               and classifier->FindFlowId(flowToFiveTuple.at(flow_id), fm_index)) {
      const auto ret =
        flowToFlowMonitorIndex.insert(
              std::make_pair(flow_id, fm_index));
      NS_ASSERT(ret.second);
      found = true;
    }
    if (!found) continue;
    //std::cout << "found flow " << flow_id << " in flow monitor\n";
    //NS_ASSERT(stats.find(fm_index) != stats.end());
    //const FlowMonitor::FlowStats& flow_stats = stats.at(fm_index);
    // Mb/s
    //std::cout << "Finding rates for flow " << flow_id << " in epoch " << next_epoch-1 << std::endl;
    double optimal_rate = 0;
    auto const& opt_it = opt_rates.at(next_epoch-1).find(flow_id);
    if (opt_it != opt_rates.at(next_epoch-1).end())
      optimal_rate = opt_it->second;
    double stats_rate = monitor->getEwmaRate(fm_index);
    double measured_rate = stats_rate / 1000000.0;
    double error = abs(optimal_rate - measured_rate);

    // std::cout << "Flow " << flow_id << " has measured rate " << measured_rate << " Mbps"
    //           << " and expected rate " << optimal_rate << " Mbps.\n";
    NS_ASSERT(optimal_rate > 0);
    if (error < 0.1 * optimal_rate) small_errors.push_back(error);
    else  other_errors.push_back(error);
    
    current_total_rate += measured_rate;    
  }
  uint32_t total_flows = small_errors.size()+other_errors.size();
  if (small_errors.size() >= 0.95 * total_flows) {
    ninety_fifth_converged++;
  } else {
    ninety_fifth_converged = 0;
  }

  if (ninety_fifth_converged > max_iterations_of_goodness) {
    ninety_fifth_converged = 0;
    if (next_epoch_event.IsRunning()) {
      std::cout << "Next epoch event is running, cancel.\n";
      next_epoch_event.Cancel();
    }

    // start next epoch i.e., add/ remove more flows
    if (event_list.size() > 0) {
      // should be in 0 seconds
      std::cout << "We saw " << max_iterations_of_goodness
                << " iterations of goodness, scheduling next epoch event from checkRate in " << 0 << "s.\n";;
      next_epoch_event = Simulator::Schedule(Seconds(0),
                                             &ConvergenceExperiments::startNextEpoch, this, true);
      // next_epoch++;
    } else {
      std::cout << "Stopping simulations after " << max_iterations_of_goodness << " iterations of goodness.\n";
      Simulator::Stop();
    }
  }

  // std::cout << Simulator::Now().GetSeconds() << "s Total: "
  //           << current_total_rate << " Mbps.\n";
   check_rates_event =
     Simulator::Schedule(
                         sampling_interval,
                         &ConvergenceExperiments::checkRates,
                         this);
}

// fills in flowToFlowId (so we can get correct FlowMonitor stats for each flow)
// only uses source, destination IP address and destination port
// void ConvergenceExperiments::mapFlowToFlowMonitorStats() {
//   Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
//   std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
//   for (const auto& fm_iter : stats) {
//     Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (fm_iter.first);
//     bool found = false;
//     for (const auto& iter : fiveTupleToFlow) {
//       if (iter.first.sourceAddress == t.sourceAddress &&
//           iter.first.destinationAddress == t.destinationAddress &&
//           iter.first.destinationPort == t.destinationPort) {
//         flowToFlowMonitorIndex[iter.second] = fm_iter.first;
//         found = true;
//       } else if (iter.first.destinationAddress == t.sourceAddress &&
//           iter.first.sourceAddress == t.destinationAddress &&
//           iter.first.destinationPort == t.sourcePort) {
//         found = true; // found FlowMonitor flow for ACK
//       }
//     }
//     if (!found) {
//       std::cout << "didn't find flow_id for stats for "
//                 << " flowId(FlowMonitor)=" << fm_iter.first << "/"
//                 << " sourceAddress=" << t.sourceAddress << "/"
//                 << " destinationAddress=" << t.destinationAddress << "/"
//                 << " protocol=" << int(t.protocol) << "/"
//                 << " sourcePort=" << t.sourcePort << "/"
//                 << " destinationPort=" << t.destinationPort << "\n";
//     }
//   }
// }


void ConvergenceExperiments::printSingleFlowStats(const FlowMonitor::FlowStats& flow_stats) {
  std::cout << "  Tx Packets:   " << flow_stats.txPackets << std::endl;
  std::cout << "  Tx Bytes:   " << flow_stats.txBytes << std::endl;
  std::cout << "  FCT (timeFirstTx to timeLastRx): " << (flow_stats.timeLastRxPacket.GetSeconds () - flow_stats.timeFirstTxPacket.GetSeconds ()) << " s" << std::endl;
  std::cout << "  Offered Load: " << flow_stats.txBytes * 8.0 / (flow_stats.timeLastTxPacket.GetSeconds () - flow_stats.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  Rx Packets:   " << flow_stats.rxPackets << std::endl;
  std::cout << "  Rx Bytes:   " << flow_stats.rxBytes << std::endl;
  
  std::cout << "  Throughput: " << flow_stats.rxBytes * 8.0 / (flow_stats.timeLastRxPacket.GetSeconds () - flow_stats.timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  EWMA rate of packets at receiver: " << flow_stats.rxEwmaRate / 1000000 << " Mbps" << std::endl;
  std::cout << "  Mean delay:   " << flow_stats.delaySum.GetSeconds () / flow_stats.rxPackets << std::endl;
  std::cout << "  Mean jitter:   " << flow_stats.jitterSum.GetSeconds () / (flow_stats.rxPackets - 1) << std::endl;
}

// prints stats for each flow in flows file from IP level (akak FlowMonitor stats)
// and application level. also prints queue stats for every link in network.
void ConvergenceExperiments::printExperimentStatsForWorkload() {
  NS_LOG_FUNCTION (this);
  //mapFlowToFlowMonitorStats();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
  for (uint32_t flow_id = 0; flow_id < all_flows.size(); flow_id++) {
    FlowId fm_index;
    bool found = false;
    if (flowToFlowMonitorIndex.find(flow_id) != flowToFlowMonitorIndex.end()) {
      fm_index = flowToFlowMonitorIndex.at(flow_id);
      found = true;
    } else if (flowToFiveTuple.find(flow_id) != flowToFiveTuple.end()
               and classifier->FindFlowId(flowToFiveTuple.at(flow_id), fm_index)) {
      const auto ret = flowToFlowMonitorIndex.insert(std::make_pair(flow_id, fm_index));
      NS_ASSERT(ret.second);
      found = true;
    }
      
    if (found) {
        NS_ASSERT(stats.find(fm_index) != stats.end());
        
        std::cout << "Flow flow=" << flow_id << "/"
                  << " flowId(FlowMonitor)=" << fm_index <<"\n";
                  // << "/"
                  // << " sourceAddress=" << t.sourceAddress << "/"
                  // << " destinationAddress=" << t.destinationAddress << "/"
                  // << " protocol=" << int(t.protocol) << "/"
                  // << " sourcePort=" << t.sourcePort << "/"
                  // << " destinationPort=" << t.destinationPort << "\n";
      
        printSingleFlowStats(stats.at(fm_index));
    }
  }
  

  std::cout << std::endl << "*** Application statistics ***" << std::endl;
  for (uint32_t i = 0; i < sink_apps.GetN(); i++) {
    double thr = 0;
    uint32_t totalPacketsThr = DynamicCast<PacketSink> (sink_apps.Get (i))->GetTotalRx ();
    thr = totalPacketsThr * 8 / (simulationTime * 1000000.0); //Mbit/s
    std::cout << "  Sink App " << i << " Rx Bytes: " << totalPacketsThr << std::endl;
    std::cout << "  Sink App " << i << " Average Goodput: " << thr << " Mbit/s" << std::endl;
  }

  
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
  NS_LOG_FUNCTION (this);
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));
}

void ConvergenceExperiments::setupFlowMonitor() {
  NS_LOG_FUNCTION (this);
  monitor = flowmon.InstallAll();
}

void ConvergenceExperiments::loadWorkloadFromFiles() {
  NS_LOG_FUNCTION (this);
  loadFlowArrivals();
  loadFlowDepartures();
  loadEvents();
  loadAllFlows();
  loadOptRates();
  using_files = true;
}

void ConvergenceExperiments::loadFlowArrivals() {
  NS_LOG_FUNCTION (this);
  std::ifstream flow_arrivals_file(flow_arrivals_filename, std::ifstream::in);
  
  if (flow_arrivals_file.is_open()) {
    uint32_t epoch, flow_id;
    while (flow_arrivals_file >> epoch >> flow_id) {
      if (epoch > flows_to_start.size()) {
        flows_to_start.push_back(std::vector<uint32_t>(1, flow_id));
      } else {
        flows_to_start.back().push_back(flow_id);
      }
    }
  }
}

void ConvergenceExperiments::loadFlowDepartures() {
  NS_LOG_FUNCTION (this);
  std::ifstream flow_departures_file(flow_departures_filename, std::ifstream::in);
  uint32_t last_epoch = 0;
  if (flow_departures_file.is_open()) {
    uint32_t epoch, flow_id;
    while (flow_departures_file >> epoch >> flow_id) {
      // epoch must start from 1
      if (epoch > last_epoch) {
        // epoch 1 X means flows_to_stop should have size 1
        flows_to_stop.push_back(std::vector<uint32_t>());
        last_epoch = epoch;
      }
      flows_to_stop.back().push_back(flow_id);
    }}
}

void ConvergenceExperiments::loadEvents() {
  NS_LOG_FUNCTION (this);
  const std::string start_str="start_flows";
  std::ifstream events_file(events_filename, std::ifstream::in);
  if (events_file.is_open()) {
    std::string event_type;
    while(events_file >> event_type) {
      std::cout << "event type is " << event_type
                << " and start_str is "  << start_str << "\n";
      if(event_type.compare(start_str) == 0) {
        event_list.push_back(true);
      } else {
        event_list.push_back(false);
      }
    }
  }
}

// flows indexed by rank in flows file, this is flow_id
// must specify source, destination and destination port
void ConvergenceExperiments::loadAllFlows() {
  NS_LOG_FUNCTION (this);
  std::ifstream flows_file(flows_filename, std::ifstream::in);
  if (flows_file.is_open()) {
    uint32_t source, destination;
    uint16_t source_port, destination_port;
    while(flows_file >> source >> source_port >> destination >> destination_port) {
      all_flows.push_back(std::make_pair(source, destination));
      all_flows_ports.push_back(std::make_pair(source_port, destination_port));
    }
  }
}

void ConvergenceExperiments::loadOptRates() {
  NS_LOG_FUNCTION (this);
  std::ifstream opt_rates_file(opt_rates_filename,
                               std::ifstream::in);
  if (opt_rates_file.is_open()) {
    uint32_t epoch, flow_id;
    double rate; // Mbps
    while(opt_rates_file >> epoch >> flow_id >> rate) {
      opt_rates[epoch][flow_id] = rate;
    }
  }
}

void ConvergenceExperiments::showWorkloadFromFiles() {
  NS_LOG_FUNCTION (this);
  std::stringstream out_str;
  uint32_t next_flows_to_start = 0;
  uint32_t next_flows_to_stop = 0;
  uint32_t next_event = 0;
  uint32_t epoch = next_event+1;
  while (next_event < event_list.size() && next_event < opt_rates.size()) {    
    epoch = next_event+1;
    out_str << "epoch " << epoch << ": ";
    bool start_flow_next = event_list.front();
    event_list.pop_front();
    event_list.push_back(start_flow_next);
    if (start_flow_next) {
      out_str << "; start ";
      for (const auto& flow_id : flows_to_start.at(next_flows_to_start)) {
        out_str << flow_id
                << "(" << all_flows.at(flow_id).first
                << "->" << all_flows.at(flow_id).second
                << ") ";
      }
      next_flows_to_start++;
    } else {
      out_str << "; stop ";
      for (const auto& flow_id : flows_to_stop.at(next_flows_to_stop)) {
        out_str << flow_id
                << "(" << all_flows.at(flow_id).first
                << "->" << all_flows.at(flow_id).second
                << ") ";
      }
      next_flows_to_stop++;
    }
    out_str << "; opt_rates (epoch " << next_event+1 << ") ";
    const auto& rates = opt_rates.at(next_event+1); 
    for (const auto& it : rates) {
      out_str << it.first << ": " << it.second << " Mbps ";
    }
    out_str << "\n";
    next_event++;
  }
  std::cout << out_str.str();  
}
