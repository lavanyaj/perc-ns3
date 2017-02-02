#include "declarations.h"
#include <string>
#include <iostream>
#include <fstream>

using namespace ns3;

std::string get_datarate(std::string s)
{
  std::string::size_type pos = s.find('G');
    if (pos != std::string::npos)
    {
        return s.substr(0, pos);
    }
    else
    {
        return s;
    }
}

void sinkInstallNodeEvent(uint32_t sourceN, uint32_t sinkN, uint16_t port, uint32_t flow_id, double startTime, uint32_t numBytes, uint32_t tcp)
{
  // Create a packet sink on the star "hub" to receive these packets
  Address anyAddress = InetSocketAddress (Ipv4Address::GetAny (), port);

  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", anyAddress);
  if(tcp) {
    sinkHelper.SetAttribute("Protocol", StringValue("ns3::TcpSocketFactory"));
    sinkHelper.SetAttribute("Local", AddressValue(anyAddress));
//    sinkHelper ("ns3::TcpSocketFactory", anyAddress);
  }
  ApplicationContainer sinkAppContainer = sinkHelper.Install (allNodes.Get(sinkN));
  sinkAppContainer.Start(Seconds(0.0));
  sinkApps.Add(sinkAppContainer);


  NS_LOG_UNCOND("sink apps installed on node "<<(allNodes.Get(sinkN))->GetId());
  Ptr<PacketSink> pSink = StaticCast <PacketSink> (sinkAppContainer.Get(0));
  pSink->SetAttribute("numBytes", UintegerValue(numBytes));
  pSink->SetAttribute("flowid", UintegerValue(flow_id));
  pSink->SetAttribute("nodeid", UintegerValue(allNodes.Get(sinkN)->GetId()));
  pSink->SetAttribute("peernodeid", UintegerValue(allNodes.Get(sourceN)->GetId()));
  pSink->setTracker(flowTracker);


  /* Debug... Check what we set */
  UintegerValue nb, fid, n1, n2;
  pSink->GetAttribute("numBytes", nb);
  pSink->GetAttribute("flowid", fid);
  pSink->GetAttribute("nodeid", n1);
  pSink->GetAttribute("peernodeid", n2);
  NS_LOG_UNCOND("sink attributed set : numbytes "<<nb.Get()<<" flowid "<<fid.Get()<<" nodeid "<<n1.Get()<<" source nodeid "<<n2.Get());
  
}
Ptr<PacketSink> sinkInstallNode(uint32_t sourceN, uint32_t sinkN, uint16_t port, uint32_t flow_id, double startTime, uint32_t numBytes, uint32_t tcp)
{
  std::cout << "create a packet sink to receiver packets of flow "
	    << flow_id << " from " << sourceN << " to " << sinkN
	    << " which has " << port << " ports (?)\n";
    
  // Create a packet sink on the star "hub" to receive these packets
  Address anyAddress = InetSocketAddress (Ipv4Address::GetAny (), port);

  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", anyAddress);
  if(tcp) {
    sinkHelper.SetAttribute("Protocol", StringValue("ns3::TcpSocketFactory"));
    sinkHelper.SetAttribute("Local", AddressValue(anyAddress));
//    sinkHelper ("ns3::TcpSocketFactory", anyAddress);
  }
  assert(sinkNodes.GetN() > sinkN);
  ApplicationContainer sinkAppContainer = sinkHelper.Install (sinkNodes.Get(sinkN));
  sinkAppContainer.Start(Seconds(0.0));
  sinkApps.Add(sinkAppContainer);


  NS_LOG_UNCOND("sink apps installed on node "<<(sinkNodes.Get(sinkN))->GetId());

  assert(sinkAppContainer.GetN() > 0);
  Ptr<PacketSink> pSink = StaticCast <PacketSink> (sinkAppContainer.Get(0));

  pSink->SetAttribute("numBytes", UintegerValue(numBytes));
  pSink->SetAttribute("flowid", UintegerValue(flow_id));
  pSink->SetAttribute("nodeid", UintegerValue(sinkNodes.Get(sinkN)->GetId()));
  assert(sourceNodes.GetN() > sourceN);
  pSink->SetAttribute("peernodeid", UintegerValue(sourceNodes.Get(sourceN)->GetId()));
  std::cout << "setTracker\n";
  pSink->setTracker(flowTracker);


  /* Debug... Check what we set */
  UintegerValue nb, fid, n1, n2;
  pSink->GetAttribute("numBytes", nb);
  pSink->GetAttribute("flowid", fid);
  pSink->GetAttribute("nodeid", n1);
  pSink->GetAttribute("peernodeid", n2);
  NS_LOG_UNCOND("sink attributed set : numbytes "<<nb.Get()<<" flowid "<<fid.Get()<<" nodeid "<<n1.Get()<<" source nodeid "<<n2.Get());

  return pSink;
  
}

