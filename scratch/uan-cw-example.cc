/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
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
 * Author: Leonard Tracy <lentracy@gmail.com>
 */


/**
 * \file uan-cw-example.cc
 * \ingroup uan
 * 
 * This example showcases the "CW-MAC" described in System Design Considerations
 * for Undersea Networks article in the IEEE Journal on Selected Areas of
 * Communications 2008 by Nathan Parrish, Leonard Tracy and Sumit Roy.
 * The MAC protocol is implemented in the class UanMacCw.  CW-MAC is similar
 * in nature to the IEEE 802.11 DCF with a constant backoff window.
 * It requires two parameters to be set, the slot time and
 * the contention window size.  The contention window size is
 * the backoff window size in slots, and the slot time is
 * the duration of each slot.  These parameters should be set
 * according to the overall network size, internode spacing and
 * the number of nodes in the network.
 * 
 * This example deploys nodes randomly (according to RNG seed of course)
 * in a finite square region with the X and Y coordinates of the nodes
 * distributed uniformly.  The CW parameter is varied throughout
 * the simulation in order to show the variation in throughput
 * with respect to changes in CW.
 */
#include "uan-cw-example.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/stats-module.h"
#include "ns3/applications-module.h"
#include "ns3/simple-device-energy-model.h"
#include "ns3/basic-energy-source-helper.h"    
#include "ns3/acoustic-modem-energy-model-helper.h"
#include "ns3/acoustic-modem-energy-model.h"  
#include "ns3/double.h"
	
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UanCwExample");

Experiment::Experiment () 
  : m_numNodes (15),
    m_dataRate (1000),
    m_depth (70),
    m_boundary (500),
    m_packetSize (23),
    m_bytesTotal (0),
    m_cwMin (125),
    m_cwMax (120),
    m_cwStep (10),
    m_avgs (10),
    m_slotTime (Seconds (0.06)),
    m_simTime (Seconds (100)),
    m_gnudatfile ("uan-cw-example.gpl"),
    m_asciitracefile ("uan-cw-example.asc"),
    m_bhCfgFile ("uan-apps/dat/default.cfg")
	,m_maxOfferedLoad(10)
{
}

void
Experiment::ResetData ()
{
  NS_LOG_DEBUG (Simulator::Now ().GetSeconds () << "  Resetting data  "<<(m_bytesTotal * 8.0 / m_simTime.GetSeconds ()));
  m_throughputs.push_back (m_bytesTotal * 8.0 / m_simTime.GetSeconds ());
  m_bytesTotal = 0;
}

void
	Experiment::ReportPower (DeviceEnergyModelContainer cont)
    {
	 NS_LOG_DEBUG (Simulator::Now ().GetSeconds () <<"  "<< cont.Get(0)->GetTotalEnergyConsumption()-m_powerBuffer.back() << " Power Consumed " );
	
	 m_powerBuffer.push_back(cont.Get(0)->GetTotalEnergyConsumption());
	
    }
