#include <string>
#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;


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
