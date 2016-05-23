/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/uan-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/stats-module.h"
#include "ns3/applications-module.h"

#include "ns3/simple-device-energy-model.h"   
#include "ns3/acoustic-modem-energy-model-helper.h"
#include "ns3/acoustic-modem-energy-model.h"  
#include "ns3/infinite-energy-source.h"

#include "ns3/double.h"

#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WakeupTestSimple");

class SimpleTest
{
public:
  SimpleTest();
  virtual ~SimpleTest();
  void StartRun();
  void SetPhyMac();
  void SetupWakeup(NodeContainer& nc, NodeContainer& sink);
  void ResetData();
  void ReportData();
  void ReportPower(NodeContainer& nc, NodeContainer& sink);
  void UpdatePositions (NodeContainer &nodes);
  void SinkReceiveData (Ptr<Packet> pkt, const UanAddress &src);
  void DataSent(Ptr<Packet> pkt);
  void Send (NetDeviceContainer &devices, UanAddress &uanAddress, uint8_t src);
  
  uint32_t m_numNodes;
  uint32_t m_dataRate;
  double m_depth;
  double m_boundary;
  uint32_t m_packetSize;
  uint32_t m_avgs;
  uint32_t m_maxOfferedLoad;
  uint32_t m_offeredLoad;
  Time m_simTime;
  bool m_ack;
  double m_backoff;
  uint32_t m_sentPackets;
  uint32_t m_receivedPackets;
  uint32_t m_generatedPackets; 
  
  std::string m_gnudatfile;
  std::string m_asciitracefile; 
  
  bool m_isUltra;

  std::vector<double> m_energyNodes;
  std::vector<double> m_energySink;
  std::vector<uint32_t> m_PacketsCount;
  std::vector<double> m_throughputs;
  
  UanHelper m_uan;

  NetDeviceContainer devices;
  NetDeviceContainer devicesWakeup;
  NetDeviceContainer devicesWakeupHE;

  NetDeviceContainer sinkDevice;
  NetDeviceContainer sinkDeviceWakeup;
  NetDeviceContainer sinkDeviceWakeupHE;
  
  Gnuplot3dDataset m_plotData;
private:
  Ptr<UniformRandomVariable>  m_rand;
  Ptr<ExponentialRandomVariable> m_randExp;
};

SimpleTest::SimpleTest()

  : m_numNodes (15),
   m_dataRate (1000),
   m_depth (70),
   m_boundary (500),
   m_packetSize (23),
   m_avgs (10),
   m_maxOfferedLoad (100),
   m_offeredLoad(1),
   m_simTime (Seconds(100)),
   m_ack (true),
   m_backoff(0.5),
   m_sentPackets(0),
   m_receivedPackets(0),
   m_generatedPackets(0),
   
   m_gnudatfile ("uan-cw-example.gpl"),
   m_asciitracefile ("uan-cw-example.asc")
{   
   m_rand = CreateObject<UniformRandomVariable> ();
   m_randExp = CreateObject<ExponentialRandomVariable> ();
   m_randExp->SetAttribute ("Mean", DoubleValue (0.3));
   m_randExp->SetAttribute ("Bound", DoubleValue (0.9));
   m_isUltra = true;
}

SimpleTest::~SimpleTest()
{

}

void
SimpleTest::SetPhyMac()
{
  m_randExp->SetAttribute ("Mean", DoubleValue (m_offeredLoad*0.1));
  m_randExp->SetAttribute ("Bound", DoubleValue (m_offeredLoad*0.3));
  
  std::string perModel = "ns3::UanPhyPerGenDefault";
  std::string sinrModel = "ns3::UanPhyCalcSinrDefault";

  ObjectFactory obf;
  obf.SetTypeId (perModel);
  Ptr<UanPhyPer> per = obf.Create<UanPhyPer> ();
  obf.SetTypeId (sinrModel);
  Ptr<UanPhyCalcSinr> sinr = obf.Create<UanPhyCalcSinr> ();

  UanTxMode mode;
  mode = UanTxModeFactory::CreateMode (UanTxMode::FSK, m_dataRate,
                                       m_dataRate, 12000,
                                       m_dataRate, 2,
                                       "Default mode");
  UanModesList myModes;
  myModes.AppendMode (mode);

  m_uan.SetPhy ("ns3::UanPhyGen",
              "PerModel", PointerValue (per),
              "SinrModel", PointerValue (sinr),
              "SupportedModes", UanModesListValue (myModes));
  m_uan.SetMac ("ns3::UanMacWakeupTlohi");
}

