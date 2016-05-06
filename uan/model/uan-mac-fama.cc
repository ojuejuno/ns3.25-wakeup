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

#include "uan-mac-fama.h"
#include "uan-tx-mode.h"
#include "uan-address.h"
#include "ns3/log.h"
#include "uan-phy.h"
#include "uan-header-common.h"
#include "uan-header-wakeup.h"
#include "ns3/double.h"


#include <iostream>
NS_LOG_COMPONENT_DEFINE ("UanMacFama");


namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED (UanMacFama);

UanMacFama::UanMacFama ()
  : UanMac (),
    m_cleared (false),
    m_maxBackoff (0.5),
    m_maxPropTime (0.3),
    m_maxPacketSize (64)
{
  m_timerBackoff.SetFunction (&UanMacFama::On_timerBackoff, this);
  m_timerWaitToBackoff.SetFunction (&UanMacFama::On_timerWaitToBackoff, this);

  m_rtsSize = 0;
  m_useAck = false;
  m_sendingData = false;

  m_maxBulkSend = 0;
  m_bulkSend = 0;
  m_state = UanMacWakeup::IDLE;

  m_rand= CreateObject<UniformRandomVariable>();
}

UanMacFama::~UanMacFama ()
{
  m_timerBackoff.Cancel ();
  m_timerWaitToBackoff.Cancel ();
}

void
UanMacFama::Clear ()
{
  if (m_cleared)
    {
      return;
    }
  m_cleared = true;
  m_timerBackoff.Cancel ();
  if (m_mac)
    {
      m_mac->Clear ();
      m_mac = 0;
    }
}

void
UanMacFama::DoDispose ()
{
  Clear ();
  UanMac::DoDispose ();
}

TypeId
UanMacFama::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanMacFama")
    .SetParent<UanMac> ()
	.SetGroupName ("Uan")
    .AddConstructor<UanMacFama> ()
  ;
  return tid;
}

Address
UanMacFama::GetAddress (void)
{
  return m_mac->GetAddress();
}

void
UanMacFama::SetAddress (UanAddress addr)
{
  m_mac->SetAddress (addr);
  m_address = addr;
}

void
UanMacFama::SetBackoffTime (double backoff)
{
  m_maxBackoff = backoff;
}
double
UanMacFama::GetBackoffTime ()
{
  return m_maxBackoff;
}

void
UanMacFama::SetRtsSize (uint32_t size)
{
  m_rtsSize = size;
}
uint32_t
UanMacFama::GetRtsSize () const
{
  return m_rtsSize;
}

void
UanMacFama::SetUseAck (bool ack)
{
  m_useAck = ack;
}
bool
UanMacFama::GetUseAck () const
{
  return m_useAck;
}

void
UanMacFama::SetBulkSend (uint8_t bulkSend)
{
  m_bulkSend = m_maxBulkSend = (bulkSend > 0) ? bulkSend - 1 : 0;
}

bool
UanMacFama::Enqueue (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Queueing packet for " << UanAddress::ConvertFrom (dest));

  UanAddress src = UanAddress::ConvertFrom (GetAddress ());
  UanAddress udest = UanAddress::ConvertFrom (dest);

  UanHeaderCommon header;
  header.SetSrc (src);
  header.SetDest (udest);
  header.SetType (DATA);

  packet->AddHeader (header);

  m_sendQueue.push(packet);
  if (m_sendQueue.size () >= 1
      && !m_timerWaitToBackoff.IsRunning()
      && !m_timerBackoff.IsRunning ()
      && m_state == UanMacWakeup::IDLE)
    {
      uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
      UanHeaderCommon header;
      header.SetSrc (src);
      header.SetDest (udest);
      header.SetType (RTS);

      // RTS size
      uint32_t size = DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
      if (m_rtsSize > size)
        size = m_rtsSize - size;

      Ptr<Packet> packetRts = Create<Packet> (size);
      packetRts->AddHeader (header);

      m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + packetRts->GetSize ()) * 8 / dataRate));
    }

  return true;
}

