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

#include "uan-mac-aloha-cs.h"
#include "ns3/attribute.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/nstime.h"
#include "ns3/uan-header-common.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("UanMacAlohaCs");

NS_OBJECT_ENSURE_REGISTERED (UanMacAlohaCs);

UanMacAlohaCs::UanMacAlohaCs ()
  : UanMac (),
    m_phy (0),
    m_pktTx (0),
    m_state (IDLE),
    m_cleared (false)

{
  m_rv = CreateObject<UniformRandomVariable> ();
}

UanMacAlohaCs::~UanMacAlohaCs ()
{
}

void
UanMacAlohaCs::Clear ()
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
UanMacAlohaCs::DoDispose ()
{
  Clear ();
  UanMac::DoDispose ();
}

TypeId
UanMacAlohaCs::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanMacAlohaCs")
    .SetParent<UanMac> ()
    .SetGroupName ("Uan")
    .AddConstructor<UanMacAlohaCs> ()
    .AddAttribute ("CW",
                   "The MAC parameter CW.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&UanMacAlohaCs::m_cw),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("SlotTime",
                   "Time slot duration for MAC backoff.",
                   TimeValue (MilliSeconds (50)),
                   MakeTimeAccessor (&UanMacAlohaCs::m_slotTime),
                   MakeTimeChecker ())
    .AddTraceSource ("Enqueue",
                     "A packet arrived at the MAC for transmission.",
                     MakeTraceSourceAccessor (&UanMacAlohaCs::m_enqueueLogger),
                     "ns3::UanMacAlohaCs::QueueTracedCallback")
    .AddTraceSource ("Dequeue",
                     "A was passed down to the PHY from the MAC.",
                     MakeTraceSourceAccessor (&UanMacAlohaCs::m_dequeueLogger),
                     "ns3::UanMacAlohaCs::QueueTracedCallback")
    .AddTraceSource ("RX",
                     "A packet was destined for this MAC and was received.",
                     MakeTraceSourceAccessor (&UanMacAlohaCs::m_rxLogger),
                     "ns3::UanMac::PacketModeTracedCallback")

  ;
  return tid;
}

Address
UanMacAlohaCs::GetAddress ()
{
  return this->m_address;
}

void
UanMacAlohaCs::SetAddress (UanAddress addr)
{
  m_address = addr;
}

bool
UanMacAlohaCs::Enqueue (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
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
            m_pktTxProt = protocolNumber;
            m_state = CCABUSY;
            uint32_t cw = m_cw;
            m_savedDelayS = Seconds ((double)(cw) * m_slotTime.GetSeconds ());
            m_sendTime = Simulator::Now () + m_savedDelayS;
            NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << ": Addr " << GetAddress () << ": Enqueuing new packet while busy:  (Chose CW " << cw << ", Sending at " << m_sendTime.GetSeconds () << " Packet size: " << packet->GetSize ());
            NS_ASSERT (m_phy->GetTransducer ()->GetArrivalList ().size () >= 1 || m_phy->IsStateTx ());
          }
        else
          {
            NS_ASSERT (m_state != TX);
            NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << ": Addr " << GetAddress () << ": Enqueuing new packet while idle (sending)");
            NS_ASSERT (m_phy->GetTransducer ()->GetArrivalList ().size () == 0 && !m_phy->IsStateTx ());
            m_state = TX;
            m_phy->SendPacket (packet,protocolNumber);

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
UanMacAlohaCs::SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress&> cb)
{
  m_forwardUpCb = cb;
}

void
UanMacAlohaCs::AttachPhy (Ptr<UanPhy> phy)
{
  m_phy = phy;
  m_phy->SetReceiveOkCallback (MakeCallback (&UanMacAlohaCs::PhyRxPacketGood, this));
  m_phy->SetReceiveErrorCallback (MakeCallback (&UanMacAlohaCs::PhyRxPacketError, this));
  m_phy->RegisterListener (this);
}

Address
UanMacAlohaCs::GetBroadcast (void) const
{
  return UanAddress::GetBroadcast ();
}


