#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TrafficControlExample");

int
main (int argc, char *argv[])
{
  double simulationTime = 10; //seconds
  std::string transportProt = "Udp";
  std::string socketType;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  if (transportProt.compare ("Tcp") == 0)
    {
      socketType = "ns3::TcpSocketFactory";
    }
  else
    {
      socketType = "ns3::UdpSocketFactory";
    }

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
  pointToPoint.SetQueue ("ns3::DropTailQueue", "Mode", StringValue ("QUEUE_MODE_PACKETS"), "MaxPackets", UintegerValue (1));

  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get(0),nodes.Get(1));
  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get(1),nodes.Get(2));

  InternetStackHelper stack;
  stack.Install (nodes);

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::RedQueueDisc");
  // Add the internal queue used by Red
  tch.AddInternalQueues (handle, 1, "ns3::DropTailQueue", "MaxPackets", UintegerValue (10000));
  QueueDiscContainer qdiscs1 = tch.Install (devices1);
  QueueDiscContainer qdiscs2 = tch.Install (devices2);

  Ipv4AddressHelper address1;
  address1.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address1.Assign (devices1);

  Ipv4AddressHelper address2;
  address2.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address2.Assign (devices2);

 Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //Flow
  uint16_t port = 7;
  Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper (socketType, localAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (2));

  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 0.1));

  uint32_t payloadSize = 1448;
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));

  OnOffHelper onoff (socketType, Ipv4Address::GetAny ());
  onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onoff.SetAttribute ("DataRate", StringValue ("50Mbps")); //bit/s
  ApplicationContainer apps;

  AddressValue remoteAddress (InetSocketAddress (interfaces2.GetAddress (1), port));
  onoff.SetAttribute ("Remote", remoteAddress);
  apps.Add (onoff.Install (nodes.Get (0)));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (simulationTime + 0.1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (simulationTime + 5));
  Simulator::Run ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
  std::cout << "  Tx Packets:   " << stats[1].txPackets << std::endl;
  std::cout << "  Tx Bytes:   " << stats[1].txBytes << std::endl;
  std::cout << "  Offered Load: " << stats[1].txBytes * 8.0 / (stats[1].timeLastTxPacket.GetSeconds () - stats[1].timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  Rx Packets:   " << stats[1].rxPackets << std::endl;
  std::cout << "  Rx Bytes:   " << stats[1].rxBytes << std::endl;
  std::cout << "  Packets Dropped by Queue Disc:   " << stats[1].packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC] << std::endl;
  std::cout << "  Bytes Dropped by Queue Disc:   " << stats[1].bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC] << std::endl;
  std::cout << "  Packets Dropped by NetDevice:   " << stats[1].packetsDropped[Ipv4FlowProbe::DROP_QUEUE] << std::endl;
  std::cout << "  Bytes Dropped by NetDevice:   " << stats[1].bytesDropped[Ipv4FlowProbe::DROP_QUEUE] << std::endl;
  std::cout << "  Throughput: " << stats[1].rxBytes * 8.0 / (stats[1].timeLastRxPacket.GetSeconds () - stats[1].timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  Mean delay:   " << stats[1].delaySum.GetSeconds () / stats[1].rxPackets << std::endl;
  std::cout << "  Mean jitter:   " << stats[1].jitterSum.GetSeconds () / (stats[1].rxPackets - 1) << std::endl;

  Simulator::Destroy ();

  return 0;
}
