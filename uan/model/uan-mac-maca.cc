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

#include "uan-mac-maca.h"
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

#include <iostream>





namespace ns3
{
NS_LOG_COMPONENT_DEFINE ("UanMacMaca");
NS_OBJECT_ENSURE_REGISTERED (UanMacMaca);

UanMacMaca::UanMacMaca ()
  :m_cleared (false),
  m_maxBackoff (0.5),
  m_maxPropTime (0.3),
  m_maxPacketSize (64)
  {
  m_timerSendBackoff.SetFunction (&UanMacMaca::OntimerSendBackoff,this);
  m_timerQuiet.SetFunction (&UanMacMaca::OntimerQuiet,this);
  m_timerWFCTS.SetFunction (&UanMacMaca::OntimerWFCTS,this);
  
  m_rtsSize = 0;
  
  m_maxBulkSend = 0;
  m_bulkSend = 0;
  m_state = UanMacWakeup::IDLE;
  m_macaState = IDLE;
  m_rand= CreateObject<UniformRandomVariable>();
  
  m_phy = 0;
  m_tCTS = 0.05;
  m_tDATA = 0.22;
    
  m_minBEB=1;
  m_maxBEB=10;
  m_countBEB=1;
  }
  
UanMacMaca::~UanMacMaca ()
{
  m_timerSendBackoff.Cancel ();
  m_timerQuiet.Cancel ();
  m_timerWFCTS.Cancel ();
}

void
UanMacMaca::Clear ()
{
  if (m_cleared)
    {
      return;
    }
  m_cleared = true;
  m_timerSendBackoff.Cancel ();
  if (m_phy)
    {
      m_phy->Clear ();

      m_phy = 0;
    }
}

void
UanMacMaca::DoDispose ()
{
  Clear ();
  UanMac::DoDispose ();
}

TypeId
UanMacMaca::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanMacMaca")
    .SetParent<Object> ()
	.SetGroupName ("Uan")
    .AddConstructor<UanMacMaca> ()
  ;
  return tid;
}

Address
UanMacMaca::GetAddress (void)
{
  return this->m_address;
}

void
UanMacMaca::SetAddress (UanAddress addr)
{
  m_address = addr;
}

void
UanMacMaca::SetBackoffTime (double backoff)
{
  m_maxBackoff = backoff;
}
double
UanMacMaca::GetBackoffTime ()
{
  return m_maxBackoff;
}

void
UanMacMaca::SetRtsSize (uint32_t size)
{
  m_rtsSize = size;
}
uint32_t
UanMacMaca::GetRtsSize () const
{
  return m_rtsSize;
}


void
UanMacMaca::SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress& > cb)
{
  m_forUpCb = cb;
}


bool
UanMacMaca::Enqueue (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber){
   //NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Queueing packet for " << UanAddress::ConvertFrom (dest));

  UanAddress src = UanAddress::ConvertFrom (GetAddress ());
  UanAddress udest = UanAddress::ConvertFrom (dest);

  UanHeaderCommon header;
  header.SetSrc (src);
  header.SetDest (udest);
  header.SetType (DATA);

  packet->AddHeader (header);
   DynamicCast<UanMacWakeupMaca> (m_mac) -> SetSleepMode(false);
  if(m_sendQueue.size() < 10){
    m_sendQueue.push(packet);
    //NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" packet queueing increase to "<<m_sendQueue.size());
  }
  
  if (m_macaState == IDLE){
    m_macaState = CONTEND;
	
	if (!m_timerSendBackoff.IsRunning())
	  BackoffNextSend ();
	else if (m_sendQueue.size() == 1){
	  m_timerSendBackoff.Cancel();
	  BackoffNextSend();
	}
  }
  return true;
}

