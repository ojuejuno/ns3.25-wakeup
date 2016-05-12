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

#include "uan-mac-cw-w.h"
#include "ns3/attribute.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/nstime.h"
#include "ns3/uan-header-common.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/log.h"

#include "uan-header-wakeup.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("UanMacCwW");

NS_OBJECT_ENSURE_REGISTERED (UanMacCwW);

UanMacCwW::UanMacCwW ()
  : UanMac (),
    m_phy (0),
    m_pktTx (0),
    m_state (IDLE),
    m_cleared (false)

{
  m_rv = CreateObject<UniformRandomVariable> ();
}

UanMacCwW::~UanMacCwW ()
{
}

void
UanMacCwW::Clear ()
{
  if (m_cleared)
    {
      return;
    }
  m_cleared = true;
  m_pktTx = 0;
  if (m_phy)
    {
      m_phy->Clear ();
      m_phy = 0;
    }
  m_sendEvent.Cancel ();
  m_txEndEvent.Cancel ();
}

void
UanMacCwW::DoDispose ()
{
  Clear ();
  UanMac::DoDispose ();
}

TypeId
UanMacCwW::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanMacCwW")
    .SetParent<UanMac> ()
    .SetGroupName ("Uan")
    .AddConstructor<UanMacCwW> ()
    .AddAttribute ("CW",
                   "The MAC parameter CW.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&UanMacCwW::m_cw),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("SlotTime",
                   "Time slot duration for MAC backoff.",
                   TimeValue (MilliSeconds (20)),
                   MakeTimeAccessor (&UanMacCwW::m_slotTime),
                   MakeTimeChecker ())
    .AddTraceSource ("Enqueue",
                     "A packet arrived at the MAC for transmission.",
                     MakeTraceSourceAccessor (&UanMacCwW::m_enqueueLogger),
                     "ns3::UanMacCwW::QueueTracedCallback")
    .AddTraceSource ("Dequeue",
                     "A was passed down to the PHY from the MAC.",
                     MakeTraceSourceAccessor (&UanMacCwW::m_dequeueLogger),
                     "ns3::UanMacCwW::QueueTracedCallback")
    .AddTraceSource ("RX",
                     "A packet was destined for this MAC and was received.",
                     MakeTraceSourceAccessor (&UanMacCwW::m_rxLogger),
                     "ns3::UanMac::PacketModeTracedCallback")

  ;
  return tid;
}

Address
UanMacCwW::GetAddress ()
{
  return this->m_address;
}

void
UanMacCwW::SetAddress (UanAddress addr)
{
  m_address = addr;
}

bool
UanMacCwW::Enqueue (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{

  switch (m_state)
    {
    case CCABUSY:
      NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " MAC " << GetAddress () << " Starting enqueue CCABUSY");
      if (m_txEndEvent.IsRunning ())
        {
          NS_LOG_DEBUG ("State is TX");
        }
      else
        {
          NS_LOG_DEBUG ("State is not TX");
        }

      NS_ASSERT (m_phy->GetTransducer ()->GetArrivalList ().size () >= 1 || m_phy->IsStateTx ());
      return false;
    case RUNNING:
      NS_LOG_DEBUG ("MAC " << GetAddress () << " Starting enqueue RUNNING");
      NS_ASSERT (m_phy->GetTransducer ()->GetArrivalList ().size () == 0 && !m_phy->IsStateTx ());
      return false;
    case TX:
	  NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () <<"MAC " << GetAddress () << " Starting enqueue TX");
	  return false;
    case IDLE:
      {
        NS_ASSERT (!m_pktTx);

        UanHeaderCommon header;
        header.SetDest (UanAddress::ConvertFrom (dest));
		
        header.SetSrc (m_address);
        header.SetType (0);
        packet->AddHeader (header);

        m_enqueueLogger (packet, protocolNumber);

        if (m_phy->IsStateBusy ())
          {
            m_pktTx = packet;
            m_dest = dest;
			m_pktTxProt = protocolNumber;
            m_state = CCABUSY;
            uint32_t cw = (uint32_t) m_rv->GetValue (0,m_cw);
            m_savedDelayS = Seconds ((double)(cw) * m_slotTime.GetSeconds ());
            m_sendTime = Simulator::Now () + m_savedDelayS;
            NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << ": Addr " << GetAddress () << ": Enqueuing new packet while busy:  (Chose CW " << cw << ", Sending at " << m_sendTime.GetSeconds () << " Packet size: " << packet->GetSize ());
            NS_ASSERT (m_phy->GetTransducer ()->GetArrivalList ().size () >= 1 || m_phy->IsStateTx ());
          }
        else
          {
            
            NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << ": Addr " << GetAddress () << ": Enqueuing new packet while idle (sending)");
            NS_ASSERT (m_state != TX);
			NS_ASSERT (m_phy->GetTransducer ()->GetArrivalList ().size () == 0 && !m_phy->IsStateTx ());
            m_state = TX;
			//UanWakeupTag uwt;
			//uwt.m_type = UanWakeupTag::DATA;
            //m_pktTx->AddPacketTag (uwt);
			m_sendDataCallback (packet->Copy());
			m_mac->Enqueue (packet, dest, 0);

            //m_phy->SendPacket (packet,protocolNumber);

          }
        break;
      }
    default:
      NS_LOG_DEBUG ("MAC " << GetAddress () << " Starting enqueue SOMETHING ELSE");
      return false;
    }

  return true;


}

