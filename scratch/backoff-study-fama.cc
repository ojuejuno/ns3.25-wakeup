/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Network Security Lab, University of Washington, Seattle.
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
 * Author: He Wu <mdzz@u.washington.edu>
 */

#include "ns3/uan-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "uan-cw-example.h"
#include "ns3/core-module.h"
#include "ns3/stats-module.h"
#include "ns3/applications-module.h"

#include "ns3/callback.h"
#include "ns3/log.h"
#include "ns3/test.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/config.h"
#include "ns3/string.h"

#include "ns3/command-line.h"
#include "ns3/infinite-energy-source.h"


#include <math.h>
#include <iomanip>
#include <algorithm>
#include <fstream>

//#include <boost/math/distributions/students_t.hpp>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CapacitorEnergyUpdateTest");


std::vector<uint32_t> generatedPackets;
std::vector<uint32_t> sentPackets;
std::vector<uint32_t> receivedPackets;
std::vector<uint32_t> dupPackets;
std::vector<double> delay;
std::vector<double> energyNode;
std::vector<double> energySink;

namespace ns3 {
class DupTag : public Tag
{
public:
  static uint64_t m_idCounter;
  uint64_t m_id;

  static uint64_t GetNewId ()
  {
    return m_idCounter++;
  }

  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::DupTag")
      .SetParent<Tag> ()
      .AddConstructor<DupTag> ()
    ;
    return tid;
  }

  DupTag ()
  {
  }

  DupTag (uint64_t id)
  {
    m_id = id;
  }

  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const
  {
    return sizeof(m_id);
  }
  virtual void Serialize (TagBuffer start) const
  {
    start.WriteU64 (m_id);
  }
  virtual void Deserialize (TagBuffer start)
  {
    m_id = start.ReadU64();
  }
  virtual void Print (std::ostream &os) const
  {
    os << m_id;
  }
  virtual TypeId GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }
};
NS_OBJECT_ENSURE_REGISTERED (DupTag);
uint64_t DupTag::m_idCounter = 0;
}

void dummyCallback (Ptr<Packet> arg0, double arg1, UanTxMode arg2)
{
  return;
}


/**
 * Test case of update remaining energy for BasicEnergySource and
 * WifiRadioEnergyModel.
 */
class BackoffStudy
{
public:
  BackoffStudy ();
  virtual ~BackoffStudy ();

private:
  void EnergyDepletion ();
  void PrepareSimulation ();
  void Free ();
  void SinkReceiveData (Ptr<Packet> pkt, const UanAddress &src);
  void DataSent (Ptr<Packet> pkt);
  void Send (NetDeviceContainer &devices, UanAddress &uanAddress, uint8_t src);
  void SendExp (NetDeviceContainer &devices, UanAddress &uanAddress, uint8_t src);
  void SendRandSrc (NetDeviceContainer &devices, UanAddress &uanAddress);


  UanHelper uan;
  NodeContainer nc;
  NetDeviceContainer devices;
  NetDeviceContainer devicesWakeup;
  NetDeviceContainer devicesWakeupHE;

public:
  void DoRun (void);

  std::string m_bhCfgFile;
  uint32_t m_numNodes;
  uint32_t m_stop;

  double m_boundary;
  double m_depth;

  double m_runNumber;
  double m_seedNumber;

  double m_backoff;
  double m_trafficGenerationInterval;
  bool m_ack;
  unsigned int m_txSpeed;

  uint32_t m_dataSize;

  Ptr<UniformRandomVariable> m_rand;
  Ptr<ExponentialRandomVariable> m_randExp;

  uint32_t m_generatedPackets;
  uint32_t m_receivedPackets;
  uint32_t m_dupPackets;
  uint32_t m_sentPackets;
  double m_delay;
  std::map<uint64_t, double> m_pktSent;
  std::set<uint64_t> m_dupReceivedPackets;
  std::set<uint64_t> m_dupTags;
};

