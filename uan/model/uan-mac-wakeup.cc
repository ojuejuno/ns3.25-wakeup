/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Universitat Politècnica de València
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
 * Author: Salvador Climent <jocliba@upvnet.upv.es>
 */

#include "uan-mac-wakeup.h"
#include "uan-tx-mode.h"
#include "uan-address.h"
#include "ns3/log.h"
#include "uan-phy.h"
#include "uan-header-common.h"
#include "uan-header-wakeup.h"
//#include "uan-mac-rts.h"
#include "uan-phy-header.h"


#include <iostream>



namespace ns3
{

NS_LOG_COMPONENT_DEFINE ("UanMacWakeup");
NS_OBJECT_ENSURE_REGISTERED (UanMacWakeup);

UanMacWakeup::UanMacWakeup ()
  : UanMac (),
    m_cleared (false),
    m_timeDelayTx (1),
    m_dataSent (false)
{
  m_timerEndTx.SetFunction (&UanMacWakeup::On_timerEndTx, this);
  m_timerDelayTx.SetFunction (&UanMacWakeup::On_timerDelayTx, this);
  m_timerDelayTxWUHE.SetFunction (&UanMacWakeup::On_timerDelayTxWUHE, this);
  m_timerTxFail.SetFunction (&UanMacWakeup::On_timerTxFail, this);

  m_pkt = 0;
  m_highEnergyMode = false;
  m_wuAlone = false;
  m_toneMode = false;
  m_txEnd.Nullify ();
}

UanMacWakeup::~UanMacWakeup ()
{
}

void
UanMacWakeup::SetToneMode ()
{
  m_toneMode = true;
}

void
UanMacWakeup::SetHighEnergyMode ()
{
  m_highEnergyMode = true;
}

void
UanMacWakeup::Clear ()
{
  if (m_cleared)
    {
      return;
    }
  m_cleared = true;
  if (m_phy)
    {
      m_phy->Clear ();
      m_phy = 0;
    }
}

void
UanMacWakeup::DoDispose ()
{
  Clear ();
  m_timerEndTx.Cancel ();
  m_timerDelayTx.Cancel ();
  m_timerDelayTxWUHE.Cancel ();
  m_timerTxFail.Cancel ();
  UanMac::DoDispose ();
}

TypeId
UanMacWakeup::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanMacWakeup")
    .SetParent<UanMac> ()
    .SetGroupName ("Uan")
    .AddConstructor<UanMacWakeup> ()
  ;
  return tid;
}

Address
UanMacWakeup::GetAddress (void)
{
  return m_address;
}

void
UanMacWakeup::SetAddress (UanAddress addr)
{
  m_address=addr;
}