void
Experiment::ReportData()
{
	 NS_ASSERT (m_throughputs.size () == m_avgs);
	double avgThroughput = 0.0;
	double avgPower = m_powerBuffer.back()-m_powerBuffer[0];
	for (uint32_t i=0; i<m_avgs; i++)
    {
		avgThroughput += m_throughputs[i];
    }
	avgThroughput /= m_avgs;
	//avgThroughput /= m_offeredLoad*m_dataRate;
	avgPower /= m_avgs;
  
	m_data.Add (m_offeredLoad, avgThroughput, avgPower);
	m_throughputs.clear ();

	//Config::Set ("/NodeList/*/DeviceList/*/Mac/CW", UintegerValue (cw + m_cwStep));

	SeedManager::SetRun (SeedManager::GetRun () + 1);

	NS_LOG_DEBUG ("Average for offered load=" << m_offeredLoad << " over " << m_avgs << " runs: " << avgThroughput);
	NS_LOG_DEBUG ("Average Power for offered load=" << m_offeredLoad << " over " << m_avgs << " runs: " << avgPower);
  
  
	avgPower = m_powerBuffer.back();
	m_powerBuffer.clear ();
	m_powerBuffer.push_back(avgPower);
}
void
Experiment::IncrementCw (uint32_t cw)
{
  NS_ASSERT (m_throughputs.size () == m_avgs);

  double avgThroughput = 0.0;
  double avgPower = m_powerBuffer.back()-m_powerBuffer[0];
  for (uint32_t i=0; i<m_avgs; i++)
    {
      avgThroughput += m_throughputs[i];
    }
  avgThroughput /= m_avgs;
  avgPower /= m_avgs;
  
  m_data.Add (cw, avgThroughput, avgPower);
  m_throughputs.clear ();

  Config::Set ("/NodeList/*/DeviceList/*/Mac/CW", UintegerValue (cw + m_cwStep));

  SeedManager::SetRun (SeedManager::GetRun () + 1);

  NS_LOG_DEBUG ("Average for cw=" << cw << " over " << m_avgs << " runs: " << avgThroughput);
  NS_LOG_DEBUG ("Average Power for cw=" << cw << " over " << m_avgs << " runs: " << avgPower);
  
  
  avgPower = m_powerBuffer.back();
  m_powerBuffer.clear ();
  m_powerBuffer.push_back(avgPower);
  
}
void
Experiment::UpdatePositions (NodeContainer &nodes)
{

  NS_LOG_DEBUG (Simulator::Now ().GetSeconds () << " Updating positions");
  NodeContainer::Iterator it = nodes.Begin ();
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  for (; it != nodes.End (); it++)
    {
      Ptr<MobilityModel> mp = (*it)->GetObject<MobilityModel> ();
      mp->SetPosition (Vector (uv->GetValue (0, m_boundary), uv->GetValue (0, m_boundary), 70.0));
    }
	 // if (m_offeredLoad<=1)
    //Simulator::Stop(m_simTime);
}

void
Experiment::ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;

  while ((packet = socket->Recv ()))
    {
      m_bytesTotal += packet->GetSize ();
    }
  packet = 0;
}

