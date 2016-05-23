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
#include "ns3/uan-header-common.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/attribute.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

#include "ns3/nstime.h"
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
    m_maxPacketSize (26)
{
  m_timerCONTEND.SetFunction (&UanMacFama::On_timerCONTEND, this);
  m_timerWaitToBackoff.SetFunction (&UanMacFama::On_timerWaitToBackoff, this);
  m_timerWfCTS.SetFunction (&UanMacFama::On_timerWfCTS, this);
  m_timerWfDATA.SetFunction (&UanMacFama::On_timerWfDATA, this);
  m_timerBackoff.SetFunction (&UanMacFama::On_timerBackoff, this);
  m_timerWfACK.SetFunction (&UanMacFama::On_timerWfACK, this);

  m_rtsSize = 0;
  m_useAck = true;
  m_sendingData = false;

  m_maxBulkSend = 1;
  m_bulkSend = 0;
  m_state = UanMacWakeup::IDLE;

  m_rand= CreateObject<UniformRandomVariable>();
}

UanMacFama::~UanMacFama ()
{
  StopTimer();
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
  StopTimer();
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
  UanAddress src = UanAddress::ConvertFrom (GetAddress ());
  UanAddress udest = UanAddress::ConvertFrom (dest);

  UanHeaderCommon header;
  header.SetSrc (src);
  header.SetDest (udest);
  header.SetType (DATA);

  packet->AddHeader (header);

 if(m_sendQueue.size() < 10){
    m_sendQueue.push(packet);
    NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" packet queueing increase to "<<m_sendQueue.size());
  }
  if (m_sendQueue.size () >= 1
      && !m_timerWaitToBackoff.IsRunning()
      && !m_timerCONTEND.IsRunning ()
      && m_state == UanMacWakeup::IDLE
	  && m_macState == IDLE)
    {
      m_macState = CONTEND;
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
    NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send RTS. Queue "<<m_sendQueue.size ());
  if(m_sendQueue.size() < 1)
    return false;
  //NS_ASSERT(m_sendQueue.size () > 0);

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

bool success = Send(packet);
  if(success){
    m_macState = WFCTS;
	/*uint32_t size = 10;// DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
    if (m_rtsSize > size)
      size = m_rtsSize - size;
    Ptr<Packet> packetRts = Create<Packet> (size);
    packetRts->AddHeader (header);*/
	m_timerWfCTS.Schedule(Seconds(m_maxPropTime * 2 + (size + packet->GetSize ()) * 8 / dataRate)*2);
  }
  return success;
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

  Ptr<Packet> packet = Create<Packet> (m_maxPacketSize);
  packet->AddHeader (header);

  bool success = Send(packet);
  if(success) {
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send CTS");
  m_macState = WFDATA;
  m_timerWfDATA.Schedule(Seconds(m_maxPropTime * 2 + (size + packet->GetSize ()) * 8 / dataRate));
  }
  return success;
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
    if (m_macState == IDLE)
      {
        m_timerWaitToBackoff.Cancel ();
        m_timerCONTEND.Cancel ();

        if (m_sendQueue.size () >= 1)
          {
             StartContend();
          }
      }

    break;
  case UanMacWakeup::BUSY:
    m_timerWaitToBackoff.Cancel ();
    m_timerCONTEND.Cancel ();

	
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
          m_macState = IDLE;
          m_sendQueue.pop ();

          if (m_sendQueue.size () >= 1 && m_bulkSend > 0)
            {

			 NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send Pkt Train (no ACK). Queue "<< m_sendQueue.size());
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
 			  StartContend();
            }
        }
    else{
		  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Sent DATA, WFACK");
		  uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
		  m_macState = WFACK;
		  m_sendingData = false;
		  m_timerWfACK.Schedule(Seconds(m_maxPropTime * 2 + (m_maxPacketSize+20 + 5) * 8 / dataRate));
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
  //NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<"Receiving packet from " << header.GetSrc () << " For " << header.GetDest ());

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
        StopTimer();
        if (m_useAck) SendAck (addr);
		
        NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" RX DATA from " << header.GetSrc ()<<"********************************");
        m_macState = IDLE;
		m_forUpCb (pkt, m_rxSrc);
        m_forUpCb (pkt, header.GetSrc ());
      }
   else{
	     NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" rx xDATA " );
		 StopTimer();
	     if(m_useAck){
	       m_timerBackoff.Schedule(Seconds(2*m_maxPropTime+0.1));
		 }
		 else{
		   m_timerBackoff.Schedule(Seconds(m_maxPropTime+0.1));
		 }

		 m_macState = BACKOFF;
	}
    break;
  case ACK:
    if (header.GetDest () == GetAddress () || header.GetDest () == UanAddress::GetBroadcast ())
      {
        m_sendQueue.pop ();
        m_sendingData = false;
        
		NS_ASSERT(m_macState = WFACK);
		m_timerWfACK.Cancel();
		StopTimer();
		m_macState = IDLE;
        if (m_sendQueue.size () >= 1 && m_bulkSend > 0)
          {
            Ptr<Packet> sendPkt = m_sendQueue.front();

            m_macState = SDATA;
            if (Send (sendPkt->Copy()))
              {
                NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send Pkt Train after ACK. Queue size"<< m_sendQueue.size());
                m_sendingData = true;
                m_bulkSend--;
                m_sendDataCallback (sendPkt);
              }
          }
        else if (m_sendQueue.size () >= 1
            && !m_timerWaitToBackoff.IsRunning()
            && !m_timerBackoff.IsRunning ())
          {
			NS_LOG_DEBUG ( " Schedule Next packet in Queue after ACK. ");
			StartContend();
         }

        m_forUpCb (pkt, header.GetSrc ());
      }
       else{
	    if(m_macState != WFDATA && m_macState != WFACK){
	      StopTimer();
		  m_macState = BACKOFF;
		  m_timerBackoff.Schedule(Seconds(m_maxPropTime+0.08));
		  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" rx xACK " );
		}
	  }
    break;
  }
}
void 
UanMacFama::StartContend(){
    NS_ASSERT(!m_timerCONTEND.IsRunning());
	NS_ASSERT(!m_timerWaitToBackoff.IsRunning());
	
    m_macState = CONTEND;
	NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds ()  <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Schedule Next packet. Queue size"<< m_sendQueue.size());
    // Size
    uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
    uint32_t size = 10;//DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
    m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + m_maxPacketSize) * 8 / dataRate));
}