void 
SimpleTest::SetupWakeup( NodeContainer& nc, NodeContainer& sink){

   nc.Create (m_numNodes);
   sink.Create (1);
   
  PacketSocketHelper socketHelper;
  socketHelper.Install (nc);
  socketHelper.Install (sink);

   
/*
  Ptr<UanPropModelBh> prop = CreateObjectWithAttributes<UanPropModelBh> ("ConfigFile", StringValue (m_bhCfgFile));
  Ptr<UanNoiseModel> noise = CreateObjectWithAttributes<UanNoiseModelDefault>();
  noise->SetAttribute ("Wind", DoubleValue(0));
  Ptr<UanChannel> channel = CreateObjectWithAttributes<UanChannel> ("PropagationModel", PointerValue (prop),
                                                                    "NoiseModel", PointerValue(noise));
*/
#ifdef UAN_PROP_BH_INSTALLED
  Ptr<UanPropModelBh> prop = CreateObjectWithAttributes<UanPropModelBh> ("ConfigFile", StringValue ("exbhconfig.cfg"));
#else 
  Ptr<UanPropModelIdeal> prop = CreateObjectWithAttributes<UanPropModelIdeal> ();
#endif //UAN_PROP_BH_INSTALLED
  Ptr<UanChannel> channel = CreateObjectWithAttributes<UanChannel> ("PropagationModel", PointerValue (prop));
  
  //Create net device and nodes with UanHelper
  devices = m_uan.Install (nc, channel);
  sinkDevice = m_uan.Install(sink, channel);
  //Create wakeup net device
  devicesWakeup = m_uan.Install (nc, channel);
  sinkDeviceWakeup = m_uan.Install(sink, channel);
  //Create wakeupHE net device
  devicesWakeupHE = m_uan.Install (nc, channel);
  sinkDeviceWakeupHE = m_uan.Install(sink, channel);
  // Set location
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();

  pos->Add (Vector (m_boundary/2.0, m_boundary/2.0, 1));
  for (uint32_t i = 0; i < m_numNodes; i++)
    {
      double x = m_rand->GetValue(0, m_boundary);
      double y = m_rand->GetValue(0, m_boundary);

      pos->Add (Vector (x, y, m_depth));
    }
  
  mobility.SetPositionAllocator (pos);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sink);
  mobility.Install (nc);