void
Experiment::Run (UanHelper &uan)
{
  uan.SetMac ("ns3::UanMacFamaNW");
  //uan.SetMac ("ns3::UanMacAloha");
  //uan.SetMac ("ns3::UanMacCw", "CW", UintegerValue (m_cwMin), "SlotTime", TimeValue (m_slotTime));//uan.SetMac ("ns3::UanMacAloha");
  NodeContainer nc = NodeContainer ();
  NodeContainer sink = NodeContainer ();
  nc.Create (m_numNodes);
  sink.Create (1);

  PacketSocketHelper socketHelper;
  socketHelper.Install (nc);
  socketHelper.Install (sink);

#ifdef UAN_PROP_BH_INSTALLED
  Ptr<UanPropModelBh> prop = CreateObjectWithAttributes<UanPropModelBh> ("ConfigFile", StringValue ("exbhconfig.cfg"));
#else 
  Ptr<UanPropModelIdeal> prop = CreateObjectWithAttributes<UanPropModelIdeal> ();
#endif //UAN_PROP_BH_INSTALLED
  Ptr<UanChannel> channel = CreateObjectWithAttributes<UanChannel> ("PropagationModel", PointerValue (prop));

  //Create net device and nodes with UanHelper
  NetDeviceContainer devices = uan.Install (nc, channel);
  NetDeviceContainer sinkdev = uan.Install (sink, channel);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();

  {
    Ptr<UniformRandomVariable> urv = CreateObject<UniformRandomVariable> ();
    pos->Add (Vector (m_boundary / 2.0, m_boundary / 2.0, m_depth));
    double rsum = 0;

    double minr = 2 * m_boundary;
    for (uint32_t i = 0; i < m_numNodes; i++)
      {
        double x = urv->GetValue (0, m_boundary);
        double y = urv->GetValue (0, m_boundary);
        double newr = std::sqrt ((x - m_boundary / 2.0) * (x - m_boundary / 2.0)
                            + (y - m_boundary / 2.0) * (y - m_boundary / 2.0));
        rsum += newr;
        minr = std::min (minr, newr);
        pos->Add (Vector (x, y, m_depth));

      }
    NS_LOG_DEBUG ("Mean range from gateway: " << rsum / m_numNodes
                                              << "    min. range " << minr);

    mobility.SetPositionAllocator (pos);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (sink);

    NS_LOG_DEBUG ("Position of sink: "
                  << sink.Get (0)->GetObject<MobilityModel> ()->GetPosition ());
    mobility.Install (nc);

	BasicEnergySourceHelper energyHelper;
    AcousticModemEnergyModelHelper modemHelper;   
	energyHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (10000.0));  
    modemHelper.Set ("IdlePowerW",DoubleValue(0.024));
    modemHelper.Set ("RxPowerW",DoubleValue(0.024));
    modemHelper.Set ("TxPowerW",DoubleValue(0.12));
    modemHelper.Set ("SleepPowerW",DoubleValue(0.000008));	
	energyHelper.Install (nc.Get(0));
	Ptr<EnergySource> source = nc.Get(0)->GetObject<EnergySourceContainer> ()->Get (0); 
	DeviceEnergyModelContainer cont = modemHelper.Install (devices.Get(0),source); 
    m_powerBuffer.clear();
    m_powerBuffer.push_back(0);
	
    PacketSocketAddress socket;
    socket.SetSingleDevice (sinkdev.Get (0)->GetIfIndex ());
    socket.SetPhysicalAddress (sinkdev.Get (0)->GetAddress ());
    socket.SetProtocol (0);


    UanNodeApplicationHelper app ("ns3::PacketSocketFactory", Address (socket));
    app.SetAttribute ("MaxIntervalTime", DoubleValue (m_offeredLoad * 0.1));
    app.SetAttribute ("MinIntervalTime", DoubleValue (m_offeredLoad * 0.1 - 0.05));//0.01));
    app.SetAttribute ("DataRate", DataRateValue (m_dataRate));
    app.SetAttribute ("PacketSize", UintegerValue (m_packetSize));

    ApplicationContainer apps = app.Install (nc);
    apps.Start (Seconds (0.5));
    
	PacketSinkHelper appSink ("ns3::PacketSocketFactory", Address (socket));
	ApplicationContainer appSinks = appSink.Install(sink);
	appSinks.Start (Seconds (0.5));
	

    Time nextEvent = Seconds (0.5);
    for (uint32_t an = 0; an < m_avgs; an++)
        {
        nextEvent += m_simTime;
        Simulator::Schedule (nextEvent, &Experiment::ReportPower, this, cont);
		Simulator::Schedule (nextEvent, &Experiment::ResetData, this);
        Simulator::Schedule (nextEvent, &Experiment::UpdatePositions, this, nc);
		
		//app.SetAttribute ("DataRate", DataRateValue (m_dataRate));
		//apps = app.Install (nc);
		}
    Simulator::Schedule (nextEvent, &Experiment::ReportData, this);
	Simulator::Stop (nextEvent+Seconds(0.5)+ m_simTime);
    apps.Stop (nextEvent + m_simTime);
	appSinks.Stop (nextEvent + m_simTime);

    Ptr<Node> sinkNode = sink.Get (0);
    TypeId psfid = TypeId::LookupByName ("ns3::PacketSocketFactory");
    if (sinkNode->GetObject<SocketFactory> (psfid) == 0)
      {
        Ptr<PacketSocketFactory> psf = CreateObject<PacketSocketFactory> ();
        sinkNode->AggregateObject (psf);
      }
    Ptr<Socket> sinkSocket = Socket::CreateSocket (sinkNode, psfid);
    sinkSocket->Bind (socket);
    sinkSocket->SetRecvCallback (MakeCallback (&Experiment::ReceivePacket, this));

    m_bytesTotal = 0;

    std::ofstream ascii (m_asciitracefile.c_str ());
    if (!ascii.is_open ())
      {
        NS_FATAL_ERROR ("Could not open ascii trace file: "
                        << m_asciitracefile);
      }
    uan.EnableAsciiAll (ascii);

    Simulator::Run ();
    sinkNode = 0;
    sinkSocket = 0;
    pos = 0;
    channel = 0;
    prop = 0;
    for (uint32_t i=0; i < nc.GetN (); i++)
      {
        nc.Get (i) = 0;
      }
    for (uint32_t i=0; i < sink.GetN (); i++)
      {
        sink.Get (i) = 0;
      }

    for (uint32_t i=0; i < devices.GetN (); i++)
      {
        devices.Get (i) = 0;
      }
    for (uint32_t i=0; i < sinkdev.GetN (); i++)
      {
        sinkdev.Get (i) = 0;
      }

    Simulator::Destroy ();
    //return m_data;
  }
}

