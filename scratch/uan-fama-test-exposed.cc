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
 * \file uan-fama-test.cc
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
#include "uan-fama-test-exposed.h"
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
  : m_numNodes (20),
    m_dataRate (1000),
    m_depth (70),
    m_boundary (20),
    m_packetSize (23),
    m_bytesTotal (0),
    m_cwMin (125),
    m_cwMax (120),
    m_cwStep (10),
    m_avgs (10),
    m_slotTime (Seconds (0.05)),
    m_simTime (Seconds (100)),
    m_gnudatfile ("uan-fama-test-e.gpl"),
    m_asciitracefile ("uan-fama-test.asc"),
    m_bhCfgFile ("uan-apps/dat/default.cfg")
	,m_maxOfferedLoad(100)
{
    m_randExp = CreateObject<ExponentialRandomVariable> ();
	m_randExp->SetAttribute ("Mean", DoubleValue (0.3));
    m_randExp->SetAttribute ("Bound", DoubleValue (0.9));
	m_doExposed = true; 
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
	 double accumEnergy = 0;
	for (uint32_t i = 0; i < m_numNodes; i++)
    {
		accumEnergy += cont.Get(i)->GetTotalEnergyConsumption();
	}
	double sinkEnergy = (cont.Get(m_numNodes)->GetTotalEnergyConsumption()-m_sinkPower.back());
	if (m_doExposed) sinkEnergy = ( sinkEnergy+cont.Get(m_numNodes+1)->GetTotalEnergyConsumption()-m_sinkPower.back()) / 2 ;
	m_powerBuffer.push_back(accumEnergy / m_numNodes);
	
    NS_LOG_DEBUG (Simulator::Now ().GetSeconds () <<"  "<< (accumEnergy / m_numNodes) - m_powerBuffer.back() << " Node Power Consumed "<<sinkEnergy<<" Sink Energy Comsumed") ;
	m_sinkPower.push_back(sinkEnergy );

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
	avgPower = m_sinkPower.back();
	m_sinkPower.clear ();
	m_sinkPower.push_back(avgPower);
	
	
	
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
  Ptr<UniformRandomVariable> urv = CreateObject<UniformRandomVariable> ();
  double rsum = 0;
  double minr = 270;

  	if(m_doExposed){
    for (uint32_t i = 0; i < m_numNodes/2; i++)
      {
        double x = 250+urv->GetValue (0, m_boundary);
        double y = 240+urv->GetValue (0, m_boundary);//CenterA = 260,250,bound = 20
        double newr = std::sqrt ((x - 80) * (x - 80)
                            + (y - 250) * (y -250));
        rsum += newr;
        minr = std::min (minr, newr);
        Ptr<MobilityModel> mp = (*it)->GetObject<MobilityModel> ();
        mp->SetPosition (Vector (x, y, m_depth));

      }

    	for (uint32_t i = m_numNodes/2; i < m_numNodes; i++)
        {
          double x = 410+urv->GetValue (0, m_boundary);
          double y = 240+urv->GetValue (0, m_boundary);//CenterB: 420,250,bound = 20
          double newr = std::sqrt ((x - 600) * (x - 600)
                            + (y - 250) * (y -  250));
          rsum += newr;
          minr = std::min (minr, newr);
          Ptr<MobilityModel> mp = (*it)->GetObject<MobilityModel> ();
          mp->SetPosition (Vector (x, y, m_depth));

        }
     }
	 else{
	   for (uint32_t i = 0; i < m_numNodes; i++)
      {
        double x = 250+urv->GetValue (0, m_boundary);
        double y = 240+urv->GetValue (0, m_boundary);//CenterA = 260,250,bound = 20
        double newr = std::sqrt ((x - 80) * (x - 80)
                            + (y - 250) * (y -250));
        rsum += newr;
        minr = std::min (minr, newr);
        Ptr<MobilityModel> mp = (*it)->GetObject<MobilityModel> ();
        mp->SetPosition (Vector (x, y, m_depth));

      }
	 }
   
	 // if (m_offeredLoad<=1)
    //Simulator::Stop(m_simTime);
	NS_LOG_DEBUG ("Mean range from gateway: " << rsum / m_numNodes
                                              << "    min. range " << minr);
}