bool
UanMacWakeup::Enqueue (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{
  if (m_pkt != 0)
    {
      NS_LOG_DEBUG ("UanMacWakeup not IDLE, discarding");
      return false;
    }

//  std::cerr << m_phy->IsStateIdle () << " " << m_wakeupPhy->IsStateIdle () << std::endl;
  if (!m_phy->IsStateIdle () && !m_wakeupPhy->IsStateIdle ())
    {
      NS_LOG_DEBUG ("Phy not IDLE, discarding");
      return false;
    }

  m_dest = UanAddress::ConvertFrom (dest);
  m_pkt = packet;
  UanWakeupTag uwt;
  if (!m_pkt->PeekPacketTag(uwt))
    {
      uwt.m_type = UanWakeupTag::DATA;
      m_pkt->AddPacketTag (uwt);
    }
  else
    {
      NS_ASSERT (uwt.m_type == UanWakeupTag::DATA);
    }

  UanPhyHeader phyHeader;
  m_pkt->AddHeader (phyHeader);
  UanPhyTrailer phyTrailer;
  m_pkt->AddTrailer (phyTrailer);


  NS_LOG_DEBUG ((uint32_t) UanAddress::ConvertFrom (GetAddress ()).GetAsInt ()
      << " sending wakeup signal to " << (uint32_t) m_dest.GetAsInt ());

  //Send wakeup signal
  if (m_highEnergyMode) SendBroadcastWU ();
  else if (m_toneMode) SendBroadcastWU();
  else SendWU (m_dest);

  return true;
}

uint32_t
UanMacWakeup::GetHeadersSize () const
{
  uint32_t size = 0;

  UanPhyHeader phyHeader;
  size += phyHeader.GetSerializedSize ();
  UanPhyTrailer phyTrailer;
  size += phyTrailer.GetSerializedSize ();
  UanHeaderWakeup wakeup;
  size += wakeup.GetSerializedSize ();
  UanPhyWUHeader phyWakeupHeader;
  size += phyWakeupHeader.GetSerializedSize ();

  if (m_highEnergyMode)
    {
      size += phyWakeupHeader.GetSerializedSize ();
    }

  return size;
}


bool
UanMacWakeup::Send ()
{
  NS_LOG_DEBUG ((uint32_t) UanAddress::ConvertFrom (GetAddress ()).GetAsInt ()
      << " sending packet to " << (uint32_t) m_dest.GetAsInt ());

  m_state = DATA;
  m_wakeupPhy->SendPacket (m_pkt, 0);
  //m_phy->SendPacket (m_pkt, 0);

  return true;
}

void
UanMacWakeup::SendWU (UanAddress dst)
{
  Ptr<Packet> pkt = Create<Packet> ();

  UanHeaderWakeup wakeup;
  wakeup.SetDest (dst);
  pkt->AddHeader (wakeup);
  UanPhyWUHeader phyHeader;
  pkt->AddHeader (phyHeader);

  UanWakeupTag uwt;
  uwt.m_type = UanWakeupTag::WU;
  pkt->AddPacketTag (uwt);

  m_state = WU;
  m_wakeupPhy->SendPacket (pkt, 0);
  //m_phy->SendPacket (pkt, 0);
}

bool
UanMacWakeup::SendWUAlone (UanAddress dst)
{
  if (m_pkt != 0)
    {
      NS_LOG_DEBUG ("UanMacWakeup not IDLE, discarding");
      return false;
    }

  if (!m_phy->IsStateIdle () && !m_wakeupPhy->IsStateIdle ())
    {
      NS_LOG_DEBUG ("Phy not IDLE, discarding");
      return false;
    }

  m_wuAlone = true;
  SendWU(dst);

  return true;
}

void
UanMacWakeup::SendBroadcastWU ()
{
  SendWU (UanAddress::ConvertFrom (GetBroadcast()).GetAsInt());
}

void
UanMacWakeup::SendWUHE ()
{
  Ptr<Packet> pkt = Create<Packet> ();

  UanHeaderWakeup wakeup;
  wakeup.SetDest (m_dest);
  pkt->AddHeader (wakeup);

  UanWakeupTag uwt;
  uwt.m_type = UanWakeupTag::WUHE;
  pkt->AddPacketTag (uwt);

  m_state = WUHE;
  m_wakeupPhy->SendPacket (pkt, 0);
  //m_phy->SendPacket (pkt, 0);
}

void
UanMacWakeup::SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress& > cb)
{
  m_forUpCb = cb;
}
void
UanMacWakeup::SetTxEndCallback (TxEndCallback cb)
{
  m_txEnd = cb;
}
void
UanMacWakeup::SetToneRxCallback (ToneRxCallback cb)
{
  m_toneRxCallback = cb;
}
void UanMacWakeup::SetSendPhyStateChangeCb (Callback<void, PhyState> cb)
{
  m_stateChangeCb = cb;
}