void
CheckQueueSize (Ptr<Queue> queue)
{
  if(queue_type == "WFQ") {
    uint32_t qSize = StaticCast<PrioQueue> (queue)->GetCurSize ();
    uint32_t nid = StaticCast<PrioQueue> (queue)->nodeid;
    std::string qname = StaticCast<PrioQueue> (queue)->GetLinkIDString();
    double cur_price = StaticCast<PrioQueue> (queue)->getCurrentPrice();
    double cur_departrate = StaticCast<PrioQueue> (queue)->getCurrentDepartureRate();
    double cur_utilterm = StaticCast<PrioQueue> (queue)->getCurrentUtilTerm();
    checkTimes++;
    std::cout<<"common_utils.cc: WFQ QueueStats "<<qname<<" "<<Simulator::Now ().GetSeconds () << " " << qSize<<" "<<nid<<" "<<cur_price<<" "<<cur_departrate<<" "<<cur_utilterm<<std::endl;
    std::map<std::string, uint32_t>::iterator it;
/*    for (std::map<std::string,uint32_t>::iterator it= flowids.begin(); it!= flowids.end(); ++it) {
      double dline = StaticCast<PrioQueue> (queue)->get_stored_deadline(it->first);
      double virtual_time = StaticCast<PrioQueue> (queue)->get_virtualtime();
      double current_slope = StaticCast<PrioQueue> (queue)->getCurrentSlope();
      std::cout<<"QueueStats1 "<<qname<<" "<<Simulator::Now().GetSeconds()<<" "<<it->second<<" "<<dline<<" "<<virtual_time<<" "<<" "<<current_slope<<" "<<cur_price<<" "<<nid<<std::endl;
    } */
  } 
  if(queue_type == "W2FQ") {
    uint32_t qSize = StaticCast<W2FQ> (queue)->GetCurSize (0);
    uint32_t nid = StaticCast<W2FQ> (queue)->nodeid;
    std::string qname = StaticCast<W2FQ> (queue)->GetLinkIDString();
    checkTimes++;
    std::cout<<"common_utils.cc: W2FQ QueueStats "<<qname<<" "<<Simulator::Now ().GetSeconds () << " " << qSize<<" "<<nid<<std::endl;
/*    std::map<std::string, uint32_t>::iterator it;
    for (std::map<std::string,uint32_t>::iterator it= flowids.begin(); it!= flowids.end(); ++it) {
      uint64_t virtual_time = StaticCast<W2FQ> (queue)->get_virtualtime();
      double dline = 0.0;
      std::cout<<"QueueStats1 "<<qname<<" "<<Simulator::Now().GetSeconds()<<" "<<it->second<<" "<<dline<<" "<<virtual_time<<" "<<" "<<nid<<std::endl;
    } */
  }

  if(queue_type == "hybridQ") {
    uint32_t qSize = StaticCast<hybridQ> (queue)->GetCurSize (0);
    uint32_t nid = StaticCast<hybridQ> (queue)->nodeid;
    std::string qname = StaticCast<hybridQ> (queue)->GetLinkIDString();
    uint32_t fifosize = StaticCast<hybridQ> (queue)->GetFifoSize();
    checkTimes++;
    std::cout<<"common_utils.cc: hybridQ QueueStats "<<qname<<" "<<Simulator::Now ().GetSeconds () << " " << qSize<<" "<<nid<<" "<<fifosize<<std::endl;
/*    std::map<std::string, uint32_t>::iterator it;
    for (std::map<std::string,uint32_t>::iterator it= flowids.begin(); it!= flowids.end(); ++it) {
      uint64_t virtual_time = StaticCast<hybridQ> (queue)->get_virtualtime();
      double dline = 0.0;
      std::cout<<"QueueStats1 "<<qname<<" "<<Simulator::Now().GetSeconds()<<" "<<it->second<<" "<<dline<<" "<<virtual_time<<" "<<" "<<nid<<std::endl;
    }
*/
  }

  if(queue_type == "fifo_hybridQ") {
    uint32_t nid = StaticCast<fifo_hybridQ> (queue)->nodeid;
    std::string qname = StaticCast<fifo_hybridQ> (queue)->GetLinkIDString();
    uint32_t fifo_1_size = StaticCast<fifo_hybridQ> (queue)->GetFifo_1_Size();
    uint32_t fifo_2_size = StaticCast<fifo_hybridQ> (queue)->GetFifo_2_Size();
    checkTimes++;
    // example line
    // QueueStats 2_2_0 1.5 fifo_2_size 1078 fifo_1_size 2 0
    std::cout<<"common_utils.cc: fifo_hybridQ QueueStats "<< qname <<" "<<Simulator::Now ().GetSeconds () << " fifo_2_size " << fifo_2_size << " fifo_1_size " << fifo_1_size << " node_id " << nid <<std::endl;
  }

  if(queue_type == "FifoQueue") {
    uint32_t qSize = StaticCast<FifoQueue> (queue)->GetCurSize ();
    uint32_t nid = StaticCast<FifoQueue> (queue)->nodeid;
    std::string qname = StaticCast<FifoQueue> (queue)->GetLinkIDString();
    checkTimes++;
    std::cout<<"common_utils.cc: FifoQueue QueueStats "<<qname<<" "<<Simulator::Now ().GetSeconds () << " " << qSize<<" "<<nid<<std::endl;
  }

  
    Simulator::Schedule (Seconds (sampling_interval), &CheckQueueSize, queue);
    if(Simulator::Now().GetSeconds() >= sim_time+1.0) {
      Simulator::Stop();
    }
 
}