BackoffStudy::BackoffStudy ()
{
  //m_bhCfgFile = "/home/jocliba/Documents/uan/bellhop/burriana_directional/burriana.dat";
  m_numNodes = 100;
  m_stop = 1800;

  m_boundary = 100.0;
  m_depth = 25.0;

  m_backoff = 0.5;
  m_trafficGenerationInterval = 0.3;
  m_ack = false;
  m_txSpeed = 1000;

  m_runNumber = 0;
  m_seedNumber = 1;

  m_dataSize = 61; /* bytes */
  m_generatedPackets = 0;
  m_receivedPackets = 0;
  m_dupPackets = 0;
  m_sentPackets = 0;
  m_delay = 0;
  
  m_rand = CreateObject <UniformRandomVariable>() ;
  m_randExp = CreateObject<ExponentialRandomVariable> ();
  m_randExp->SetAttribute ("Mean", DoubleValue (0.3));
  m_randExp->SetAttribute ("Bound", DoubleValue (0.9));
  }

BackoffStudy::~BackoffStudy ()
{
  //delete m_randExp;
}

void
BackoffStudy::SinkReceiveData (Ptr<Packet> pkt, const UanAddress &src)
{
  DupTag tag;
  NS_ASSERT (pkt->PeekPacketTag (tag));
  if (m_dupReceivedPackets.find (tag.m_id) == m_dupReceivedPackets.end ())
    {
      m_receivedPackets++;
      m_dupReceivedPackets.insert (tag.m_id);

      double delay = Simulator::Now ().GetSeconds() - m_pktSent[tag.m_id];
      m_delay += delay;
    }
}

void
BackoffStudy::DataSent (Ptr<Packet> pkt)
{
  DupTag dupTag;
  NS_ASSERT (pkt->PeekPacketTag (dupTag));
  NS_ASSERT (dupTag.m_id < DupTag::m_idCounter);

  if (m_dupTags.find (dupTag.m_id) == m_dupTags.end ())
    {
      m_dupTags.insert (dupTag.m_id);
      m_sentPackets++;
    }
  else
    {
      m_dupPackets++;
    }
}

//void
//BackoffStudy::Send (NetDeviceContainer &devices, UanAddress &uanAddress, uint8_t src)
//{
//  NS_ASSERT(false); // Revisar
//  Ptr<Packet> pkt = Create<Packet> (m_dataSize);
//
//  DupTag dupTag(DupTag::GetNewId ());
//  pkt->AddPacketTag(dupTag);
//
//  devices.Get (src)->Send (pkt, uanAddress, 0);
//  m_generatedPackets++;
//
//  double sendTime = Simulator::Now().GetSeconds () + m_trafficGenerationInterval;
//  if (sendTime < m_stop)
//    {
//      Simulator::Schedule(Seconds (m_trafficGenerationInterval), &BackoffStudy::Send, this, devices, uanAddress, src);
//    }
//}

void
BackoffStudy::Send (NetDeviceContainer &devices, UanAddress &uanAddress, uint8_t src)
{
  Ptr<Packet> pkt = Create<Packet> (m_dataSize);

  DupTag dupTag(DupTag::GetNewId ());
  pkt->AddPacketTag(dupTag);

  m_pktSent[dupTag.m_id] = Simulator::Now ().GetSeconds ();

  devices.Get (src)->Send (pkt, uanAddress, 0);
  m_generatedPackets++;

  UanAddress uanDst (0);
  double time = m_randExp->GetValue();
  double sendTime = Simulator::Now().GetSeconds () + time;
  if (sendTime < m_stop && m_generatedPackets < 50000)
    {
      Simulator::Schedule(Seconds (time), &BackoffStudy::Send, this, devices, uanDst, src);
    }
}

void
BackoffStudy::SendRandSrc (NetDeviceContainer &devices, UanAddress &uanAddress)
{
  Ptr<Packet> pkt = Create<Packet> (m_dataSize);

  DupTag dupTag(DupTag::GetNewId ());
  pkt->AddPacketTag(dupTag);

  m_pktSent[dupTag.m_id] = Simulator::Now ().GetSeconds ();

  uint8_t src = m_rand->GetInteger(1, m_numNodes-1);

  devices.Get (src)->Send (pkt, uanAddress, 0);
  m_generatedPackets++;

  UanAddress uanDst (0);
  double time = m_randExp->GetValue();
  double sendTime = Simulator::Now().GetSeconds () + time;
  if (sendTime < m_stop)
    {
      Simulator::Schedule(Seconds (time), &BackoffStudy::SendRandSrc, this, devices, uanDst);
    }
}