void
UanMacMaca::BackoffNextSend ()
{
  NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" sendBackoff ");
  NS_ASSERT(!m_timerSendBackoff.IsRunning());
  double backoffTime = (double)(m_rand->GetValue(0,m_countBEB))*(m_maxPropTime+m_tCTS);
  m_timerSendBackoff.Schedule(Seconds(backoffTime));
}

void
UanMacMaca::BackoffNextSend (bool b)
{
  NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" sendBackoff after Data");
  NS_ASSERT(!m_timerSendBackoff.IsRunning());
  double backoffTime = (double)(m_rand->GetValue(0,m_countBEB))*(m_maxPropTime+m_tCTS);
  m_timerSendBackoff.Schedule(Seconds(backoffTime*1));
}

bool
UanMacMaca::SendRTS ()
{
    NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send RTS. Queue "<<m_sendQueue.size ());
  if((m_sendQueue.size() + m_relayQueue.size())< 1)
    return false;
  //NS_ASSERT(m_sendQueue.size () >= 1);

  //m_bulkSend = m_maxBulkSend;

//Get a RTS header
  UanHeaderCommon dataHeader;
  Ptr<Packet> pkt=0;
  if(!m_relayQueue.empty())  pkt=m_relayQueue.front();
  else  pkt = m_sendQueue.front ();
  pkt->PeekHeader (dataHeader);

  UanAddress udest = dataHeader.GetDest();
  UanAddress src = UanAddress::ConvertFrom (GetAddress ());

  UanHeaderCommon rtsHeader;
  rtsHeader.SetSrc (src);
  rtsHeader.SetDest (udest);
  rtsHeader.SetType (RTS);
  

  // RTS size
 uint8_t size = DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
  if (m_rtsSize > size)
    size = m_rtsSize - size;

  Ptr<Packet> packet = Create<Packet> (size);
  packet->AddHeader (rtsHeader);
  /*UanHeaderCommon dataHeader;
  Ptr<Packet> pkt=0;
  if(!m_relayQueue.empty())  pkt=m_relayQueue.front();
  else  pkt = m_sendQueue.front ();
  pkt->PeekHeader (dataHeader);

  UanAddress udest = dataHeader.GetDest();
  UanAddress src = UanAddress::ConvertFrom (GetAddress ());
  
  DynamicCast<UanMacWakeupMaca> (m_mac) -> SendBroadcastCTD(udest);*/
  
  
  
  //return true;

  return Send (packet);
}

bool
UanMacMaca::Send (Ptr<Packet> pkt)
{
  UanHeaderCommon header;
  NS_ASSERT (pkt->PeekHeader (header));
  UanAddress dest = header.GetDest();
  //uint32_t protocolNumber=0;
 if(!m_phy->IsStateBusy()){
    DynamicCast<UanMacWakeupMaca> (m_mac) -> Enqueue (pkt, dest, 0);
    //m_phy->SendPacket (pkt, 0);
	
	if(!m_sendQueue.size()){
	  DynamicCast<UanMacWakeupMaca> (m_mac) -> SetSleepMode(true);
	}
    return true;
  }
  else 
    return false;
}


void
UanMacMaca::RxPacket (Ptr<Packet> pkt,const UanAddress& dst)
{
  UanHeaderCommon header;
  pkt->RemoveHeader (header);
  

  m_rxSrc = header.GetSrc();
  m_rxDest = header.GetDest();
  m_rxType = header.GetType();
  m_rxSize = header.GetSerializedSize();
  
  if (UanAddress::ConvertFrom (GetAddress ()) == m_rxDest){
    //NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" Receiving "<< int(m_rxType) <<" packet from " << m_rxSrc);
  }
  else{
    //NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" Receiving "<< int(m_rxType) <<" packet from " << m_rxSrc << " For " << m_rxDest);
  }
  if (m_rxSrc == GetAddress ())
  
    return;



  uint8_t type = header.GetType();

  switch (type)
  {
  case RTS:
    RxRTS (pkt,m_rxDest);
    break;
  case CTS:
    RxCTS (pkt,m_rxDest);
    break;
  case DATA:
    if (m_macaState == WFDATA){
	
	  if (m_rxDest == GetAddress () || m_rxDest == UanAddress::GetBroadcast ())
        {
		   NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" RX DATA from "<<  m_rxSrc <<"***************");
           m_macaState = IDLE;
		   m_timerWFCTS.Cancel();
		   m_forUpCb (pkt, m_rxSrc);
        }
    }
	else if(m_macaState == QUIET && m_rxDest != GetAddress ())
	{
	   m_timerQuiet.Cancel();
	   NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds ()<<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << "end quiet");
       m_macaState = IDLE;
       BackoffNextSend ();
	}
    break;
  }
}

