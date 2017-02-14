/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Universita' degli Studi di Napoli "Federico II"
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
 * Author: Pasquale Imputato <p.imputato@gmail.com>
 * Author: Stefano Avallone <stefano.avallone@unina.it>
 */

#include "ns3/core-module.h"
#include "ns3/convergence-experiments.h"

// This simple example shows how to run convergence times experiments
//
// Network topology
//
//       10.1.1.0
// n0-4 -------------- n5
//    point-to-point
//
// The output will consist of all the traced changes in the length of the
// internal queue and in the length of the netdevice queue:
//
//    DevicePacketsInQueue 0 to 1
//    TcPacketsInQueue 7 to 8
//    TcPacketsInQueue 8 to 9
//    DevicePacketsInQueue 1 to 0
//    TcPacketsInQueue 9 to 8
//
// plus some statistics collected at the network layer (by the flow monitor)
// and the application layer. Finally, the number of packets dropped by the
// queuing discipline, the number of packets dropped by the netdevice and
// the number of packets requeued by the queuing discipline are reported.

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ConvergenceExperimentsExample");

int
main (int argc, char *argv[])
{
  
  //ConvergenceExperiments exp;
  ConvergenceExperiments exp("src/perc/examples/flow_arrivals_1.txt",
                         "src/perc/examples/flow_departures_1.txt",
                         "src/perc/examples/events_1.txt",
                         "src/perc/examples/flows_1.txt",
                         "src/perc/examples/opt_rates_1.txt");

  exp.parseCmdConfig(argc, argv);
  exp.run();
  return 0;
}