CommandLine addCmdOptions(void)
{
  
  CommandLine cmd;
  cmd.AddValue ("num_events", "num_events", num_events);
  cmd.AddValue ("bwe_enable" , "bwe_enable", bwe_enable);  
  cmd.AddValue ("num_subflows", "num_subflows", num_subflows);
  cmd.AddValue ("mptcp", "mptcp",mptcp);
  cmd.AddValue ("measurement_ewma", "measurement_ewma", kvalue_measurement);
  cmd.AddValue ("wfq_testing", "wfq_testing", wfq);
  cmd.AddValue ("fct_alpha", "fctalpha for utility", fct_alpha);
  cmd.AddValue ("nNodes", "Number of nodes", N);
  cmd.AddValue ("prefix", "Output prefix", prefix);
  cmd.AddValue ("queuetype", "Queue Type", queue_type);
  cmd.AddValue ("pkt_tag","pkt_tag",pkt_tag);
  cmd.AddValue ("sim_time", "sim_time", sim_time);
  cmd.AddValue ("pkt_size", "pkt_size", pkt_size);
  cmd.AddValue ("link_rate","link_rate",link_rate);
  cmd.AddValue ("link_delay","link_delay",link_delay);
  cmd.AddValue ("ecn_thresh", "ecn_thresh", max_ecn_thresh);
  cmd.AddValue ("load", "load",load);
  cmd.AddValue ("controller_estimated_unknown_load", "controller_estimated_unknown_load",controller_estimated_unknown_load);
  cmd.AddValue ("rate_update_time", "rate_update_time", rate_update_time);
  cmd.AddValue ("sampling_interval", "sampling_interval", sampling_interval);
  cmd.AddValue ("kvalue_price", "kvalue_price", kvalue_price);
  cmd.AddValue ("kvalue_rate", "kvalue_rate", kvalue_rate);
  cmd.AddValue ("vpackets", "vpackets", vpackets);
  cmd.AddValue ("xfabric", "xfabric", xfabric);
  cmd.AddValue ("dctcp", "dctcp", dctcp);
  cmd.AddValue ("hostflows", "hostflows",flows_per_host);
  cmd.AddValue ("flows_tcp", "flows_tcp", flows_tcp);
  cmd.AddValue ("weight_change", "weight_change", weight_change);
  //cmd.AddValue ("weight_norm", "weight_norm", weight_normalized);
  cmd.AddValue ("rate_based", "rate_based", rate_based);
  //cmd.AddValue ("UNKNOWN_FLOW_SIZE_CUTOFF", "unknown_flow_size_cutoff", UNKNOWN_FLOW_SIZE_CUTOFF);
  cmd.AddValue ("scheduler_mode_edf", "scheduler_mode_edf", scheduler_mode_edf);
  cmd.AddValue ("deadline_mode", "deadline_mode", deadline_mode);
  cmd.AddValue ("deadline_mean", "deadline_mean", deadline_mean);
  cmd.AddValue ("price_update_time", "price_update_time", price_update_time);
  cmd.AddValue ("xfabric_eta", "xfabric_eta", xfabric_eta);
  cmd.AddValue ("xfabric_beta", "xfabric_beta", xfabric_beta);
  cmd.AddValue ("host_compensate", "host_compensate", host_compensate);
  cmd.AddValue ("util_method", "util_method", util_method);
  cmd.AddValue ("strawmancc", "strawmancc", strawmancc);
  cmd.AddValue ("dgd_b", "dgd_b", dgd_b);
  cmd.AddValue ("dgd_a", "dgd_a", dgd_a);
  cmd.AddValue ("target_queue", "target_queue", target_queue);
  cmd.AddValue ("guardtime", "guardtime", guard_time);
  cmd.AddValue ("pfabric_util", "pfabric_util",pfabric_util);
  cmd.AddValue ("num_spines", "num_spines", num_spines);
  cmd.AddValue ("num_leafs", "num_leafs", num_leafs);
  cmd.AddValue ("num_hosts_per_leaf", "num_hosts_per_leaf", num_hosts_per_leaf);
  cmd.AddValue ("fabric_datarate", "fabric_datarate", fabric_datarate);
  cmd.AddValue ("edge_datarate", "edge_datarate", edge_datarate);
  cmd.AddValue ("application_datarate", "application_datarate", application_datarate);
  cmd.AddValue ("flow_ecmp", "flow_ecmp", flow_ecmp);
  cmd.AddValue ("packet_spraying", "packet_spraying", packet_spraying);
  cmd.AddValue ("dt_val", "dt_value", dt_val);
  cmd.AddValue ("price_multiply", "price_multiply", price_multiply);
  cmd.AddValue ("cdf_file", "cdf_file", empirical_dist_file);
  cmd.AddValue ("num_flows", "num_flows", number_flows); 
  cmd.AddValue ("desynchronize", "desynchronize", desynchronize);
  cmd.AddValue ("dgd_m", "dgd_m", multiplier);
  cmd.AddValue ("opt_rates_file", "opt_rates_file", opt_rates_file);

  cmd.AddValue ("rcp_alpha", "rcp_alpha", rcp_alpha);
  cmd.AddValue ("rcp_beta", "rcp_beta", rcp_beta);
  cmd.AddValue ("alpha_fair_rcp", "alpha_fair_rcp", alpha_fair_rcp);
  std::cout<<"desync is "<<desynchronize<<std::endl;

  return cmd;
}