void
UanMacMaca::RxRTS (Ptr<Packet> pkt,const UanAddress& dst)
{
  if(dst == GetAddress()) {
    DynamicCast<UanMacWakeupMaca> (m_mac) -> SetSleepMode(false);
  }
  
  if(m_macaState == QUIET && (dst != GetAddress())){
      NS_ASSERT(m_timerQuiet.IsRunning());
	  double newtime = std::max(m_timerQuiet.GetDelayLeft().GetSeconds(),(2 * m_maxPropTime + m_tCTS));
	  m_timerQuiet.Cancel();
	  m_timerQuiet.Schedule(Seconds(newtime));
	  //NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds ()<< " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" hear xRTS reset quiet "<<newtime);
	}
  else if (m_macaState == IDLE || m_macaState == CONTEND){
    if (dst == GetAddress()){
      if (SendCTS (m_rxSrc, 0)){
		m_timerSendBackoff.Cancel();
		m_macaState=WFDATA;
		m_timerWFCTS.Schedule(Seconds(m_maxPropTime*2+m_tDATA));
	  }
    }
	else{
	  //uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
	  //TO DO: Change timer period accroding to datarate and pktsize
	  m_timerSendBackoff.Cancel();
	  m_macaState = QUIET;
	  m_timerQuiet.Schedule(Seconds(2 * m_maxPropTime + m_tCTS));
	  //NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " RX xRTS to QUIET");
	}	
  }
}

void
UanMacMaca::RxCTS (Ptr<Packet> pkt ,const UanAddress& dst)
{
  if (m_macaState == WFCTS && dst == GetAddress()){
    m_timerWFCTS.Cancel();
	
	if ( m_sendQueue.size() > 0){
	    NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send DATA");
        Ptr<Packet> sendPkt = m_sendQueue.front();
        Ptr<Packet> sendPktCb = sendPkt -> Copy();
	    Ptr<Packet> sendPktCb2 = sendPktCb -> Copy();
        m_countBEB=m_minBEB;
	    if (Send (sendPkt)){
	        
              m_sendingData = true;
            //m_sendDataCallback (sendPktCb);
			m_sendQueue.pop();
           }

	   }
 	   m_macaState = IDLE;
	   BackoffNextSend(true);
 }
  else if (dst != GetAddress()){
    if(m_macaState == QUIET){
	  NS_ASSERT(m_timerQuiet.IsRunning());
	  double newtime = std::max(m_timerQuiet.GetDelayLeft().GetSeconds(),(2 * m_maxPropTime + m_tDATA));
	  m_timerQuiet.Cancel();
	  m_timerQuiet.Schedule(Seconds(newtime));
	  	 //NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds ()<< " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" hear xCTS reset quiet "<<newtime);
	}
	else if (m_macaState == IDLE || m_macaState == CONTEND || m_macaState == WFCTS){
	
	  m_timerSendBackoff.Cancel();
	  m_timerWFCTS.Cancel();
	  m_macaState = QUIET;
	  m_timerQuiet.Schedule(Seconds(2 * m_maxPropTime + m_tDATA));
	  //NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " RX xCTS to QUIET");
	}
	
  }
}