void
BackoffStudy::SendExp (NetDeviceContainer &devices, UanAddress &uanAddress, uint8_t src)
{
  NS_ASSERT(false); // REVISAR
  Ptr<Packet> pkt = Create<Packet> (m_dataSize);

  DupTag dupTag(DupTag::GetNewId ());
  pkt->AddPacketTag(dupTag);

  devices.Get(src)->Send(pkt, uanAddress, 0);
  m_generatedPackets++;

  UanAddress uanDst (0);
  double value = m_randExp->GetValue ();
  double sendTime = Simulator::Now().GetSeconds () + value;
  if (sendTime < m_stop)
    {
      Simulator::Schedule(Seconds (value), &BackoffStudy::SendExp, this, devices, uanDst, src);
    }
}

void
BackoffStudy::DoRun (void)
{
  //m_randExp = new ExponentialVariable (m_trafficGenerationInterval, 3*m_trafficGenerationInterval);

  PrepareSimulation ();

  Simulator::Stop (Seconds (m_stop));
  Simulator::Run ();

//  std::cout << "Data generated:\t" << m_generatedPackets << std::endl;
//  std::cout << "Data sent:\t" << m_sentPackets << std::endl;
//  std::cout << "Data received:\t" << m_receivedPackets << std::endl;
//  std::cout << "Delay:\t" << m_delay / m_receivedPackets << std::endl;

  generatedPackets.push_back (m_generatedPackets);
  sentPackets.push_back (m_sentPackets);
  receivedPackets.push_back (m_receivedPackets);
  dupPackets.push_back (m_dupPackets);
  delay.push_back (m_delay / m_receivedPackets);

  double accumEnergy = 0;
  for (uint32_t i = 1; i < nc.GetN (); i++)
    {
      Ptr<InfiniteEnergySource> source = nc.Get (i)->GetObject<InfiniteEnergySource> ();
      accumEnergy += source->GetEnergyConsumption ();
    }
  energyNode.push_back (accumEnergy / (nc.GetN () - 1));
  energySink.push_back (nc.Get (0)->GetObject<InfiniteEnergySource> ()->GetEnergyConsumption ());

  Free ();
  Simulator::Destroy();
}

void
BackoffStudy::Free ()
{
  for (uint32_t i=0; i < nc.GetN (); i++)
    {
      DynamicCast<UanNetDevice> (devices.Get(i))->GetPhy () = 0;
      DynamicCast<UanNetDevice> (devices.Get(i))->GetMac () = 0;
      DynamicCast<UanNetDevice> (devicesWakeup.Get(i))->GetPhy () = 0;
      DynamicCast<UanNetDevice> (devicesWakeup.Get(i))->GetMac () = 0;
      DynamicCast<UanNetDevice> (devicesWakeupHE.Get(i))->GetPhy () = 0;
      DynamicCast<UanNetDevice> (devicesWakeupHE.Get(i))->GetMac () = 0;
      devices.Get(i) = 0;
      devicesWakeup.Get(i) = 0;
      devicesWakeupHE.Get(i) = 0;
      nc.Get (i) = 0;
    }
  m_pktSent.clear ();
}

void
BackoffStudy::PrepareSimulation ()
{
  /*
   * Tx model
   */
  UanTxMode mode;
  mode = UanTxModeFactory::CreateMode (UanTxMode::FSK, m_txSpeed,
                                       m_txSpeed, 85000,
                                       1000, 2,
                                       "Default mode");
  UanModesList myModes;
  myModes.AppendMode (mode);

  ObjectFactory obf;
  obf.SetTypeId ("ns3::UanPhyPerGenDefault");
  Ptr<UanPhyPer> per = obf.Create<UanPhyPer> ();
  obf.SetTypeId ("ns3::UanPhyCalcSinrDefault");
  Ptr<UanPhyCalcSinr> sinr = obf.Create<UanPhyCalcSinr> ();

  uan.SetPhy ("ns3::UanPhyGen",
              "PerModel", PointerValue (per),
              "SinrModel", PointerValue (sinr),
              "SupportedModes", UanModesListValue (myModes));

  uan.SetMac ("ns3::UanMacWakeup");
  nc.Create (m_numNodes);

  Ptr<UanPropModelIdeal> prop = CreateObjectWithAttributes<UanPropModelIdeal> ();
  Ptr<UanNoiseModel> noise = CreateObjectWithAttributes<UanNoiseModelDefault>();
  noise->SetAttribute ("Wind", DoubleValue(0));
  Ptr<UanChannel> channel = CreateObjectWithAttributes<UanChannel> ("PropagationModel", PointerValue (prop),
                                                                    "NoiseModel", PointerValue(noise));

  //Create net device and nodes with UanHelper
  devices = uan.Install (nc, channel);

  //Create wakeup net device
  devicesWakeup = uan.Install (nc, channel);

  //Create wakeupHE net device
  devicesWakeupHE = uan.Install (nc, channel);

  // Set location
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();

  pos->Add (Vector (m_boundary/2.0, m_boundary/2.0, 1));
  for (uint32_t i = 0; i < m_numNodes-1; i++)
    {
      double x = m_rand->GetValue(0, m_boundary);
      double y = m_rand->GetValue(0, m_boundary);

      pos->Add (Vector (x, y, m_depth));
    }
  mobility.SetPositionAllocator (pos);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nc);

