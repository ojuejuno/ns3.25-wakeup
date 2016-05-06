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

#include "uan-mac-maca-nw.h"
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
NS_LOG_COMPONENT_DEFINE ("UanMacMacaNW");
NS_OBJECT_ENSURE_REGISTERED (UanMacMacaNW);

UanMacMacaNW::UanMacMacaNW ()
  :m_cleared (false),
  m_maxBackoff (0.5),
  m_maxPropTime (0.3),
  m_maxPacketSize (64)
  {
  m_timerSendBackoff.SetFunction (&UanMacMacaNW::OntimerSendBackoff,this);
  m_timerQuiet.SetFunction (&UanMacMacaNW::OntimerQuiet,this);
  m_timerWFCTS.SetFunction (&UanMacMacaNW::OntimerWFCTS,this);
  
  m_rtsSize = 0;
  
  m_maxBulkSend = 0;
  m_bulkSend = 0;
  m_state = UanMacWakeup::IDLE;
  m_macaState = IDLE;
  m_rand= CreateObject<UniformRandomVariable>();
  
  m_phy = 0;
  m_tCTS = 0.1;
  m_tDATA = 0.2;
    
  m_minBEB=1;
  m_maxBEB=10;
  m_countBEB=1;
  }
  
UanMacMacaNW::~UanMacMacaNW ()
{
  m_timerSendBackoff.Cancel ();
  m_timerQuiet.Cancel ();
  m_timerWFCTS.Cancel ();
}

void
UanMacMacaNW::Clear ()
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
UanMacMacaNW::DoDispose ()
{
  Clear ();
  UanMac::DoDispose ();
}

TypeId
UanMacMacaNW::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanMacMacaNW")
    .SetParent<Object> ()
	.SetGroupName ("Uan")
    .AddConstructor<UanMacMacaNW> ()
  ;
  return tid;
}

Address
UanMacMacaNW::GetAddress (void)
{
  return this->m_address;
}

void
UanMacMacaNW::SetAddress (UanAddress addr)
{
  m_address = addr;
}

void
UanMacMacaNW::SetBackoffTime (double backoff)
{
  m_maxBackoff = backoff;
}
double
UanMacMacaNW::GetBackoffTime ()
{
  return m_maxBackoff;
}

void
UanMacMacaNW::SetRtsSize (uint32_t size)
{
  m_rtsSize = size;
}
uint32_t
UanMacMacaNW::GetRtsSize () const
{
  return m_rtsSize;
}


void
UanMacMacaNW::SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress& > cb)
{
  m_forUpCb = cb;
}


bool
UanMacMacaNW::Enqueue (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber){
   NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Queueing packet for " << UanAddress::ConvertFrom (dest));

  UanAddress src = UanAddress::ConvertFrom (GetAddress ());
  UanAddress udest = UanAddress::ConvertFrom (dest);

  UanHeaderCommon header;
  header.SetSrc (src);
  header.SetDest (udest);
  header.SetType (DATA);

  packet->AddHeader (header);

  m_sendQueue.push(packet);
  
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
UanMacMacaNW::BackoffNextSend ()
{
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" sendBackoff is "<< m_timerSendBackoff.IsRunning());
  NS_ASSERT(!m_timerSendBackoff.IsRunning());
  double backoffTime = (double)(m_rand->GetValue(0,m_countBEB))*(m_maxPropTime+m_tCTS);
  m_timerSendBackoff.Schedule(Seconds(backoffTime));
}

bool
UanMacMacaNW::SendRTS ()
{
    NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send RTS. Queue "<<m_sendQueue.size ());
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
  uint32_t size = 10;//DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
  if (m_rtsSize > size)
    size = m_rtsSize - size;

  Ptr<Packet> packet = Create<Packet> (size);
  packet->AddHeader (rtsHeader);



  return Send (packet);
}

bool
UanMacMacaNW::Send (Ptr<Packet> pkt)
{
  UanHeaderCommon header;
  NS_ASSERT (pkt->PeekHeader (header));
  //uint32_t protocolNumber=0;
 if(!m_phy->IsStateBusy()){
    m_phy->SendPacket (pkt, 0);
    return true;
  }
  else 
    return false;
}


void
UanMacMacaNW::RxPacket (Ptr<Packet> pkt,  double sinr, UanTxMode mode)
{
  UanHeaderCommon header;
  pkt->RemoveHeader (header);
  

  m_rxSrc = header.GetSrc();
  m_rxDest = header.GetDest();
  m_rxType = header.GetType();
  m_rxSize = header.GetSerializedSize();
  
  if (UanAddress::ConvertFrom (GetAddress ()) == m_rxDest){
    NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" Receiving "<< int(m_rxType) <<" packet from " << m_rxSrc);
  }
  else{
    NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" Receiving "<< int(m_rxType) <<" packet from " << m_rxSrc << " For " << m_rxDest);
  }
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
    if (m_macaState == WFDATA){
	
	  if (m_rxDest == GetAddress () || m_rxDest == UanAddress::GetBroadcast ())
        {
           m_macaState = IDLE;
		   m_timerWFCTS.Cancel();
		   m_forUpCb (pkt, m_rxSrc);
        }
    }
    break;
  }
}

