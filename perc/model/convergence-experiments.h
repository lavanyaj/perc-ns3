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

using namespace ns3;

class ConvergenceExperiments {
public:
  ConvergenceExperiments();
  void parseCmdConfig(int argc, char *argv[]);
  void run();
  
private:
  void createTopology();
  void setupApps();
  void printExperimentStats();
  void setupFlowMonitor();
  void setConfigDefaults();

  double simulationTime = 10; //seconds
  uint32_t payloadSize = 1448;
  
  NodeContainer hosts;  
  NodeContainer leafnodes;
  std::string transportProt = "Tcp";
  std::string socketType;
  NetDeviceContainer edgedevices;
  Ipv4InterfaceContainer interfaces;
  QueueDiscContainer qdiscs;

  ApplicationContainer sinkApp;
  ApplicationContainer apps;

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor;
};

#endif