bool
UanMacFama::SendRTS ()
{
  NS_ASSERT(m_sendQueue.size () > 0);

  m_bulkSend = m_maxBulkSend;

  uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();

  UanHeaderCommon dataHeader;
  Ptr<Packet> pkt = m_sendQueue.front ();
  pkt->PeekHeader (dataHeader);

  UanAddress udest = dataHeader.GetDest();
  UanAddress src = UanAddress::ConvertFrom (GetAddress ());

  UanHeaderCommon header;
  header.SetSrc (src);
  header.SetDest (udest);
  header.SetType (RTS);

  // RTS size
  uint32_t size = DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
  if (m_rtsSize > size)
    size = m_rtsSize - size;

  Ptr<Packet> packet = Create<Packet> (size);
  packet->AddHeader (header);

  m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + packet->GetSize ()) * 8 / dataRate));


  return Send (packet);
}

bool
UanMacFama::SendCTS (const Address &dest, const double duration)
{
  uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();

  UanAddress src = UanAddress::ConvertFrom (GetAddress ());
  UanAddress udest = UanAddress::ConvertFrom (dest);

  UanHeaderCommon header;
  header.SetSrc (src);
  header.SetDest (udest);
  header.SetType (CTS);

  // CTS size
  uint32_t size = DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
  if (m_rtsSize > size)
    size = m_rtsSize - size;

  Ptr<Packet> packet = Create<Packet> (size);
  packet->AddHeader (header);

  m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + packet->GetSize ()) * 8 / dataRate));

  return Send (packet);
}

bool
UanMacFama::SendWakeupBroadcast (Ptr<Packet> pkt)
{
  UanHeaderCommon header;
  NS_ASSERT (pkt->PeekHeader (header));

  return m_mac->Enqueue (pkt, UanAddress::GetBroadcast(), 0);
}

bool
UanMacFama::Send (Ptr<Packet> pkt)
{
  UanHeaderCommon header;
  NS_ASSERT (pkt->PeekHeader (header));

  return m_mac->Enqueue (pkt, header.GetDest (), 0);
}

void
UanMacFama::SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress& > cb)
{
  m_forUpCb = cb;
}

void
UanMacFama::PhyStateCb (UanMacWakeup::PhyState phyState)
{
  m_state = phyState;

  switch (phyState)
  {
  case UanMacWakeup::IDLE:
    if (!m_sendingData)
      {
        m_timerWaitToBackoff.Cancel ();
        m_timerBackoff.Cancel ();

        if (m_sendQueue.size () >= 1)
          {
            // Size
            uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
            uint32_t size = DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
            m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + m_maxPacketSize) * 8 / dataRate));
          }
      }

    break;
  case UanMacWakeup::BUSY:
    m_timerWaitToBackoff.Cancel ();
    m_timerBackoff.Cancel ();

    break;
  }
}

void
UanMacFama::TxEnd ()
{
  if (m_sendingData)
    {
      if (!m_useAck)
        {
          m_sendingData = false;
          m_sendQueue.pop ();

          if (m_sendQueue.size () >= 1 && m_bulkSend > 0)
            {
              Ptr<Packet> sendPkt = m_sendQueue.front();

              if (Send (sendPkt->Copy()))
                {
                  m_sendingData = true;
                  m_bulkSend--;
                  m_sendDataCallback (sendPkt);
                }
            }
          else if (m_sendQueue.size () >= 1
              && !m_timerWaitToBackoff.IsRunning()
              && !m_timerBackoff.IsRunning ())
            {
              NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds ()  <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Shedule Next packet in Queue (no ACK). Queue "<< m_sendQueue.size());
              // Size
              uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
              uint32_t size = DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
              m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + m_maxPacketSize) * 8 / dataRate));
            }
        }
    }
}


void
UanMacFama::AttachPhy (Ptr<UanPhy> phy)
{
  m_phy = phy;
}
void
UanMacFama::AttachMacWakeup (Ptr<UanMacWakeup> mac)
{
  m_mac = mac;
  m_mac->SetForwardUpCb (MakeCallback (&UanMacFama::RxPacket, this));
}