//  std::cerr << "Position of sink: "
//          << nc.Get (0)->GetObject<MobilityModel> ()->GetPosition () << std::endl;

  for (uint32_t i = 0; i < devices.GetN()+1; i++)
  {
      Ptr<UanPhyGen> phy = 0 ;
	  Ptr<UanMac> mac = 0 ;
	  Ptr<UanMacWakeupTlohi> macWakeup = 0 ;
	  Ptr<UanPhy> phyWakeup = 0;
	  Ptr<UanNetDevice> uanNetdevice = 0;
	  Ptr<Node> nodePtr = 0 ;
	  Ptr<NetDevice> wakeUpDevicePtr = 0 ;  
	  UanAddress uanAddress (i);
	  if (i == devices.GetN()){
	    
	    phy = DynamicCast<UanPhyGen> (DynamicCast<UanNetDevice> (sinkDevice.Get(0))->GetPhy ());
		mac = DynamicCast<UanNetDevice> (sinkDevice.Get(0))->GetMac ();
        macWakeup = DynamicCast<UanMacWakeupTlohi> (mac);
        phy->RegisterListener( &(*macWakeup));
		macWakeup->AttachPhy(phy);
		


        phyWakeup = DynamicCast<UanNetDevice> (sinkDeviceWakeup.Get(0))->GetPhy ();
        macWakeup->AttachWakeupPhy (phyWakeup);
        phyWakeup->SetMac (macWakeup);
        phyWakeup->RegisterListener( &(*macWakeup));


        //Afegir mac rts
		uanNetdevice = DynamicCast<UanNetDevice> (sinkDevice.Get(0));
        
		
		nodePtr = sink.Get(0);
		wakeUpDevicePtr = sinkDeviceWakeupHE.Get(0);
	  }
	  else{
	  
	    phy = DynamicCast<UanPhyGen> (DynamicCast<UanNetDevice> (devices.Get(i))->GetPhy ());
        mac = DynamicCast<UanNetDevice> (devices.Get(i))->GetMac ();
        macWakeup = DynamicCast<UanMacWakeupTlohi> (mac);
        phy->RegisterListener( &(*macWakeup));
		macWakeup->AttachPhy(phy);
		

        phyWakeup = DynamicCast<UanNetDevice> (devicesWakeup.Get(i))->GetPhy ();
        macWakeup->AttachWakeupPhy (phyWakeup);
        phyWakeup->SetMac (macWakeup);
        phyWakeup->RegisterListener( &(*macWakeup));


        //Afegir mac rts
        uanNetdevice = DynamicCast<UanNetDevice> (devices.Get(i));

        nodePtr=nc.Get(i);
		wakeUpDevicePtr = devicesWakeupHE.Get(i);
		}
      Ptr<UanMacTlohi> macAlohaRts = Create<UanMacTlohi> ();
      macAlohaRts->AttachMacWakeup (macWakeup);
      macAlohaRts->AttachPhy (phy);
      //macAlohaRts->SetBackoffTime (m_backoff);
      //macAlohaRts->SetUseAck (m_ack);
      //macAlohaRts->SetBulkSend (0);
	  macAlohaRts->SetUltra(m_isUltra);
	  
      uanNetdevice->SetMac (macAlohaRts);
      macAlohaRts->SetAddress (uanAddress);
     // macAlohaRts->SetRtsSize (13);
      macWakeup->SetAddress (uanAddress);

      if (i == devices.GetN())
        macAlohaRts->SetForwardUpCb(MakeCallback(&SimpleTest::SinkReceiveData, this));
      else
        macAlohaRts->SetSendDatacb (MakeCallback (&SimpleTest::DataSent, this));

      //macWakeup->SetSendPhyStateChangeCb (MakeCallback (&UanMacFama::PhyStateCb, macAlohaRts));
      macWakeup->SetTxEndCallback (MakeCallback (&UanMacTlohi::TxEnd, macAlohaRts));
	  macWakeup->SetToneRxCallback (MakeCallback (&UanMacTlohi::RxCTD,macAlohaRts));


      /********** Energy **********/
	  //BasicEnergySourceHelper energyHelper;
	  //energyHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (10000.0));
      Ptr<InfiniteEnergySource> infiniteEnergy = CreateObject<InfiniteEnergySource> ();
      infiniteEnergy->SetNode (nodePtr);
      nodePtr->AggregateObject (infiniteEnergy);

      // Energy modem
      Ptr<AcousticModemEnergyModel> modelPhy = CreateObject<AcousticModemEnergyModel> ();
      modelPhy->SetTxPowerW(0.120);
      modelPhy->SetRxPowerW(0.024);
      modelPhy->SetIdlePowerW(0.024);
      modelPhy->SetSleepPowerW(0.000003);

      modelPhy->SetNode (nodePtr);
      modelPhy->SetEnergySource (infiniteEnergy);
      infiniteEnergy->AppendDeviceEnergyModel (modelPhy);

      DeviceEnergyModel::ChangeStateCallback cb;
      cb = MakeCallback (&DeviceEnergyModel::ChangeState, modelPhy);
      phy->SetEnergyModelCallback (cb);

      // Energy wakeup
      Ptr<AcousticModemEnergyModel> modelWakeup = CreateObject<AcousticModemEnergyModel> ();
      modelWakeup->SetTxPowerW(0.120);
      modelWakeup->SetRxPowerW(0.0000081);
      modelWakeup->SetIdlePowerW(0.0000081);
      modelWakeup->SetSleepPowerW(0.0000081);

      modelWakeup->SetNode (nodePtr);
      modelWakeup->SetEnergySource (infiniteEnergy);
      infiniteEnergy->AppendDeviceEnergyModel (modelWakeup);

      cb = MakeCallback (&DeviceEnergyModel::ChangeState, modelWakeup);
      phyWakeup->SetEnergyModelCallback (cb);


      phy->SetSleepMode (true);
      phyWakeup->SetSleepMode (false);
      /****************************/


      /**************************************/
      /********** High Energy Mode **********/

      /*macWakeup->SetHighEnergyMode ();
      Ptr<UanPhy> phyWakeupHE = DynamicCast<UanNetDevice> (wakeUpDevicePtr)->GetPhy ();
      macWakeup->AttachWakeupHEPhy (phyWakeupHE);
      phyWakeupHE->SetMac (macWakeup);*/


      /********** Energy **********/
      // Energy wakeupHE
      /*Ptr<AcousticModemEnergyModel> modelWakeupHE = CreateObject<AcousticModemEnergyModel> ();
      modelWakeupHE->SetTxPowerW(0.120);
      modelWakeupHE->SetRxPowerW(0.00054);
      modelWakeupHE->SetIdlePowerW(0.00054);
      modelWakeupHE->SetSleepPowerW(0);

      modelWakeupHE->SetNode (nodePtr);
      modelWakeupHE->SetEnergySource (infiniteEnergy);
      infiniteEnergy->AppendDeviceEnergyModel (modelWakeupHE);

      cb = MakeCallback (&DeviceEnergyModel::ChangeState, modelWakeupHE);
      phyWakeupHE->SetEnergyModelCallback (cb);*/

      /****************************/

      /*phyWakeupHE->SetSleepMode (true);*/
    }
      /**************************************/
  // uan.EnableAsciiAll (std::cerr);

    
  
  