void dump_config(void)
{
  std::cout<<"num_spines "<<num_spines<<std::endl;
  std::cout<<"num_leafs "<<num_leafs<<std::endl;
  std::cout<<"num_hosts_per_leaf "<<num_hosts_per_leaf<<std::endl;
  std::cout<<"price_update_time "<<price_update_time<<std::endl;
  std::cout<<"guard_time "<<guard_time<<std::endl;
  std::cout<<"dt "<<dt_val<<std::endl;
  std::cout<<"kvalue_rate "<<kvalue_rate<<std::endl;
  std::cout<<"kvalue_price "<<kvalue_price<<std::endl;
  std::cout<<"kvalue_measurement "<<kvalue_measurement<<std::endl;
  std::cout<<"opt_rates_file "<<opt_rates_file<<std::endl;
  std::cout<<"util_method "<<util_method<<std::endl;
  std::cout<<"fct_alpha "<<fct_alpha<<std::endl;

  std::cout<<"application_datarate "<<application_datarate<<std::endl;
  std::cout<<"eta_val "<<xfabric_eta<<std::endl;
  std::cout<<"beta_val "<<xfabric_beta<<std::endl;
  std::cout<<"host_compensate "<<host_compensate<<std::endl;

  std::cout<<"rcp alpha "<<rcp_alpha<<std::endl;
  std::cout<<"rcp beta "<<rcp_beta<<std::endl;
  std::cout<<"alpha fair rcp "<<alpha_fair_rcp<<std::endl;
  std::cout<<"sim_time "<<sim_time<<std::endl;
  std::cout<<"num_events "<<num_events<<std::endl;

  std::string::size_type sz;
  double edge_data = atof(get_datarate(edge_datarate).c_str());
  double fabric_data = atof(get_datarate(fabric_datarate).c_str())/edge_data; 

  std::cout<<"topo_info "<<num_leafs<<" "<<num_spines<<" "<<num_hosts_per_leaf<<" 1 "<<fabric_data<<std::endl;
}