void
UanMacFama::StopTimer(){
  m_timerBackoff.Cancel();
  m_timerCONTEND.Cancel();
  m_timerWaitToBackoff.Cancel();
  m_timerWfACK.Cancel();
  m_timerWfCTS.Cancel();
  m_timerWfDATA.Cancel();
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
      	if(m_macState != WFDATA){
	    NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" RX RTS from " << m_rxSrc);
		StopTimer();
        SendCTS (m_rxSrc, 0);
	  }
    }

    else
  {
    if(m_macState != WFDATA && m_macState != WFACK){
      StopTimer();
	  m_timerBackoff.Schedule(Seconds(2*m_maxPropTime+0.1));
	  m_macState = BACKOFF;
	  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" rx xRTS " );
	}
  }
}

void
UanMacFama::RxCTS (Ptr<Packet> pkt)
{
  if (m_rxDest == GetAddress())
  {
    if(m_macState == WFCTS && m_sendQueue.size() > 0){
	  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " RX CTS");
      //NS_ASSERT (m_sendQueue.size () > 0);
      Ptr<Packet> sendPkt = m_sendQueue.front();

      
      StopTimer();
	  m_macState = IDLE;
      if (Send (sendPkt->Copy()))
        {
          NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Sent DATA");
          //m_sendQueue.pop ();
          m_sendingData = true;
          m_sendDataCallback (sendPkt);
		  m_macState = SDATA;
        }
      }
    }
else{
    uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
    if(m_useAck){
	  StopTimer();
	  m_timerBackoff.Schedule(Seconds(4*m_maxPropTime+(m_maxPacketSize+10+8)*2*8/dataRate));
	  m_macState = BACKOFF;
	  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" rx xCTS " );
	}
	else{
	  StopTimer();
	  m_timerBackoff.Schedule(Seconds(2*m_maxPropTime+(m_maxPacketSize)*2*8/dataRate));
	  m_macState = BACKOFF;
	  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" rx xCTS " );
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
  NS_ASSERT(!m_timerCONTEND.IsRunning());
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Schedule RTS timer");
  m_timerCONTEND.Schedule (Seconds (m_rand->GetValue(0.1, m_maxBackoff)));
}

void
UanMacFama::On_timerCONTEND (void)
{
  SendRTS();
}
void 
UanMacFama::On_timerWfCTS(void){
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " not receive CTS");
  double bkoffNum=m_rand->GetValue(0.1,m_maxBackoff);
  m_timerBackoff.Schedule(Seconds(10*bkoffNum*(2*m_maxPropTime+0.1)));
  m_macState = BACKOFF;

}
void 
UanMacFama::On_timerWfDATA(void){
  m_macState = IDLE;
}
void 
UanMacFama::On_timerBackoff(void){
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Exit Backoff");
  m_macState = IDLE;
  if(m_sendQueue.size()){
    StopTimer();
	//StartContend();
	SendRTS();
  }
}

void
UanMacFama::On_timerWfACK(void){
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " not receive ACK");
  double bkoffNum=m_rand->GetValue(0.1,m_maxBackoff);
  m_timerBackoff.Schedule(Seconds(10*bkoffNum*(2*m_maxPropTime+0.1)));
  m_macState = BACKOFF;
  
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