//  SendRandSrc(devices, uanDst);
//  {
//    Ptr<Packet> pkt = Create<Packet> (m_dataSize);
//    DupTag dupTag(DupTag::GetNewId ());
//    pkt->AddPacketTag(dupTag);
//    Simulator::Schedule(Seconds (10), &NetDevice::Send, devices.Get (1), pkt, uanDst, 0);
//  }
//  {
//    Ptr<Packet> pkt = Create<Packet> (m_dataSize);
//    DupTag dupTag(DupTag::GetNewId ());
//    pkt->AddPacketTag(dupTag);
//    Simulator::Schedule(Seconds (10), &NetDevice::Send, devices.Get (1), pkt, uanDst, 0);
//  }
  /*UanAddress uanDst (0);
  for (uint8_t src = 1; src < devices.GetN(); src++)
    {
      UanAddress uanDst (0);
      double time = m_randExp->GetValue();
      Simulator::Schedule(Seconds (time), &BackoffStudy::Send, this, devices, uanDst, src);
    }*/
	
	
	
}
void
SimpleTest::StartRun(){
  
    NodeContainer nc;
    NodeContainer sink;

	
	SetPhyMac();
    SetupWakeup(nc ,sink);
	/*
	PacketSocketAddress socket;
    socket.SetSingleDevice (sinkDevice.Get (0)->GetIfIndex ());
    socket.SetPhysicalAddress (sinkDevice.Get (0)->GetAddress ());
    socket.SetProtocol (0);

    UanNodeApplicationHelper app ("ns3::PacketSocketFactory", Address (socket));
    app.SetAttribute ("MaxIntervalTime", DoubleValue (m_offeredLoad * 0.1));
    app.SetAttribute ("MinIntervalTime", DoubleValue (m_offeredLoad * 0.1 - 0.1));
    app.SetAttribute ("DataRate", DataRateValue (m_dataRate));
    app.SetAttribute ("PacketSize", UintegerValue (m_packetSize));

    ApplicationContainer apps = app.Install (nc);
    apps.Start (Seconds (0.5));
    
	PacketSinkHelper appSink ("ns3::PacketSocketFactory", Address (socket));
	ApplicationContainer appSinks = appSink.Install(sink);
	appSinks.Start (Seconds (0.5));
	*/


	m_energyNodes.clear();
    m_energyNodes.push_back(0);
	m_energySink.clear();
	m_energySink.push_back(0);
	
	
    /*PacketSocketAddress socket;
    socket.SetSingleDevice (sinkDevice.Get (0)->GetIfIndex ());
    socket.SetPhysicalAddress (sinkDevice.Get (0)->GetAddress ());
    socket.SetProtocol (0);


    UanNodeApplicationHelper app ("ns3::PacketSocketFactory", Address (socket));
    app.SetAttribute ("MaxIntervalTime", DoubleValue (m_offeredLoad * 0.1));
    app.SetAttribute ("MinIntervalTime", DoubleValue (m_offeredLoad * 0.1 - 0.1));
    app.SetAttribute ("DataRate", DataRateValue (m_dataRate*m_offeredLoad));
    app.SetAttribute ("PacketSize", UintegerValue (m_packetSize));

    ApplicationContainer apps = app.Install (nc);
    apps.Start (Seconds (0.5));
	*/
	
	Time nextEvent = Seconds (0.5);
    for (uint32_t an = 0; an < m_avgs; an++)
        {
        nextEvent += m_simTime;
        Simulator::Schedule (nextEvent, &SimpleTest::ReportPower, this, nc ,sink);
		Simulator::Schedule (nextEvent, &SimpleTest::ResetData, this);
        Simulator::Schedule (nextEvent, &SimpleTest::UpdatePositions, this, nc);
		//app.SetAttribute ("DataRate", DataRateValue (m_dataRate));
		//apps = app.Install (nc);
		
		}
    Simulator::Schedule (nextEvent, &SimpleTest::ReportData, this);
	Simulator::Stop (nextEvent+Seconds(0.5)+ m_simTime);
    //apps.Stop (nextEvent + m_simTime);
	//appSinks.Stop (nextEvent + m_simTime);
    for (uint8_t src = 0; src < devices.GetN(); src++)
    {
      UanAddress uanDst (m_numNodes);
      double time = m_randExp->GetValue();
      Simulator::Schedule(Seconds (time), &SimpleTest::Send, this, devices, uanDst, src);
    }
	/*Ptr<Node> sinkNode = sink.Get (0);
    TypeId psfid = TypeId::LookupByName ("ns3::PacketSocketFactory");
    if (sinkNode->GetObject<SocketFactory> (psfid) == 0)
      {
        Ptr<PacketSocketFactory> psf = CreateObject<PacketSocketFactory> ();
        sinkNode->AggregateObject (psf);
      }
    Ptr<Socket> sinkSocket = Socket::CreateSocket (sinkNode, psfid);
    sinkSocket->Bind (socket);*/
    //sinkSocket->SetRecvCallback (MakeCallback (&SimpleTest::ReceivePacket, this));


    std::ofstream ascii (m_asciitracefile.c_str ());
    if (!ascii.is_open ())
      {
        NS_FATAL_ERROR ("Could not open ascii trace file: "
                        << m_asciitracefile);
      }
    m_uan.EnableAsciiAll (ascii);

    Simulator::Run ();

    
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
    for (uint32_t i=0; i < sinkDevice.GetN (); i++)
      {
        sinkDevice.Get (i) = 0;
      }

	for (uint32_t i=0; i < devicesWakeup.GetN (); i++)
      {
        devicesWakeup.Get (i) = 0;
      }
	for (uint32_t i=0; i < sinkDeviceWakeup.GetN (); i++)
      {
        sinkDeviceWakeup.Get (i) = 0;
      }
	  
	 for (uint32_t i=0; i < devicesWakeupHE.GetN (); i++)
      {
        devicesWakeupHE.Get (i) = 0;
      }
	 for (uint32_t i=0; i < sinkDeviceWakeupHE.GetN (); i++)
      {
        sinkDeviceWakeupHE.Get (i) = 0;
      }
	  
    Simulator::Destroy ();
    //return m_plotData;

}