void common_config(void)
{

  // get link rate from edge_datarate string
  link_rate = ONEG * atof(get_datarate(edge_datarate).c_str());
  double total_rtt = link_delay * 8.0 *1.0; 
  uint32_t bdproduct = link_rate *total_rtt/(1000000.0* 8.0);

  //uint32_t initcwnd = (bdproduct / max_segment_size) +1;
  uint32_t initcwnd = 0;

  if(alpha_fair_rcp || strawmancc) {
    initcwnd = bdproduct * 2.0;
  } else {
    initcwnd = bdproduct/4;
  }

  initcwnd = initcwnd / max_segment_size;

  uint32_t ssthresh = initcwnd * max_segment_size;
  LastEventTime = 1.0;
  pkt_tag = xfabric; 
  
  dgd_a = dgd_a*multiplier;
  dgd_b = dgd_b*multiplier;

  std::cout<<"dgd_b "<<dgd_b<<" dgd_a "<<dgd_a<<" multiplier "<<multiplier<<std::endl;

  std::cout<<"Setting ssthresh = "<<ssthresh<<" initcwnd = "<<initcwnd<<" link_delay  "<<link_delay<<" bdproduct "<<bdproduct<<" total_rtt "<<total_rtt<<" link_rate "<<link_rate<<std::endl;  

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(max_segment_size));
//  Config::SetDefault ("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(ssthresh_value));
  Config::SetDefault ("ns3::TcpSocketBase::Timestamp", BooleanValue(false));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (recv_buf_size));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (send_buf_size));
  Config::SetDefault ("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(ssthresh));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue(initcwnd));

  // Disable delayed ack
  Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  Config::SetDefault("ns3::TcpNewReno::dctcp", BooleanValue(dctcp));
  Config::SetDefault("ns3::TcpNewReno::xfabric", BooleanValue(xfabric));
  Config::SetDefault("ns3::PacketSink::StartMeasurement",TimeValue(Seconds(measurement_starttime)));
  Config::SetDefault("ns3::TcpNewReno::strawman", BooleanValue(strawmancc));
  Config::SetDefault("ns3::TcpNewReno::dt_value", DoubleValue(dt_val));
  Config::SetDefault("ns3::TcpNewReno::line_rate", DoubleValue(link_rate));

  if(!xfabric) {
    Config::SetDefault("ns3::PrioQueue::m_pkt_tag",BooleanValue(false));
  } else {
    Config::SetDefault("ns3::PrioQueue::m_pkt_tag",BooleanValue(true));
    Config::SetDefault("ns3::Ipv4L3Protocol::m_pkt_tag", BooleanValue(pkt_tag));
  }
  Config::SetDefault("ns3::PrioQueue::desynchronize" , BooleanValue(desynchronize));
	
  Config::SetDefault("ns3::PrioQueue::rcp_alpha", DoubleValue(rcp_alpha));
  Config::SetDefault("ns3::PrioQueue::rcp_beta", DoubleValue(rcp_beta));
  Config::SetDefault("ns3::PrioQueue::fct_alpha", DoubleValue(fct_alpha));
  Config::SetDefault("ns3::PrioQueue::alpha_fair_rcp",BooleanValue(alpha_fair_rcp));
  Config::SetDefault("ns3::Ipv4L3Protocol::alpha_fair_rcp", BooleanValue(alpha_fair_rcp));

  Config::SetDefault("ns3::Ipv4L3Protocol::wfq_testing", BooleanValue(wfq));
    
  Config::SetDefault ("ns3::PrioQueue::Mode", StringValue("QUEUE_MODE_BYTES"));
  Config::SetDefault ("ns3::PrioQueue::MaxBytes", UintegerValue (max_queue_size));
  Config::SetDefault ("ns3::PrioQueue::ECNThreshBytes", UintegerValue (max_ecn_thresh));
  Config::SetDefault ("ns3::PrioQueue::delay_mark", BooleanValue(delay_mark_value));
  Config::SetDefault("ns3::PrioQueue::xfabric_price",BooleanValue(xfabric));
  Config::SetDefault("ns3::PrioQueue::dgd_a", DoubleValue(dgd_a));
  Config::SetDefault("ns3::PrioQueue::dgd_b",DoubleValue(dgd_b));
  Config::SetDefault("ns3::PrioQueue::target_queue", DoubleValue(target_queue));
  Config::SetDefault("ns3::PrioQueue::guardTime",TimeValue(Seconds(guard_time)));
  Config::SetDefault("ns3::PrioQueue::PriceUpdateTime",TimeValue(Seconds(price_update_time)));
  Config::SetDefault("ns3::PrioQueue::numfabric_eta",DoubleValue(xfabric_eta));
  Config::SetDefault("ns3::PrioQueue::xfabric_beta",DoubleValue(xfabric_beta));
  Config::SetDefault("ns3::PrioQueue::price_multiply",BooleanValue(price_multiply));


  Config::SetDefault ("ns3::FifoQueue::Mode", StringValue("QUEUE_MODE_BYTES"));
  Config::SetDefault ("ns3::FifoQueue::MaxBytes", UintegerValue (max_queue_size));
  Config::SetDefault ("ns3::FifoQueue::ECNThreshBytes", UintegerValue (max_ecn_thresh));

  Config::SetDefault ("ns3::fifo_hybridQ::Mode", StringValue("QUEUE_MODE_BYTES"));
  Config::SetDefault ("ns3::fifo_hybridQ::MaxBytes", UintegerValue (max_queue_size));
  Config::SetDefault ("ns3::fifo_hybridQ::ECNThreshBytes", UintegerValue (max_ecn_thresh));

  Config::SetDefault ("ns3::hybridQ::Mode", StringValue("QUEUE_MODE_BYTES"));
  Config::SetDefault ("ns3::hybridQ::MaxBytes", UintegerValue (max_queue_size));
  Config::SetDefault ("ns3::hybridQ::ECNThreshBytes", UintegerValue (max_ecn_thresh));

  Config::SetDefault ("ns3::PrioQueue::host_compensate", BooleanValue(host_compensate));
  Config::SetDefault("ns3::Ipv4L3Protocol::host_compensate", BooleanValue(host_compensate));
// one-set
  Config::SetDefault("ns3::Ipv4L3Protocol::m_pfabric", BooleanValue(pfabric_util));
  Config::SetDefault("ns3::PrioQueue::m_pfabricdequeue", BooleanValue(pfabric_util)); 

  Config::SetDefault("ns3::Ipv4L3Protocol::UtilFunction", UintegerValue(util_method));
  
  
  // rate_based 
  Config::SetDefault("ns3::Ipv4L3Protocol::rate_based", BooleanValue(strawmancc));

  Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(packet_spraying));
  Config::SetDefault("ns3::Ipv4GlobalRouting::FlowEcmpRouting", BooleanValue(flow_ecmp));

  // tcp newreno



  flowTracker = new Tracker();
  flowTracker->register_callback(scheduler_wrapper);
  /*
  std::stringstream opt_rates_full; 
  opt_rates_full<<"./opt_rates/"<<opt_rates_file;
  std::ifstream ORfile (opt_rates_full.str().c_str(), std::ifstream::in);
  std::cout<<" opening optimal rates file "<<opt_rates_full.str()<<std::endl;
  if(ORfile.is_open()) {
    int epoch, flowid;
    double datarate;
    while( ORfile >> epoch >> flowid >> datarate)
    {
      //OptDataRate dRate;
      //dRate.flowid = flowid;
      //dRate.datarate = datarate;
      opt_drates[epoch][flowid]= datarate;
      std::cout<<" at epoch "<<epoch<<" datarate "<<datarate<<" flow "<<flowid<<std::endl;
    }
  }
      
  std::ifstream FAFile ("flow_arrivals", std::ifstream::in);
  if(FAFile.is_open()) {
    int flowid;
    while( FAFile >> flowid)
    {
      flows_to_start.push_back(flowid);
    }
  }

  std::ifstream FDFile ("flow_departures", std::ifstream::in);
  if(FDFile.is_open()) {
    int flowid;
    while( FDFile >> flowid)
    {
      flows_to_stop.push_back(flowid);
    }
  }

  std::ifstream EventFile ("events_list", std::ifstream::in);
  if(EventFile.is_open()) {
    std::string event_type;
    std::string start_str="start_flows";
    while( EventFile >> event_type)
    {
      std::cout<<"event_type "<<event_type<<std::endl;
      if(event_type.compare(start_str)) {
          event_list.push_back(0);
          std::cout<<"pushing 1"<<std::endl;
      } else {
          event_list.push_back(1);
          std::cout<<"pushing 0"<<std::endl;
      }
    }
  }
  */
  return;

}