bool
UanMacMaca::SendCTS (const Address &dest, const double duration)
{
  NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send CTS to "<< UanAddress::ConvertFrom (dest));
  //uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();

  UanAddress src = UanAddress::ConvertFrom (GetAddress ());
  UanAddress udest = UanAddress::ConvertFrom (dest);

  UanHeaderCommon header;
  header.SetSrc (src);
  header.SetDest (udest);
  header.SetType (CTS);

  // CTS size
  uint8_t size = DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
  if (m_rtsSize > size)
    size = m_rtsSize - size;

  Ptr<Packet> packet = Create<Packet> (size);
  packet->AddHeader (header);

  //m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + packet->GetSize ()) * 8 / dataRate));

  return Send (packet);
  /*DynamicCast<UanMacWakeupMaca> (m_mac) -> SendBroadcastCTS(UanAddress::ConvertFrom(dest));*/
  //return true;
}

/*void
UanMacMaca::TxEnd ()
{
  if (m_sendingData)
    {
      
          m_sendingData = false;
          m_sendQueue.pop ();

          if (m_sendQueue.size () >= 1
              && !m_timerWaitToBackoff.IsRunning()
              && !m_timerBackoff.IsRunning ())
            {
				NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds ()  <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Shedule Next packet in Queue (no ACK). Queue "<< m_sendQueue.size());
              // Size
              uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
              uint32_t size = 10;//DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
              m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + m_maxPacketSize) * 8 / dataRate));
            }
        
    }
}
*/
void
UanMacMaca::AttachPhy (Ptr<UanPhy> phy)
{
  m_phy = phy;
  //DynamicCast<UanPhyGen> (m_phy) -> SetReceiveOkCallback (MakeCallback (&UanMacMaca::RxPacket, this));	
}
void
UanMacMaca::AttachMacWakeup(Ptr<UanMacWakeupMaca> mac){
  m_mac = mac;
  DynamicCast<UanMacWakeupMaca> (m_mac) ->SetForwardUpCb(MakeCallback (&UanMacMaca::RxPacket, this));
  //DynamicCast<UanMacWakeupMaca> (m_mac) ->SetRxRTSCb(MakeCallback (&UanMacMaca:: RxRTS, this));
  //DynamicCast<UanMacWakeupMaca> (m_mac) ->SetRxCTSCb(MakeCallback (&UanMacMaca:: RxCTS, this));
}

void
UanMacMaca::SetSendDataCallback (Callback<void, Ptr<Packet> > sendDataCallback)
{
  m_sendDataCallback = sendDataCallback;
}

int64_t
UanMacMaca::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_rand->SetStream (stream);
  return 1;
}

void
UanMacMaca::RxPacketGood (Ptr<Packet> pkt, double sinr, UanTxMode txMode)
{

}

void
UanMacMaca::RxPacketError (Ptr<Packet> pkt, double sinr)
{
  NS_LOG_DEBUG (" " << Simulator::Now () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Received packet in error with sinr " << sinr);
}


Address
UanMacMaca::GetBroadcast (void) const
{
  UanAddress broadcast (255);
  return broadcast;
}


void
UanMacMaca::OntimerSendBackoff(){
  if (!m_sendQueue.empty()){
    m_macaState = CONTEND;
    if (SendRTS()){
	  m_macaState = WFCTS;
	  m_timerWFCTS.Schedule(Seconds(2 * m_maxPropTime + m_tCTS));
    }
	else{
	  BackoffNextSend();
	}
  }
  
}

void
UanMacMaca::OntimerWFCTS(){
  m_countBEB = std::min(uint8_t(2*m_countBEB),m_maxBEB);
  m_macaState = IDLE;
  BackoffNextSend ();
}

void
UanMacMaca::OntimerQuiet(){
  NS_LOG_DEBUG (" " << Simulator::Now ().GetSeconds ()<<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << "end quiet");
  m_macaState = IDLE;
  BackoffNextSend ();
}

}