void
SimpleTest::Send (NetDeviceContainer &devices, UanAddress &uanAddress, uint8_t src)
{
  Ptr<Packet> pkt = Create<Packet> (m_packetSize);

  //DupTag dupTag(DupTag::GetNewId ());
  //pkt->AddPacketTag(dupTag);

  //m_pktSent[dupTag.m_id] = Simulator::Now ().GetSeconds ();

  devices.Get (src)->Send (pkt, uanAddress, 0);
  //m_generatedPackets++;

  UanAddress uanDst (m_numNodes);
  double time = m_randExp->GetValue();
  //double sendTime = Simulator::Now().GetSeconds () + time;
  //if (sendTime < m_simTime && m_generatedPackets < 50000)
   // {
      Simulator::Schedule(Seconds (time), &SimpleTest::Send, this, devices, uanDst, src);
   // }
}


void
SimpleTest::ResetData ()
{
  NS_LOG_DEBUG (Simulator::Now ().GetSeconds () << " Packet Sent "<<(m_sentPackets)<<" ,Packet received "<<m_receivedPackets);
  m_PacketsCount.push_back (m_sentPackets);
  m_throughputs.push_back (m_receivedPackets*m_packetSize*8.0 / m_simTime.GetSeconds ());
  m_receivedPackets = 0;
  m_sentPackets=0;
}