void
UanMacWakeup::AttachPhy (Ptr<UanPhy> phy)
{
  m_phy = DynamicCast<UanPhyGen> (phy);
  m_phy->SetReceiveOkCallback (MakeCallback (&UanMacWakeup::RxPacketGood, this));
  m_phy->SetReceiveErrorCallback (MakeCallback (&UanMacWakeup::RxPacketError, this));
}
void
UanMacWakeup::AttachWakeupPhy (Ptr<UanPhy> phy)
{
  m_wakeupPhy = phy;
  m_wakeupPhy->SetReceiveOkCallback (MakeCallback (&UanMacWakeup::RxPacketGoodW, this));
  m_wakeupPhy->SetReceiveErrorCallback (MakeCallback (&UanMacWakeup::RxPacketErrorW, this));
}
void
UanMacWakeup::AttachWakeupHEPhy (Ptr<UanPhy> phy)
{
  m_wakeupPhyHE = phy;
  m_wakeupPhyHE->SetReceiveOkCallback (MakeCallback (&UanMacWakeup::RxPacketGoodWHE, this));
  m_wakeupPhyHE->SetReceiveErrorCallback (MakeCallback (&UanMacWakeup::RxPacketErrorWHE, this));
}
void
UanMacWakeup::RxPacketGood (Ptr<Packet> pkt, double sinr, UanTxMode txMode)
{
  UanWakeupTag uwt;
  NS_ASSERT (pkt->RemovePacketTag(uwt));
  
  if(uwt.m_type != UanWakeupTag::DATA)
    return;

  UanPhyHeader phyHeader;
  NS_ASSERT (pkt->RemoveHeader (phyHeader));
  UanPhyTrailer phyTrailer;
  NS_ASSERT (pkt->RemoveTrailer (phyTrailer));

  UanHeaderCommon header;
  NS_ASSERT (pkt->PeekHeader (header));

  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " Wakeup " <<  (uint32_t) UanAddress::ConvertFrom (GetAddress ()).GetAsInt () << " receiving packet from " << header.GetSrc () << " For " << header.GetDest ());


  m_forUpCb (pkt, header.GetSrc ());

  m_phy->SetSleepMode(true);
  //std::cerr << Simulator::Now().GetSeconds() << " Phy Data sleep " << (uint32_t) m_address.GetAsInt() <<  std::endl;
}
void
UanMacWakeup::RxPacketGoodData (Ptr<Packet> pkt, double sinr, UanTxMode txMode)
{
  UanPhyHeader phyHeader;
  NS_ASSERT (pkt->RemoveHeader (phyHeader));
  UanPhyTrailer phyTrailer;
  NS_ASSERT (pkt->RemoveTrailer (phyTrailer));

  UanHeaderCommon header;
  NS_ASSERT (pkt->PeekHeader (header));

  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " Wakeup " << (uint32_t) UanAddress::ConvertFrom (GetAddress ()).GetAsInt ()
      << " receiving DATA from " << header.GetSrc () << " For " << header.GetDest ());


  m_forUpCb (pkt, header.GetSrc ());

  m_phy->SetSleepMode(true);
  //std::cerr << Simulator::Now().GetSeconds() << " Phy Data sleep" << (uint32_t) m_address.GetAsInt() << std::endl;
}
void
UanMacWakeup::RxPacketGoodW (Ptr<Packet> pkt, double sinr, UanTxMode txMode)
{
  UanWakeupTag uwt;
  NS_ASSERT (pkt->RemovePacketTag(uwt));

  if(uwt.m_type != UanWakeupTag::WU)
    return;

  UanPhyWUHeader phyWUHeader;
  pkt->RemoveHeader (phyWUHeader);

  UanHeaderWakeup header;
  pkt->RemoveHeader (header);
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " Wakeup " << (uint32_t) UanAddress::ConvertFrom (GetAddress ()).GetAsInt ()
      << " receiving wakeup packet for " << header.GetDest ());

  UanAddress dest = header.GetDest();

  if (dest != GetAddress () && dest != UanAddress::GetBroadcast())
      return;

  m_timerTxFail.Schedule (MilliSeconds (2));
  if (!m_toneRxCallback.IsNull())
    m_toneRxCallback ();

  if (m_highEnergyMode)
    {
      m_wakeupPhyHE->SetSleepMode (false);
      //std::cerr << Simulator::Now().GetSeconds() << " Phy HE wakeup" << (uint32_t) m_address.GetAsInt() << std::endl;
    }
  else
    {
      m_phy->SetSleepMode (false);
      //std::cerr << Simulator::Now().GetSeconds() << " Phy Data wakeup" << (uint32_t) m_address.GetAsInt() << std::endl;
    }
}
void
UanMacWakeup::RxPacketGoodWHE (Ptr<Packet> pkt, double sinr, UanTxMode txMode)
{
  UanWakeupTag uwt;
  NS_ASSERT (pkt->RemovePacketTag(uwt));

  if(uwt.m_type != UanWakeupTag::WUHE)
    return;

  UanHeaderWakeup header;
  pkt->RemoveHeader (header);
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << (uint32_t) UanAddress::ConvertFrom (GetAddress ()).GetAsInt ()
      << " receiving wakeupHE packet for " << header.GetDest ());

  UanAddress dest = header.GetDest();

  if (dest != GetAddress () && dest != UanAddress::GetBroadcast())
    {
      m_wakeupPhyHE->SetSleepMode (true);
      //std::cerr << Simulator::Now().GetSeconds() << " Phy HE sleep" << (uint32_t) m_address.GetAsInt() << std::endl;
      return;
    }

  m_timerTxFail.Schedule (MilliSeconds (2));
  m_wakeupPhyHE->SetSleepMode (true);
  //std::cerr << Simulator::Now().GetSeconds() << " Phy HE sleep" << (uint32_t) m_address.GetAsInt() << std::endl;
  m_phy->SetSleepMode (false);
  //std::cerr << Simulator::Now().GetSeconds() << " Phy Data wakeup" << (uint32_t) m_address.GetAsInt() << std::endl;
}