void scheduler_wrapper(uint32_t fid)
{
     std::cout<<"trackercalled "<<fid<<" stopped "<<std::endl;
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
//  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

void setuptracing(uint32_t sindex, Ptr<Socket> skt)
{
  
    //configure tracing
    std::string one = ".cwnd";
    std::stringstream ss;
    ss << "."<<sindex;
    std::string str = ss.str();
    std::string hname1 = prefix+one+str;
    std::cout<<"cwnd output in "<<hname1<<std::endl;
   
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream0 = asciiTraceHelper.CreateFileStream (hname1);
    skt->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream0));
  
}

void setUpMonitoring(void)
{
  
  uint32_t Ntrue = allNodes.GetN(); 
  for(uint32_t nid=0; nid<Ntrue; nid++)
  {
     Ptr<Ipv4> ipv4 = (allNodes.Get(nid))->GetObject<Ipv4> ();
     NS_LOG_UNCOND("Setting flows up... "); 
     //StaticCast<Ipv4L3Protocol> (ipv4)->setFlows(flowids);
     //StaticCast<Ipv4L3Protocol> (ipv4)->setQueryTime(0.000004);
     //StaticCast<Ipv4L3Protocol> (ipv4)->setAlpha(1.0/128.0);
     //StaticCast<Ipv4L3Protocol> (ipv4)->setQueryTime(rate_update_time);
     StaticCast<Ipv4L3Protocol> (ipv4)->setQueryTime(sampling_interval);
     StaticCast<Ipv4L3Protocol> (ipv4)->setAlpha(1.0);
     StaticCast<Ipv4L3Protocol> (ipv4)->setLineRate(link_rate);
     

     StaticCast<Ipv4L3Protocol> (ipv4)->setlong_ewma_const(kvalue_price);
     StaticCast<Ipv4L3Protocol> (ipv4)->setshort_ewma_const(kvalue_rate);
     StaticCast<Ipv4L3Protocol> (ipv4)->setmeasurement_ewma_const(kvalue_measurement);
     StaticCast<Ipv4L3Protocol> (ipv4)->setMPTCP(mptcp);

     
     //StaticCast<Ipv4L3Protocol> (ipv4)->setEpochUpdate(epoch_update_time);
     StaticCast<Ipv4L3Protocol> (ipv4)->setfctAlpha(fct_alpha);
  }
     
  //apps.Start (Seconds (1.0));
  //apps.Stop (Seconds (sim_time));

  Simulator::Schedule (Seconds (1.0), &CheckIpv4Rates, allNodes);
}