bool
UanMacFama::SendAck (const UanAddress& dest)
{
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send ACK");
  UanHeaderCommon header;
  header.SetSrc (UanAddress::ConvertFrom (GetAddress ()));
  header.SetDest (dest);
  header.SetType (ACK);

  Ptr<Packet> pkt = Create<Packet> ();
  pkt->AddHeader (header);

  return Send (pkt);
}

void
UanMacFama::RxPacket (Ptr<Packet> pkt, const UanAddress& addr)
{
  UanHeaderCommon header;
  pkt->RemoveHeader (header);
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<"Receiving packet from " << header.GetSrc () << " For " << header.GetDest ());

  m_rxSrc = header.GetSrc();
  m_rxDest = header.GetDest();
  m_rxType = header.GetType();
  m_rxSize = header.GetSerializedSize();

  if (m_rxSrc == GetAddress ())
    return;


  uint8_t type = header.GetType();

  switch (type)
  {
  case RTS:
    RxRTS (pkt);
    break;
  case CTS:
    RxCTS (pkt);
    break;
  case DATA:
    if (header.GetDest () == GetAddress () || header.GetDest () == UanAddress::GetBroadcast ())
      {
        if (m_useAck) SendAck (addr);

        m_forUpCb (pkt, header.GetSrc ());
      }
    break;
  case ACK:
    if (header.GetDest () == GetAddress () || header.GetDest () == UanAddress::GetBroadcast ())
      {
        m_sendQueue.pop ();
        m_sendingData = false;

        if (m_sendQueue.size () >= 1 && m_bulkSend > 0)
          {
            Ptr<Packet> sendPkt = m_sendQueue.front();

            if (Send (sendPkt->Copy()))
              {
                m_sendingData = true;
                m_bulkSend--;
                m_sendDataCallback (sendPkt);
              }
          }
        else if (m_sendQueue.size () >= 1
            && !m_timerWaitToBackoff.IsRunning()
            && !m_timerBackoff.IsRunning ())
          {
            // Size
            uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
            uint32_t size = DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
            m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + m_maxPacketSize) * 8 / dataRate));
          }

        m_forUpCb (pkt, header.GetSrc ());
      }
    break;
  }
}

void
UanMacFama::RxPacketGood (Ptr<Packet> pkt, double sinr, UanTxMode txMode)
{

}

void
UanMacFama::RxRTS (Ptr<Packet> pkt)
{
  if (m_rxDest == GetAddress())
    {
      SendCTS (m_rxSrc, 0);
    }
}

void
UanMacFama::RxCTS (Ptr<Packet> pkt)
{
  if (m_rxDest == GetAddress())
    {

	  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send DATA");
      NS_ASSERT (m_sendQueue.size () > 0);
      Ptr<Packet> sendPkt = m_sendQueue.front();

      if (Send (sendPkt->Copy()))
        {
          //m_sendQueue.pop ();
          m_sendingData = true;
          m_sendDataCallback (sendPkt);
        }
    }
}

void
UanMacFama::RxPacketError (Ptr<Packet> pkt, double sinr)
{
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Received packet in error with sinr " << sinr);
}

Address
UanMacFama::GetBroadcast (void) const
{
  UanAddress broadcast (255);
  return broadcast;
}

void
UanMacFama::On_timerWaitToBackoff (void)
{

  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << "Schedule RTS timer");
  m_timerBackoff.Schedule (Seconds (m_rand->GetValue(0.1, m_maxBackoff)));
}

void
UanMacFama::On_timerBackoff (void)
{
  SendRTS();
}

void
UanMacFama::SetSendDataCallback (Callback<void, Ptr<Packet> > sendDataCallback)
{
  m_sendDataCallback = sendDataCallback;
}

int64_t
UanMacFama::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_rand->SetStream (stream);
  return 1;
}
}