int
main (int argc, char **argv)
{

  LogComponentEnable ("UanCwExample", LOG_LEVEL_ALL);

  Experiment exp;

  std::string gnudatfile ("cwexpgnuout.dat");
  std::string perModel = "ns3::UanPhyPerGenDefault";
  std::string sinrModel = "ns3::UanPhyCalcSinrDefault";

  CommandLine cmd;
  cmd.AddValue ("NumNodes", "Number of transmitting nodes", exp.m_numNodes);
  cmd.AddValue ("Depth", "Depth of transmitting and sink nodes", exp.m_depth);
  cmd.AddValue ("RegionSize", "Size of boundary in meters", exp.m_boundary);
  cmd.AddValue ("PacketSize", "Generated packet size in bytes", exp.m_packetSize);
  cmd.AddValue ("DataRate", "DataRate in bps", exp.m_dataRate);
  cmd.AddValue ("CwMin", "Min CW to simulate", exp.m_cwMin);
  cmd.AddValue ("CwMax", "Max CW to simulate", exp.m_cwMax);
  cmd.AddValue ("CwStep", "CW increment step", exp.m_cwStep);
  cmd.AddValue ("SlotTime", "Slot time duration", exp.m_slotTime);
  cmd.AddValue ("Averages", "Number of topologies to test for each cw point", exp.m_avgs);
  cmd.AddValue ("GnuFile", "Name for GNU Plot output", exp.m_gnudatfile);
  cmd.AddValue ("PerModel", "PER model name", perModel);
  cmd.AddValue ("SinrModel", "SINR model name", sinrModel);
  cmd.Parse (argc, argv);

  ObjectFactory obf;
  obf.SetTypeId (perModel);
  Ptr<UanPhyPer> per = obf.Create<UanPhyPer> ();
  obf.SetTypeId (sinrModel);
  Ptr<UanPhyCalcSinr> sinr = obf.Create<UanPhyCalcSinr> ();
  //double testTimes[20]={0.0} 
  for(exp.m_offeredLoad=exp.m_maxOfferedLoad; exp.m_offeredLoad>=1;exp.m_offeredLoad-=1)
  {
  UanHelper uan;
  UanTxMode mode;
  mode = UanTxModeFactory::CreateMode (UanTxMode::FSK, exp.m_dataRate,
                                       exp.m_dataRate, 12000,
                                       exp.m_dataRate, 2,
                                       "Default mode");
  UanModesList myModes;
  myModes.AppendMode (mode);

  uan.SetPhy ("ns3::UanPhyGen",
              "PerModel", PointerValue (per),
              "SinrModel", PointerValue (sinr),
              "SupportedModes", UanModesListValue (myModes));

 
  
  exp.Run (uan);
	
	}
	
  Gnuplot gp;
  Gnuplot3dDataset ds;
  gp.AddDataset (exp.m_data);

  std::ofstream of (exp.m_gnudatfile.c_str ());
  if (!of.is_open ())
    {
      NS_FATAL_ERROR ("Can not open GNU Plot outfile: " << exp.m_gnudatfile);
    }
  gp.GenerateOutput (of);

  per = 0;
  sinr = 0;
  
  of.close();

}