uint32_t getEpochNumber()
{
  return epoch_number;
}

void move_to_next()
{
 if(next_epoch_event.IsRunning()) {
    next_epoch_event.Cancel();
 }
 next_epoch_event = Simulator::ScheduleNow(startflowwrapper, sourcenodes, sinknodes);
}  

void
CheckIpv4Rates (NodeContainer &allNodes)
{
  double current_rate = 0.0;
  std::vector<double> error_vector;
  std::vector<double> nonerror_vector;

/*
  // iterate over all the sinkapp objects
  for(unsigned int i=0; i<sink_objects.size(); i++) {
      Ptr<PacketSink> sobj = sink_objects[i];
      uint32_t epoch_num = getEpochNumber();
      double ideal_rate = opt_drates[epoch_number][sobj->m_flowID] * 10000.0;
      std::cout<<"sinkdata "<<Simulator::Now().GetSeconds()<<" flowid "<<sobj->m_flowID<<" totalRx "<<sobj->GetTotalRx()*8<<" epoch "<<epoch_num<<" ideal_rate "<<ideal_rate<<std::endl;
  }
*/
  
  uint32_t N = allNodes.GetN(); 
  for(uint32_t nid=0; nid < N ; nid++)
  {

    Ptr<Ipv4L3Protocol> ipv4 = StaticCast<Ipv4L3Protocol> ((allNodes.Get(nid))->GetObject<Ipv4> ());
    std::map<std::string,uint32_t>::iterator it;
    // std::cout << "common_utils.cc: CheckIPv4 rates node " << nid << " has "
    // 	      << ipv4->flowids.size() << " flows.\n";
    for (std::map<std::string,uint32_t>::iterator it=ipv4->flowids.begin(); it!=ipv4->flowids.end(); ++it)
    {
													  
     double rate = ipv4->GetStoreDestRate (it->first);
//      double long_rate = ipv4->GetCSFQRate (it->first);
//      double short_rate = ipv4->GetShortTermRate(it->first);
      double measured_rate = ipv4->GetMeasurementRate(it->first);

      uint32_t s = it->second;

      // std::cout << "Node " << nid << " flow id " << it->second << "\n";
      // if (std::find((source_flow[nid]).begin(), (source_flow[nid]).end(), s)!=(source_flow[nid]).end()) std::cout << " and node " << nid << " is a source for flow id " << it->second << "\n";
      // else if (std::find((dest_flow[nid]).begin(), (dest_flow[nid]).end(), s)!=(dest_flow[nid]).end()) std::cout << " and node " << nid << " is a destination for flow id " << it->second << "\n";
      // else std::cout << " and node " << nid << " is neither source nore destination for flow id " << it->second << "\n";

      /* check if this flowid is from this source */
      if (std::find((source_flow[nid]).begin(), (source_flow[nid]).end(), s)!=(source_flow[nid]).end()) {
     int epoch_number = getEpochNumber();
	 if(epoch_number == num_events || epoch_number == 100) 
	 { 
	    std::cout<<" LAST EPOCH "<<Simulator::Now().GetSeconds()<<std::endl; 
	    Simulator::Stop();
	 }
         // ideal rates vector
         double ideal_rate = opt_drates[epoch_number][s] * 10000.0;
         std::cout<<"common_utils.cc CheckIpv4Rates: DestRate flowid "<<it->second<<" "<<Simulator::Now ().GetSeconds () << " " << measured_rate <<" "<< " " << rate << " " << ideal_rate<<" epoch "<<epoch_number<<std::endl;
        current_rate += measured_rate;
         double error = abs(ideal_rate - measured_rate)/ideal_rate;
         if(error < 0.1) {
           error_vector.push_back(error);
         } else {
           nonerror_vector.push_back(error);
         } 
      }
//    std::cout<<"finding flow "<<s<<" in destination node "<<nid<<std::endl;
    if (std::find((dest_flow[nid]).begin(), (dest_flow[nid]).end(), s)!=(dest_flow[nid]).end()) {
       
      /*       std::cout<<"*DestRate flowid "<<it->second<<" "<<Simulator::Now ().GetSeconds () << " " << destRate <<" "<<csfq_rate<<" "<< nid << " " << N << " " << short_rate << std::endl; */
      
        //current_rate += rate;

      }

    }
  }
  uint32_t total_flows = error_vector.size() + nonerror_vector.size();
  std::cout<<"common_utils.cc CheckIpv4Rates: flows less than 0.1 error "<<error_vector.size()<<std::endl;
  if(error_vector.size() >= 0.95 * total_flows) {
    // 95th percentile reached.. how many epochs since 95th percentile reached?
    std::cout<<"common_utils.cc CheckIpv4Rates: 95th percentil flows match continuous count "<<ninety_fifth<<" epoch "<<getEpochNumber()<<std::endl;
    ninety_fifth++;
  } else {
    ninety_fifth = 0;
  }

  double max_iterations= 250.0;
  if(ninety_fifth > max_iterations) {
    std::cout<<"common_utils.cc CheckIpv4Rates: More than "<<max_iterations<<" iterations of goodness.. moving on "<<Simulator::Now().GetSeconds()<<std::endl;
    std::cout<<"common_utils.cc CheckIpv4Rates: 95TH CONVERGED TIME "<<Simulator::Now().GetSeconds()-LastEventTime-max_iterations*sampling_interval<<" "<<Simulator::Now().GetSeconds()<<" epoch "<<getEpochNumber()<<std::endl;
    std::cout<<"common_utils.cc CheckIpv4Rates: Details "<<Simulator::Now().GetSeconds()<<" Lastevent "<<LastEventTime<<std::endl;
    move_to_next();
  }
  std::cout<<"common_utils.cc CheckIpv4Rates: " << Simulator::Now().GetSeconds()<<" TotalRate "<<current_rate<<std::endl;
  
  // check queue size every sampling_interval seconds
  Simulator::Schedule (Seconds (sampling_interval), &CheckIpv4Rates, allNodes);
  //if(Simulator::Now().GetSeconds() >= sim_time) {
  //  Simulator::Stop();
  //}
}

void printlink(Ptr<Node> n1, Ptr<Node> n2)
{
  std::cout<<"printlink: link setup between node "<<n1->GetId()<<" and node "<<n2->GetId()<<std::endl;
} 

Ipv4InterfaceContainer assignAddress(NetDeviceContainer dev, uint32_t subnet_index)
{
    /** assigining ip address **/

    Ipv4InterfaceContainer intf;

    std::ostringstream subnet;
    Ipv4AddressHelper ipv4;
    NS_LOG_UNCOND("Assigning subnet index "<<subnet_index);
    subnet<<"10.1."<<subnet_index<<".0";
    ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
    intf = ipv4.Assign (dev);
    return intf;

}