void
UanMacCwW::SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress&> cb)
{
  m_forwardUpCb = cb;
}

void
UanMacCwW::SetSendDataCallback (Callback<void, Ptr<Packet> > sendDataCallback)
{
  m_sendDataCallback = sendDataCallback;
}


void
UanMacCwW::AttachPhy (Ptr<UanPhy> phy)
{
  m_phy = phy;
  //m_phy->SetReceiveOkCallback (MakeCallback (&UanMacCwW::PhyRxPacketGood, this));
  //m_phy->SetReceiveErrorCallback (MakeCallback (&UanMacCwW::PhyRxPacketError, this));
  //m_phy->RegisterListener (this);
}

void
UanMacCwW::AttachMacWakeup (Ptr<UanMacWakeup> mac)
{
  m_mac = mac;
  m_mac->SetForwardUpCb (MakeCallback (&UanMacCwW::PhyRxPacketGood, this));
}



Address
UanMacCwW::GetBroadcast (void) const
{
  return UanAddress::GetBroadcast ();
}



void
UanMacCwW::PhyStateCb (UanMacWakeup::PhyState phyState)
{
  m_wakeupState = phyState;

  switch (m_wakeupState)
  {
  case UanMacWakeup::IDLE:
      if (m_state == CCABUSY&& !m_phy->IsStateCcaBusy ())
    {
      NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << ": Switching to channel idle");
      m_state = RUNNING;
      StartTimer ();

    }

    break;
  case UanMacWakeup::BUSY:
    if (m_state == RUNNING)
    {
      NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << ": Switching to channel busy");
      m_state = CCABUSY;
      SaveTimer ();

    }
    break;
  }
}








int64_t
UanMacCwW::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_rv->SetStream (stream);
  return 1;
}

void
UanMacCwW::EndTx (void)
{
  NS_ASSERT (m_state == TX || m_state == CCABUSY);
  if (m_state == TX)
    {
      m_state = IDLE;
	  NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () <<" ENd TX to IDLE");
    }
  else if (m_state == CCABUSY)
    {
      if (m_phy->IsStateIdle ())
        {
          NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << ": Switching to channel idle (After TX!)");
          m_state = RUNNING;
          StartTimer ();
        }
    }
  else
    {
      NS_FATAL_ERROR ("In strange state at UanMacCwW EndTx");
    }
}
void
UanMacCwW::SetCw (uint32_t cw)
{
  m_cw = cw;
}
void
UanMacCwW::SetSlotTime (Time duration)
{
  m_slotTime = duration;
}
uint32_t
UanMacCwW::GetCw (void)
{
  return m_cw;
}
Time
UanMacCwW::GetSlotTime (void)
{
  return m_slotTime;
}
void
UanMacCwW::PhyRxPacketGood (Ptr<Packet> packet, const UanAddress& add)
{
  UanHeaderCommon header;
  packet->RemoveHeader (header);

  if (header.GetDest () == m_address || header.GetDest () == UanAddress::GetBroadcast ())
    {
      m_forwardUpCb (packet, header.GetSrc ());
    }
}
void
UanMacCwW::PhyRxPacketError (Ptr<Packet> packet, double sinr)
{

}
void
UanMacCwW::SaveTimer (void)
{
  NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << " Saving timer (Delay = " << (m_savedDelayS = m_sendTime - Simulator::Now ()).GetSeconds () << ")");
  NS_ASSERT (m_pktTx);
  NS_ASSERT (m_sendTime >= Simulator::Now ());
  m_savedDelayS = m_sendTime - Simulator::Now ();
  Simulator::Cancel (m_sendEvent);


}
void
UanMacCwW::StartTimer (void)
{

  m_sendTime = Simulator::Now () + m_savedDelayS;
  if (m_sendTime == Simulator::Now ())
    {
      SendPacket ();
    }
  else
    {
      m_sendEvent = Simulator::Schedule (m_savedDelayS, &UanMacCwW::SendPacket, this);
      NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << " Starting timer (New send time = " << this->m_sendTime.GetSeconds () << ")");
    }
}

void
UanMacCwW::SendPacket (void)
{
  NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << " Transmitting ");
  NS_ASSERT (m_state == RUNNING);
  m_state = TX;

  //UanHeaderCommon pktHeader;
 //m_pktTx->PeekHeader (pktHeader);
  m_mac->Enqueue (m_pktTx, m_dest, 0);
  //m_phy->SendPacket (m_pktTx,m_pktTxProt);
  m_sendDataCallback (m_pktTx->Copy());
  m_pktTx = 0;
  m_sendTime = Seconds (0);
  m_savedDelayS = Seconds (0);

}

} // namespace ns3