void
UanMacAlohaCs::NotifyRxStart (void)
{
  if (m_state == RUNNING)
    {

      NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << ": Switching to channel busy");
      SaveTimer ();
      m_state = CCABUSY;
    }

}
void
UanMacAlohaCs::NotifyRxEndOk (void)
{
  if (m_state == CCABUSY && !m_phy->IsStateCcaBusy ())
    {
      NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << ": Switching to channel idle");
      m_state = RUNNING;
      StartTimer ();

    }

}
void
UanMacAlohaCs::NotifyRxEndError (void)
{
  if (m_state == CCABUSY && !m_phy->IsStateCcaBusy ())
    {
      NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << ": Switching to channel idle");
      m_state = RUNNING;
      StartTimer ();

    }

}
void
UanMacAlohaCs::NotifyCcaStart (void)
{
  if (m_state == RUNNING)
    {
      NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << ": Switching to channel busy");
      m_state = CCABUSY;
      SaveTimer ();

    }

}
void
UanMacAlohaCs::NotifyCcaEnd (void)
{
  if (m_state == CCABUSY)
    {
      NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << ": Switching to channel idle");
      m_state = RUNNING;
      StartTimer ();

    }

}
void
UanMacAlohaCs::NotifyTxStart (Time duration)
{

  if (m_txEndEvent.IsRunning ())
    {
      Simulator::Cancel (m_txEndEvent);
    }

  m_txEndEvent = Simulator::Schedule (duration, &UanMacAlohaCs::EndTx, this);
  NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " scheduling TxEndEvent with delay " << duration.GetSeconds ());
  if (m_state == RUNNING)
    {
      NS_ASSERT (0);
      m_state = CCABUSY;
      SaveTimer ();

    }

}

int64_t
UanMacAlohaCs::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_rv->SetStream (stream);
  return 1;
}

void
UanMacAlohaCs::EndTx (void)
{
  NS_ASSERT (m_state == TX || m_state == CCABUSY);
  if (m_state == TX)
    {
      m_state = IDLE;
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
      NS_FATAL_ERROR ("In strange state at UanMacAlohaCs EndTx");
    }
}
void
UanMacAlohaCs::SetCw (uint32_t cw)
{
  m_cw = cw;
}
void
UanMacAlohaCs::SetSlotTime (Time duration)
{
  m_slotTime = duration;
}
uint32_t
UanMacAlohaCs::GetCw (void)
{
  return m_cw;
}
Time
UanMacAlohaCs::GetSlotTime (void)
{
  return m_slotTime;
}
void
UanMacAlohaCs::PhyRxPacketGood (Ptr<Packet> packet, double sinr, UanTxMode mode)
{
  UanHeaderCommon header;
  packet->RemoveHeader (header);

  if (header.GetDest () == m_address || header.GetDest () == UanAddress::GetBroadcast ())
    {
      m_forwardUpCb (packet, header.GetSrc ());
    }
}
void
UanMacAlohaCs::PhyRxPacketError (Ptr<Packet> packet, double sinr)
{

}
void
UanMacAlohaCs::SaveTimer (void)
{
  NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << " Saving timer (Delay = " << (m_savedDelayS = m_sendTime - Simulator::Now ()).GetSeconds () << ")");
  NS_ASSERT (m_pktTx);
  NS_ASSERT (m_sendTime >= Simulator::Now ());
  m_savedDelayS = m_sendTime - Simulator::Now ();
  Simulator::Cancel (m_sendEvent);


}
void
UanMacAlohaCs::StartTimer (void)
{

  m_sendTime = Simulator::Now () + m_savedDelayS;
  if (m_sendTime == Simulator::Now ())
    {
      SendPacket ();
    }
  else
    {
      m_sendEvent = Simulator::Schedule (m_savedDelayS, &UanMacAlohaCs::SendPacket, this);
      NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << " Starting timer (New send time = " << this->m_sendTime.GetSeconds () << ")");
    }
}

void
UanMacAlohaCs::SendPacket (void)
{
  NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " Addr " << GetAddress () << " Transmitting ");
  NS_ASSERT (m_state == RUNNING);
  m_state = TX;
  m_phy->SendPacket (m_pktTx,m_pktTxProt);
  m_pktTx = 0;
  m_sendTime = Seconds (0);
  m_savedDelayS = Seconds (0);
}

} // namespace ns3
