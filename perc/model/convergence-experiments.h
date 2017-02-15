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

#ifndef CONVERGENCE_EXPERIMENTS_H
#define CONVERGENCE_EXPERIMENTS_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

#include <unordered_set>

using namespace ns3;

class ConvergenceExperiments {
public:
  ConvergenceExperiments();
  ConvergenceExperiments(const std::string& flow_arrivals_filename,
                         const std::string& flow_departures_filename,
                         const std::string& events_filename,
                         const std::string& flows_filename,
                         const std::string& opt_rates_filename);
  void parseCmdConfig(int argc, char *argv[]);
  void run();
  
private:
  void loadWorkloadFromFiles();
  void loadFlowArrivals();
  void loadFlowDepartures();
  void loadEvents();
  void loadAllFlows();
  void loadOptRates();
  void showWorkloadFromFiles();

  void createTopology();  
  void setupFlowMonitor();
  void setConfigDefaults();

  void startApp(uint16_t source_port, uint16_t destination_port,
                uint32_t source_host, uint32_t destination_host,
                Time start_time);

  void stopApp(uint32_t flow_id);

  // I think startNextEpoch is called every 20ms or as soon as 95% flows
  // have converged.. scheduled form startNextEpoch, canceled and re-scheduled
  // by checkRates
  void startNextEpoch(bool converged_in_previous);

  void checkRates();

  // fills in flowToFlowId (so we can get correct FlowMonitor stats for each flow)
  // only uses source, destination IP address and destination port
  // void mapFlowToFlowMonitorStats();
  void printExperimentStatsForWorkload();
  void printSingleFlowStats(const FlowMonitor::FlowStats& flow_stats);

  Time last_epoch_time; // time when startNextEpoch was last called
  uint32_t next_epoch = 1;
  EventId next_epoch_event;
  EventId check_rates_event;
  // index into flows to start vector, for next set of flows to
  // start in whatever epoch
  uint32_t flows_to_start_next = 0;
  uint32_t flows_to_stop_next = 0;
  
  // x : in last consecutive x iterations,
  // ninety five percent of flows were within 10 percent
  // of optimal rate for current epoch  
  uint32_t ninety_fifth_converged = 0;
  
  std::string flow_arrivals_filename; // epoch flow_index
  std::string flow_departures_filename; // epoch flow_index
  std::string events_filename; // start_flows | stop_flows
  std::string flows_filename; // src host, dst host
  std::string opt_rates_filename; // [flow_index: rate]+

  std::vector<std::vector<uint32_t> > flows_to_start;
  std::vector<std::vector<uint32_t> > flows_to_stop;
  std::list<bool> event_list; // true: start flows
  
  std::vector<std::pair<uint32_t, uint32_t> > all_flows; // source, destination
  std::vector<std::pair<uint16_t, uint32_t> > all_flows_ports; // TODO(lav): maybe merge with ^

  std::map<uint32_t, std::map<uint32_t, double> > opt_rates; // indexed by epoch

  // set of active flows for this epoch, updated in startNextEpoch
  // active flows here means, flows added in/before current epoch
  // and not removed until next epoch or after.
  // used when checking rates
  std::unordered_set<uint32_t> active_flows;
  
  bool using_files = false; // true if we load workload from files

  double simulationTime = 10; //seconds
  uint32_t payloadSize = 1448;
  // maximum number of iterations of goodness before moving on
  const uint32_t max_iterations_of_goodness = 2500;
  // maximum time before changing epoch
  Time max_epoch_seconds = Seconds(0.6);
  Time sampling_interval = MicroSeconds(100); // 20us
  
  NodeContainer hosts;  
  NodeContainer leafnodes;
  std::string transportProt = "Tcp";
  std::string socketType;
  NetDeviceContainer edgedevices;
  Ipv4InterfaceContainer interfaces;
  QueueDiscContainer qdiscs;

  // filled in by setupApp
  ApplicationContainer sink_apps;
  ApplicationContainer sending_apps;
  std::map<uint32_t, uint32_t> flowToAppIndex;
  std::map<Ipv4FlowClassifier::FiveTuple, uint32_t> fiveTupleToFlow;
  std::map<uint32_t, Ipv4FlowClassifier::FiveTuple> flowToFiveTuple;
  
  // flow monitor indexes all observed flows by a "FlowId", different from index of flow in flows file
  // currently this is populated at the end, but maybe want to update it each time flow monitor adds a new flow
  std::map<uint32_t, FlowId> flowToFlowMonitorIndex; 
  
  // used by old setupApps TODO(lav): remove
  ApplicationContainer sinkApp;
  ApplicationContainer apps;

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor;
};

#endif