//  std::cerr << "Position of sink: "
//          << nc.Get (0)->GetObject<MobilityModel> ()->GetPosition () << std::endl;


  for (uint32_t i = 0; i < devices.GetN(); i++)
  {
      Ptr<UanPhyGen> phy = DynamicCast<UanPhyGen> (DynamicCast<UanNetDevice> (devices.Get(i))->GetPhy ());
      Ptr<UanMac> mac = DynamicCast<UanNetDevice> (devices.Get(i))->GetMac ();
      Ptr<UanMacWakeup> macWakeup = DynamicCast<UanMacWakeup> (mac);
      phy->RegisterListener( &(*macWakeup));


      Ptr<UanPhy> phyWakeup = DynamicCast<UanNetDevice> (devicesWakeup.Get(i))->GetPhy ();
      macWakeup->AttachWakeupPhy (phyWakeup);
      phyWakeup->SetMac (macWakeup);
      phyWakeup->RegisterListener( &(*macWakeup));


      //Add mac rts
      Ptr<UanNetDevice> uanNetdevice = DynamicCast<UanNetDevice> (devices.Get(i));
      UanAddress uanAddress (i);

      Ptr<UanMacFama> macAlohaRts = Create<UanMacFama> ();
      macAlohaRts->AttachMacWakeup (macWakeup);
      macAlohaRts->AttachPhy (phy);
      macAlohaRts->SetBackoffTime (m_backoff);
      macAlohaRts->SetUseAck (m_ack);
      macAlohaRts->SetBulkSend (0);

      uanNetdevice->SetMac (macAlohaRts);
      macAlohaRts->SetAddress (uanAddress);
      macAlohaRts->SetRtsSize (13);
      macWakeup->SetAddress (uanAddress);

      if (i == 0)
        macAlohaRts->SetForwardUpCb(MakeCallback(&BackoffStudy::SinkReceiveData, this));
      else
        macAlohaRts->SetSendDataCallback (MakeCallback (&BackoffStudy::DataSent, this));

      macWakeup->SetSendPhyStateChangeCb (MakeCallback (&UanMacFama::PhyStateCb, macAlohaRts));
      macWakeup->SetTxEndCallback (MakeCallback (&UanMacFama::TxEnd, macAlohaRts));


      /********** Energy **********/
      Ptr<InfiniteEnergySource> infiniteEnergy = CreateObject<InfiniteEnergySource> ();
      infiniteEnergy->SetNode (nc.Get (i));
      nc.Get (i)->AggregateObject (infiniteEnergy);

      // Energy modem
      Ptr<AcousticModemEnergyModel> modelPhy = CreateObject<AcousticModemEnergyModel> ();
      modelPhy->SetTxPowerW(0.120);
      modelPhy->SetRxPowerW(0.024);
      modelPhy->SetIdlePowerW(0.024);
      modelPhy->SetSleepPowerW(0.000003);

      modelPhy->SetNode (nc.Get (i));
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
      modelWakeup->SetSleepPowerW(0);

      modelWakeup->SetNode (nc.Get (i));
      modelWakeup->SetEnergySource (infiniteEnergy);
      infiniteEnergy->AppendDeviceEnergyModel (modelWakeup);

      cb = MakeCallback (&DeviceEnergyModel::ChangeState, modelWakeup);
      phyWakeup->SetEnergyModelCallback (cb);


      phy->SetSleepMode (true);
      phyWakeup->SetSleepMode (false);
      /****************************/


      /**************************************/
      /********** High Energy Mode **********/

      macWakeup->SetHighEnergyMode ();
      Ptr<UanPhy> phyWakeupHE = DynamicCast<UanNetDevice> (devicesWakeupHE.Get(i))->GetPhy ();
      macWakeup->AttachWakeupHEPhy (phyWakeupHE);
      phyWakeupHE->SetMac (macWakeup);


      /********** Energy **********/
      // Energy wakeupHE
      Ptr<AcousticModemEnergyModel> modelWakeupHE = CreateObject<AcousticModemEnergyModel> ();
      modelWakeupHE->SetTxPowerW(0.120);
      modelWakeupHE->SetRxPowerW(0.00054);
      modelWakeupHE->SetIdlePowerW(0.00054);
      modelWakeupHE->SetSleepPowerW(0);

      modelWakeupHE->SetNode (nc.Get (i));
      modelWakeupHE->SetEnergySource (infiniteEnergy);
      infiniteEnergy->AppendDeviceEnergyModel (modelWakeupHE);

      cb = MakeCallback (&DeviceEnergyModel::ChangeState, modelWakeupHE);
      phyWakeupHE->SetEnergyModelCallback (cb);

      /****************************/

      phyWakeupHE->SetSleepMode (true);
      /**************************************/
  }