void
SimpleTest::ReportPower (NodeContainer& nc, NodeContainer& sink)
{
    double accumEnergy = 0;
	for (uint32_t i = 0; i < nc.GetN (); i++)
    {
      Ptr<InfiniteEnergySource> source = nc.Get (i)->GetObject<InfiniteEnergySource> ();
      accumEnergy += source->GetEnergyConsumption ();
    }
  m_energyNodes.push_back (accumEnergy / nc.GetN ());
  m_energySink.push_back (sink.Get (0)->GetObject<InfiniteEnergySource> ()->GetEnergyConsumption ());

  NS_LOG_DEBUG (Simulator::Now ().GetSeconds () <<"  "<<m_energyNodes.back() <<" nodes "<<m_energySink.back()<< " sink Power Consumed " );

	
}
void
SimpleTest::ReportData()
{
	 NS_ASSERT (m_throughputs.size () == m_avgs);
	double avgThroughput = 0.0;
	double avgPower = m_energyNodes.back()-m_energyNodes[0];
	for (uint32_t i=0; i<m_avgs; i++)
    {
		avgThroughput += m_throughputs[i];
    }
	avgThroughput /= m_avgs;
	//avgThroughput /= m_offeredLoad*m_dataRate;
	avgPower /= m_avgs;
  
	m_plotData.Add (m_offeredLoad, avgThroughput, avgPower);
	m_throughputs.clear ();

	//Config::Set ("/NodeList/*/DeviceList/*/Mac/CW", UintegerValue (cw + m_cwStep));

	SeedManager::SetRun (SeedManager::GetRun () + 1);

	NS_LOG_DEBUG ("Average for offered load=" << m_offeredLoad << " over " << m_avgs << " runs: " << avgThroughput);
	NS_LOG_DEBUG ("Average Power for offered load=" << m_offeredLoad << " over " << m_avgs << " runs: " << avgPower);
  
  
	avgPower = m_energyNodes.back();
	m_energyNodes.clear ();
	m_energyNodes.push_back(avgPower);
}
void
SimpleTest::UpdatePositions (NodeContainer &nodes)
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
SimpleTest::SinkReceiveData (Ptr<Packet> pkt, const UanAddress &src)
{
  
      m_receivedPackets++;
      
}

void
SimpleTest::DataSent (Ptr<Packet> pkt)
{
  
      m_sentPackets++;
   
}
int
main (int argc, char **argv)
{

  LogComponentEnable ("WakeupTestSimple", LOG_LEVEL_ALL);
  //LogComponentEnable ("UanMacTlohi", LOG_LEVEL_ALL);
  //LogComponentEnable ("UanMacWakeupTlohi", LOG_LEVEL_ALL);

  SimpleTest exp;

  std::string gnudatfile ("cwexpgnuout.dat");

  CommandLine cmd;
  cmd.AddValue ("NumNodes", "Number of transmitting nodes", exp.m_numNodes);
  cmd.AddValue ("Depth", "Depth of transmitting and sink nodes", exp.m_depth);
  cmd.AddValue ("RegionSize", "Size of boundary in meters", exp.m_boundary);
  cmd.AddValue ("PacketSize", "Generated packet size in bytes", exp.m_packetSize);
  cmd.AddValue ("DataRate", "DataRate in bps", exp.m_dataRate);
  cmd.AddValue ("Averages", "Experiement Repeat Time", exp.m_avgs);
  cmd.AddValue ("GnuFile", "Name for GNU Plot output", exp.m_gnudatfile);
  //cmd.AddValue ("PerModel", "PER model name", perModel);
  //cmd.AddValue ("SinrModel", "SINR model name", sinrModel);
  cmd.Parse (argc, argv);


  for(exp.m_offeredLoad=exp.m_maxOfferedLoad; exp.m_offeredLoad>=1;exp.m_offeredLoad-=4)
  {
    exp.StartRun();
  }
	
	
  Gnuplot gp;
  Gnuplot3dDataset ds;
  gp.AddDataset (exp.m_plotData)
;

  std::ofstream of (exp.m_gnudatfile.c_str ());
  if (!of.is_open ())
    {
      NS_FATAL_ERROR ("Can not open GNU Plot outfile: " << exp.m_gnudatfile);
    }
  gp.GenerateOutput (of);

  //per = 0;
  //sinr = 0;
  
  of.close();

}