void
UanMacWakeup::RxPacketError (Ptr<Packet> pkt, double sinr)
{
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Received packet in error with sinr " << sinr);
  if (!m_timerEndTx.IsRunning())
    {
      m_phy->SetSleepMode (true);
      //std::cerr << Simulator::Now().GetSeconds() << " Phy Data sleep" << (uint32_t) m_address.GetAsInt() << std::endl;
    }
}

void
UanMacWakeup::RxPacketErrorW (Ptr<Packet> pkt, double sinr)
{
  NS_LOG_DEBUG ("" << Simulator::Now () << " Wakeup MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Received packet in error with sinr " << sinr);
}
void
UanMacWakeup::RxPacketErrorWHE (Ptr<Packet> pkt, double sinr)
{
  NS_LOG_DEBUG ("" << Simulator::Now () << " WakeupHE MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Received packet in error with sinr " << sinr);
  if (!m_timerEndTx.IsRunning())
    {
      m_wakeupPhyHE->SetSleepMode (true);
      //std::cerr << Simulator::Now().GetSeconds() << " Phy HE sleep" << (uint32_t) m_address.GetAsInt() << std::endl;
    }
    //m_phy->SetSleepModeWHE (true);
}

Address
UanMacWakeup::GetBroadcast (void) const
{
  UanAddress broadcast (255);
  return broadcast;
}

void
UanMacWakeup::On_timerEndTx (void)
{
  if (m_wuAlone)
    {
      m_wuAlone = false;
      return;
    }

  switch (m_state)
  {
  case WU:
    if (m_highEnergyMode)
      m_timerDelayTxWUHE.Schedule (MicroSeconds (m_timeDelayTx));
    else
      m_timerDelayTx.Schedule (MicroSeconds (m_timeDelayTx));
    break;

  case WUHE:
    m_timerDelayTx.Schedule (MicroSeconds (m_timeDelayTx));
    break;

  case DATA:
    m_pkt = 0;
    m_state = WU;
    if (!m_txEnd.IsNull())
      m_txEnd ();
    break;
  }


}

void
UanMacWakeup::On_timerDelayTxWUHE (void)
{
  SendWUHE();
}

void
UanMacWakeup::On_timerDelayTx (void)
{
  Send ();
}

void
UanMacWakeup::On_timerTxFail ()
{
  if (m_highEnergyMode)
    {
      m_wakeupPhyHE->SetSleepMode (true);
      //std::cerr << Simulator::Now().GetSeconds() << " Phy HE sleep" << (uint32_t) m_address.GetAsInt() << std::endl;
    }
  else
    {
      m_phy->SetSleepMode (true);
      //std::cerr << Simulator::Now().GetSeconds() << " Phy Data sleep" << (uint32_t) m_address.GetAsInt() << std::endl;
    }
}

/*
 * PhyListener
 */
void
UanMacWakeup::NotifyRxStart (void)
{
  m_timerTxFail.Cancel ();

  if (!m_stateChangeCb.IsNull())
    m_stateChangeCb(BUSY);
}

void
UanMacWakeup::NotifyRxEndOk (void)
{
  if (!m_stateChangeCb.IsNull())

  m_stateChangeCb(IDLE);
}

void
UanMacWakeup::NotifyRxEndError (void)
{
  if (!m_stateChangeCb.IsNull())
    m_stateChangeCb(IDLE);

  m_phy->SetSleepMode (true);
  if (m_wakeupPhyHE != NULL)
    m_wakeupPhyHE->SetSleepMode (true);
}

void
UanMacWakeup::NotifyCcaStart (void)
{
  if (!m_stateChangeCb.IsNull())

  m_stateChangeCb(BUSY);
}

void
UanMacWakeup::NotifyCcaEnd (void)
{
  if (!m_stateChangeCb.IsNull())

  m_stateChangeCb(IDLE);
}

void
UanMacWakeup::NotifyTxStart (Time duration)
{
  NS_ASSERT (m_timerEndTx.IsRunning() == false);
  if (!m_stateChangeCb.IsNull())

  m_stateChangeCb(BUSY);
  m_timerEndTx.Schedule (duration);
}

int64_t
UanMacWakeup::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_rand->SetStream (stream);
  return 1;
}
}