void
Experiment::ReceivePacket (Ptr<Packet> packet, const UanAddress &src)
{
 
      m_bytesTotal += packet->GetSize ();

}

void
Experiment::Run (UanHelper &uan)
{
  //uan.SetMac ("ns3::UanMacTlohiNW");
  //uan.SetMac ("ns3::UanMacMacaNW");
  //uan.SetMac ("ns3::UanMacFamaNW");
  //uan.SetMac ("ns3::UanMacAlohaCs", "CW", UintegerValue (34));
  //uan.SetMac ("ns3::UanMacAloha");
  uan.SetMac ("ns3::UanMacCw", "CW", UintegerValue (m_cwMin), "SlotTime", TimeValue (m_slotTime));

  NodeContainer nc = NodeContainer ();
  NodeContainer sink = NodeContainer ();
  if (!m_doExposed) m_numNodes = 10;
  else m_numNodes = 20;
  nc.Create (m_numNodes);
  if (m_doExposed){
    sink.Create (2);
  }
  else{
    sink.Create(1);
  }

  PacketSocketHelper socketHelper;
  socketHelper.Install (nc);
  socketHelper.Install (sink);

#ifdef UAN_PROP_BH_INSTALLED
  Ptr<UanPropModelBh> prop = CreateObjectWithAttributes<UanPropModelBh> ("ConfigFile", StringValue ("exbhconfig.cfg"));
#else 
  Ptr<UanPropModelThorp> prop = CreateObjectWithAttributes<UanPropModelThorp> ("SpreadCoef" , DoubleValue(4.7));
#endif //UAN_PROP_BH_INSTALLED
  Ptr<UanChannel> channel = CreateObjectWithAttributes<UanChannel> ("PropagationModel", PointerValue (prop));

  //Create net device and nodes with UanHelper
  NetDeviceContainer devices = uan.Install (nc, channel);
  NetDeviceContainer sinkdev = uan.Install (sink, channel);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();

  {
    Ptr<UniformRandomVariable> urv = CreateObject<UniformRandomVariable> ();
    pos->Add (Vector (80,250,m_depth));//SinkA: 80,250
	
    double rsum = 0;

    double minr =270;
	if(m_doExposed){
	pos->Add (Vector (600,250,m_depth));//SinkB: 6000,250
    for (uint32_t i = 0; i < m_numNodes/2; i++)
      {
        double x = 250+urv->GetValue (0, m_boundary);
        double y = 240+urv->GetValue (0, m_boundary);//CenterA = 260,250,bound = 20
        double newr = std::sqrt ((x - 80) * (x - 80)
                            + (y - 250) * (y -250));
        rsum += newr;
        minr = std::min (minr, newr);
        pos->Add (Vector (x, y, m_depth));

      }

    	for (uint32_t i = m_numNodes/2; i < m_numNodes; i++)
        {
          double x = 410+urv->GetValue (0, m_boundary);
          double y = 240+urv->GetValue (0, m_boundary);//CenterB: 420,250,bound = 20
          double newr = std::sqrt ((x - 600) * (x - 600)
                            + (y - 250) * (y -  250));
          rsum += newr;
          minr = std::min (minr, newr);
          pos->Add (Vector (x, y, m_depth));

        }
     }
	 else{
	   for (uint32_t i = 0; i < m_numNodes; i++)
      {
        double x = 250+urv->GetValue (0, m_boundary);
        double y = 240+urv->GetValue (0, m_boundary);//CenterA = 260,250,bound = 20
        double newr = std::sqrt ((x - 80) * (x - 80)
                            + (y - 250) * (y -250));
        rsum += newr;
        minr = std::min (minr, newr);
        pos->Add (Vector (x, y, m_depth));

      }
	 }
    NS_LOG_DEBUG ("Mean range from gateway: " << rsum / m_numNodes
                                              << "    min. range " << minr);

    mobility.SetPositionAllocator (pos);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (sink);

    NS_LOG_DEBUG ("Position of sink: "
                  << sink.Get (0)->GetObject<MobilityModel> ()->GetPosition ());
    mobility.Install (nc);

	DeviceEnergyModelContainer cont;
	for (uint32_t i = 0; i < nc.GetN (); i++)
    {
	  BasicEnergySourceHelper energyHelper;
      AcousticModemEnergyModelHelper modemHelper;   
	  energyHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (10000.0));  
      modemHelper.Set ("IdlePowerW",DoubleValue(0.024));
      modemHelper.Set ("RxPowerW",DoubleValue(0.024));
      modemHelper.Set ("TxPowerW",DoubleValue(0.12));
      modemHelper.Set ("SleepPowerW",DoubleValue(0.000008));	
      energyHelper.Install (nc.Get(i));
	  Ptr<EnergySource> source = nc.Get(i)->GetObject<EnergySourceContainer> ()->Get (0); 
      cont.Add( modemHelper.Install (devices.Get(i),source)); 
    }
	  BasicEnergySourceHelper energyHelper;
      AcousticModemEnergyModelHelper modemHelper;   
	  energyHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (10000.0));  
      modemHelper.Set ("IdlePowerW",DoubleValue(0.024));
      modemHelper.Set ("RxPowerW",DoubleValue(0.024));
      modemHelper.Set ("TxPowerW",DoubleValue(0.12));
      modemHelper.Set ("SleepPowerW",DoubleValue(0.000008));	
      energyHelper.Install (sink.Get(0));
	  Ptr<EnergySource> source = sink.Get(0)->GetObject<EnergySourceContainer> ()->Get (0); 
      cont.Add( modemHelper.Install (sinkdev.Get(0),source)); 
	  
	 if(m_doExposed){
	  BasicEnergySourceHelper energyHelper;
      AcousticModemEnergyModelHelper modemHelper;   
	  energyHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (10000.0));  
      modemHelper.Set ("IdlePowerW",DoubleValue(0.024));
      modemHelper.Set ("RxPowerW",DoubleValue(0.024));
      modemHelper.Set ("TxPowerW",DoubleValue(0.12));
      modemHelper.Set ("SleepPowerW",DoubleValue(0.000008));	
      energyHelper.Install (sink.Get(1));
	  Ptr<EnergySource> source = sink.Get(1)->GetObject<EnergySourceContainer> ()->Get (0); 
      cont.Add( modemHelper.Install (sinkdev.Get(1),source)); 
	 }
	
	
	
	m_powerBuffer.clear();
    m_powerBuffer.push_back(0);
	m_sinkPower.clear();
    m_sinkPower.push_back(0);
	
   /* PacketSocketAddress socket;
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
	*/
	
	for(uint32_t i = 0; i < m_numNodes; i++){
	  Ptr<UanMac> mac = DynamicCast<UanNetDevice> (devices.Get(i))->GetMac ();
	  Ptr<UanMacCw> macFama = DynamicCast<UanMacCw> (mac);
	  UanAddress uanAddress(i);
	  macFama->SetAddress(uanAddress);
	}
	
	
	Ptr<UanMac> mac = DynamicCast<UanNetDevice> (sinkdev.Get(0))->GetMac ();
	Ptr<UanMacCw> macFama = DynamicCast<UanMacCw> (mac);
	macFama->SetForwardUpCb(MakeCallback(&Experiment::ReceivePacket, this));
	UanAddress uanAddress(m_numNodes);
	macFama->SetAddress(uanAddress);
	
    if(m_doExposed){
	Ptr<UanMac> mac = DynamicCast<UanNetDevice> (sinkdev.Get(1))->GetMac ();
	Ptr<UanMacCw> macFama = DynamicCast<UanMacCw> (mac);
	macFama->SetForwardUpCb(MakeCallback(&Experiment::ReceivePacket, this));
	UanAddress uanAddress(m_numNodes+1);
	macFama->SetAddress(uanAddress);
	}
	
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
	if(m_doExposed){
	for (uint8_t src = 0; src < m_numNodes/2; src++)
    {
      UanAddress uanDst (m_numNodes);
      double time = m_randExp->GetValue();
      Simulator::Schedule(Seconds (time), &Experiment::Send, this, devices, uanDst, src);
    }
	for(uint8_t src = m_numNodes/2; src < m_numNodes; src++)
	 {
      UanAddress uanDst (m_numNodes+1);
      double time = m_randExp->GetValue();
      Simulator::Schedule(Seconds (time), &Experiment::Send, this, devices, uanDst, src);
    }
	}
	else{
	  for (uint8_t src = 0; src < m_numNodes; src++)
      {
        UanAddress uanDst (m_numNodes);
        double time = m_randExp->GetValue();
        Simulator::Schedule(Seconds (time), &Experiment::Send, this, devices, uanDst, src);
      }
	}
	Simulator::Stop (nextEvent+Seconds(0.5)+ m_simTime);
    //apps.Stop (nextEvent + m_simTime);
	//appSinks.Stop (nextEvent + m_simTime);

    /*Ptr<Node> sinkNode = sink.Get (0);
    TypeId psfid = TypeId::LookupByName ("ns3::PacketSocketFactory");
    if (sinkNode->GetObject<SocketFactory> (psfid) == 0)
      {
        Ptr<PacketSocketFactory> psf = CreateObject<PacketSocketFactory> ();
        sinkNode->AggregateObject (psf);
      }
    Ptr<Socket> sinkSocket = Socket::CreateSocket (sinkNode, psfid);
    sinkSocket->Bind (socket);
    //sinkSocket->SetRecvCallback (MakeCallback (&Experiment::ReceivePacket, this));
	*/

	
    m_bytesTotal = 0;

    std::ofstream ascii (m_asciitracefile.c_str ());
    if (!ascii.is_open ())
      {
        NS_FATAL_ERROR ("Could not open ascii trace file: "
                        << m_asciitracefile);
      }
    uan.EnableAsciiAll (ascii);

    Simulator::Run ();
    //sinkNode = 0;
    //sinkSocket = 0;
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
void
Experiment::Send (NetDeviceContainer &devices, UanAddress &uanAddress, uint8_t src)
{
  Ptr<Packet> pkt = Create<Packet> (m_packetSize);

  //DupTag dupTag(DupTag::GetNewId ());
  //pkt->AddPacketTag(dupTag);

  //m_pktSent[dupTag.m_id] = Simulator::Now ().GetSeconds ();

  devices.Get (src)->Send (pkt, uanAddress, 0);
  //m_generatedPackets++;
UanAddress uanDst (m_numNodes);
  if(src > m_numNodes/2-1 && m_doExposed){
    uanDst=(m_numNodes + 1);
  }

  
  double time = m_randExp->GetValue();
  //double sendTime = Simulator::Now().GetSeconds () + time;
  //if ( m_generatedPackets < 50000)
   
      Simulator::Schedule(Seconds (time), &Experiment::Send, this, devices, uanDst, src);
  
}
int
main (int argc, char **argv)
{

  LogComponentEnable ("UanCwExample", LOG_LEVEL_ALL);
  //LogComponentEnable ("UanMacAlohaCs", LOG_LEVEL_ALL);
  //LogComponentEnable ("UanMacTlohiNW", LOG_LEVEL_ALL);
  //LogComponentEnable ("UanMacFamaNW", LOG_LEVEL_ALL);
  
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
  for(exp.m_offeredLoad=exp.m_maxOfferedLoad; exp.m_offeredLoad>=1;exp.m_offeredLoad-=4)
  {
  	exp.m_randExp->SetAttribute ("Mean", DoubleValue (exp.m_offeredLoad*0.1));
    exp.m_randExp->SetAttribute ("Bound", DoubleValue (exp.m_offeredLoad*0.3));
  
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