void
UanMacMacaNW::RxRTS (Ptr<Packet> pkt)
{

  if(m_macaState == QUIET && (m_rxDest != GetAddress())){
      NS_ASSERT(m_timerQuiet.IsRunning());
	  double newtime = std::max(m_timerQuiet.GetDelayLeft().GetSeconds(),(2 * m_maxPropTime + m_tCTS));
	  m_timerQuiet.Cancel();
	  m_timerQuiet.Schedule(Seconds(newtime));
	  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds ()<< " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" hear xRTS reset quiet"<<newtime);
	}
  else if (m_macaState == IDLE || m_macaState == CONTEND){
    if (m_rxDest == GetAddress()){
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
	  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " RX xRTS to QUIET");
	}	
  }
}

void
UanMacMacaNW::RxCTS (Ptr<Packet> pkt)
{
  if (m_macaState == WFCTS && m_rxDest == GetAddress()){
    m_timerWFCTS.Cancel();
	
	if ( m_sendQueue.size() > 0){
	    NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send DATA");
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
	   BackoffNextSend();
 }
  else if (m_rxDest != GetAddress()){
    if(m_macaState == QUIET){
	  NS_ASSERT(m_timerQuiet.IsRunning());
	  double newtime = std::max(m_timerQuiet.GetDelayLeft().GetSeconds(),(2 * m_maxPropTime + m_tDATA));
	  m_timerQuiet.Cancel();
	  m_timerQuiet.Schedule(Seconds(newtime));
	  	  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds ()<< " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" hear xCTS reset quiet"<<newtime);
	}
	else if (m_macaState == IDLE || m_macaState == CONTEND || m_macaState == WFCTS){
	
	  m_timerSendBackoff.Cancel();
	  m_timerWFCTS.Cancel();
	  m_macaState = QUIET;
	  m_timerQuiet.Schedule(Seconds(2 * m_maxPropTime + m_tDATA));
	  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " RX xCTS to QUIET");
	}
	
  }
}

bool
UanMacMacaNW::SendCTS (const Address &dest, const double duration)
{
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send CTS");
  //uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();

  UanAddress src = UanAddress::ConvertFrom (GetAddress ());
  UanAddress udest = UanAddress::ConvertFrom (dest);

  UanHeaderCommon header;
  header.SetSrc (src);
  header.SetDest (udest);
  header.SetType (CTS);

  // CTS size
  uint32_t size = 10;// DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
  if (m_rtsSize > size)
    size = m_rtsSize - size;

  Ptr<Packet> packet = Create<Packet> (size);
  packet->AddHeader (header);

  //m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + packet->GetSize ()) * 8 / dataRate));

  return Send (packet);
}

/*void
UanMacMacaNW::TxEnd ()
{
  if (m_sendingData)
    {
      
          m_sendingData = false;
          m_sendQueue.pop ();

          if (m_sendQueue.size () >= 1
              && !m_timerWaitToBackoff.IsRunning()
              && !m_timerBackoff.IsRunning ())
            {
				NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds ()  <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Shedule Next packet in Queue (no ACK). Queue "<< m_sendQueue.size());
              // Size
              uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
              uint32_t size = 10;//DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
              m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + m_maxPacketSize) * 8 / dataRate));
            }
        
    }
}
*/
void
UanMacMacaNW::AttachPhy (Ptr<UanPhy> phy)
{
  m_phy = phy;
  DynamicCast<UanPhyGen> (m_phy) -> SetReceiveOkCallback (MakeCallback (&UanMacMacaNW::RxPacket, this));	
}

void
UanMacMacaNW::SetSendDataCallback (Callback<void, Ptr<Packet> > sendDataCallback)
{
  m_sendDataCallback = sendDataCallback;
}

int64_t
UanMacMacaNW::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_rand->SetStream (stream);
  return 1;
}

void
UanMacMacaNW::RxPacketGood (Ptr<Packet> pkt, double sinr, UanTxMode txMode)
{

}

void
UanMacMacaNW::RxPacketError (Ptr<Packet> pkt, double sinr)
{
  NS_LOG_DEBUG ("" << Simulator::Now () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Received packet in error with sinr " << sinr);
}


Address
UanMacMacaNW::GetBroadcast (void) const
{
  UanAddress broadcast (255);
  return broadcast;
}


void
UanMacMacaNW::OntimerSendBackoff(){
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
UanMacMacaNW::OntimerWFCTS(){
  m_countBEB = std::min(uint8_t(2*m_countBEB),m_maxBEB);
  m_macaState = IDLE;
  BackoffNextSend ();
}

void
UanMacMacaNW::OntimerQuiet(){
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds ()<<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << "end quiet");
  m_macaState = IDLE;
  BackoffNextSend ();
}

}