// uan.EnableAsciiAll (std::cerr);


  UanAddress uanDst (0);
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

  for (uint8_t src = 1; src < devices.GetN(); src++)
    {
      UanAddress uanDst (0);
      double time = m_randExp->GetValue();
      Simulator::Schedule(Seconds (time), &BackoffStudy::Send, this, devices, uanDst, src);
    }

}

int main (int argc, char** argv)
{
  double backoff = .5;
  double trafficGenerationInterval = .6;
  bool ack = false;
  unsigned int numNodes = 100;
  unsigned int txSpeed = 1000;
  //string bhCfgFile;
  CommandLine cmd;
//
//  cmd.AddValue ("runNumber", "run Number", backoffStudy->m_runNumber);
//  cmd.AddValue ("seedNumber", "Seed number", backoffStudy->m_seedNumber);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
  cmd.AddValue ("backoff", "Maximum contention window", backoff);
  cmd.AddValue ("trafficGen", "Traffic generation interval", trafficGenerationInterval);
  cmd.AddValue ("ack", "1: Use ACK, 0: do not use ACK", ack);
  cmd.AddValue ("txSpeed", "Transmission Speed", txSpeed);
  //cmd.AddValue ("bhCfgFile", "Belhop config file", bhCfgFile);
//  cmd.AddValue ("stop", "Stop simulation time", backoffStudy->m_stop);
  cmd.Parse (argc, argv);

//  SeedManager::SetSeed (backoffStudy->m_seedNumber);
//  SeedManager::SetRun (backoffStudy->m_runNumber);


  //double sampleSize;


  double intervalReceived = 1;


  double intervalSent = 1;


  double intervalDup = 1;

  double intervalEnergy = 1;


  double intervalEnergySink = 1;


  double intervalDelay = 1;


  uint32_t runNumber = 0;
  while (intervalReceived > 0.05 || intervalSent > 0.05 || intervalDup > 0.05 || intervalEnergy > 0.05 || intervalEnergySink > 0.05 || intervalDelay > 0.05 )
    {
//      if (runNumber > 50)
//        {
//          runNumber--;
//          break;
//        }

      SeedManager::SetSeed (1330703057);
      SeedManager::SetRun (runNumber);

      DupTag::m_idCounter = 0;
      BackoffStudy* backoffStudy = new BackoffStudy ();
      //Parametres
      backoffStudy->m_backoff = backoff;
      backoffStudy->m_ack = ack;
      backoffStudy->m_numNodes = numNodes;
      backoffStudy->m_trafficGenerationInterval = trafficGenerationInterval;
      backoffStudy->m_txSpeed = txSpeed;
      //backoffStudy->m_bhCfgFile = bhCfgFile;
      backoffStudy->DoRun();

      //sampleSize = receivedPackets.size ();



}
}
