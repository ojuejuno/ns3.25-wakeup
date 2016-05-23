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

#include "uan-mac-fama-nw.h"
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
NS_LOG_COMPONENT_DEFINE ("UanMacFamaNW");
NS_OBJECT_ENSURE_REGISTERED (UanMacFamaNW);

UanMacFamaNW::UanMacFamaNW ()
  : UanMac (),
    m_cleared (false),
    m_maxBackoff (0.5),
    m_maxPropTime (0.47),
    m_maxPacketSize (26)
{
  m_timerCONTEND.SetFunction (&UanMacFamaNW::On_timerCONTEND, this);
  m_timerWaitToBackoff.SetFunction (&UanMacFamaNW::On_timerWaitToBackoff, this);
  m_timerWfCTS.SetFunction (&UanMacFamaNW::On_timerWfCTS, this);
  m_timerWfDATA.SetFunction (&UanMacFamaNW::On_timerWfDATA, this);
  m_timerBackoff.SetFunction (&UanMacFamaNW::On_timerBackoff, this);
  m_timerWfACK.SetFunction (&UanMacFamaNW::On_timerWfACK, this);
  
  m_rtsSize = 0;
  m_useAck = true;
  m_sendingData = false;

  m_maxBulkSend = 1;
  m_bulkSend = 0;
  m_state = UanMacWakeup::IDLE;
  m_rand= CreateObject<UniformRandomVariable>();
  
  m_phy=0;
  m_macState = IDLE;
}

UanMacFamaNW::~UanMacFamaNW ()
{
  StopTimer();
}

void
UanMacFamaNW::Clear ()
{
  if (m_cleared)
    {
      return;
    }
  m_cleared = true;
  StopTimer();
  if (m_phy)
    {
      m_phy->Clear ();

      m_phy = 0;
    }
}

void
UanMacFamaNW::DoDispose ()
{
  Clear ();
  UanMac::DoDispose ();
}

TypeId
UanMacFamaNW::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanMacFamaNW")
    .SetParent<UanMac> ()
	.SetGroupName ("Uan")
    .AddConstructor<UanMacFamaNW> ()
  ;
  return tid;
}

Address
UanMacFamaNW::GetAddress (void)
{
  return this->m_address;
}

void
UanMacFamaNW::SetAddress (UanAddress addr)
{
  m_address = addr;
}

void
UanMacFamaNW::SetBackoffTime (double backoff)
{
  m_maxBackoff = backoff;
}
double
UanMacFamaNW::GetBackoffTime ()
{
  return m_maxBackoff;
}

void
UanMacFamaNW::SetRtsSize (uint32_t size)
{
  m_rtsSize = size;
}
uint32_t
UanMacFamaNW::GetRtsSize () const
{
  return m_rtsSize;
}

void
UanMacFamaNW::SetUseAck (bool ack)
{
  m_useAck = ack;
}
bool
UanMacFamaNW::GetUseAck () const
{
  return m_useAck;
}

void
UanMacFamaNW::SetBulkSend (uint8_t bulkSend)
{
  m_bulkSend = m_maxBulkSend = (bulkSend > 0) ? bulkSend - 1 : 0;
}

bool
UanMacFamaNW::Enqueue (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
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
      uint32_t size = 10;// DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
      if (m_rtsSize > size)
        size = m_rtsSize - size;

      Ptr<Packet> packetRts = Create<Packet> (size);
      packetRts->AddHeader (header);

      m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + packetRts->GetSize ()) * 8 / dataRate));
	  
    }

  return true;
}

bool
UanMacFamaNW::SendRTS ()
{
    NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send RTS. Queue "<<m_sendQueue.size ());
  if(m_sendQueue.size() < 1)
    return false;
  //NS_ASSERT(m_sendQueue.size () >= 1);

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
  uint32_t size = 10;//DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
  if (m_rtsSize > size)
    size = m_rtsSize - size;

  Ptr<Packet> packet = Create<Packet> (size);
  packet->AddHeader (header);

  //m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + packet->GetSize ()) * 8 / dataRate));

  bool success = Send(packet);
  if(success){
    m_macState = WFCTS;
	/*uint32_t size = 10;// DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
    if (m_rtsSize > size)
      size = m_rtsSize - size;
    Ptr<Packet> packetRts = Create<Packet> (size);
    packetRts->AddHeader (header);*/
	m_timerWfCTS.Schedule(Seconds(m_maxPropTime * 2 + (size + packet->GetSize ()) * 8 / dataRate));
  }
  return success;
}

bool
UanMacFamaNW::SendCTS (const Address &dest, const double duration)
{
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Send CTS to" << UanAddress::ConvertFrom (dest));
  uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();

  UanAddress src = UanAddress::ConvertFrom (GetAddress ());
  UanAddress udest = UanAddress::ConvertFrom (dest);

  UanHeaderCommon header;
  header.SetSrc (src);
  header.SetDest (udest);
  header.SetType (CTS);

  uint32_t size = 10;// DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
  if (m_rtsSize > size)
  size = m_rtsSize - size;

  Ptr<Packet> packet = Create<Packet> (m_maxPacketSize);
  packet->AddHeader (header);

  //m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + packet->GetSize ()) * 8 / dataRate));

  bool success = Send(packet);
  if(success) {
  m_macState = WFDATA;
  m_timerWfDATA.Schedule(Seconds(m_maxPropTime * 2 + (size + packet->GetSize ()) * 8 / dataRate));
  }
  return success;
}

/*bool
UanMacFamaNW::SendWakeupBroadcast (Ptr<Packet> pkt)
{
  UanHeaderCommon header;
  NS_ASSERT (pkt->PeekHeader (header));

  return m_mac->Enqueue (pkt, UanAddress::GetBroadcast(), 0);
}*/

bool
UanMacFamaNW::Send (Ptr<Packet> pkt)
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
UanMacFamaNW::SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress& > cb)
{
  m_forUpCb = cb;
}

void
UanMacFamaNW::PhyStateCb (UanMacWakeup::PhyState phyState)
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
UanMacFamaNW::TxEnd ()
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
             //m_macState = SDATA;
              if (Send (sendPkt->Copy()))
                {
                  m_sendingData = true;
				  m_macState = SDATA;
                  m_bulkSend--;
                  m_sendDataCallback (sendPkt);
                }
            }
          else if (m_sendQueue.size () >= 1
              && !m_timerWaitToBackoff.IsRunning()
              && !m_timerCONTEND.IsRunning ())
            {
			  NS_LOG_DEBUG (" Schedule Next packet in Queue. ");
			  StartContend();
            }
        }
		else{
		  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Sent DATA, WFACK");
		  uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
		  m_macState = WFACK;
		  m_timerWfACK.Schedule(Seconds(m_maxPropTime * 2 + (m_maxPacketSize+20 + 5) * 8 / dataRate));
		}
    }
}


void
UanMacFamaNW::AttachPhy (Ptr<UanPhy> phy)
{
  m_phy = phy;
  DynamicCast<UanPhyGen> (m_phy) -> SetReceiveOkCallback (MakeCallback (&UanMacFamaNW::RxPacket, this));	
}
void
UanMacFamaNW::AttachMacWakeup (Ptr<UanMacWakeup> mac)
{
  m_mac = mac;
  //m_mac->SetForwardUpCb (MakeCallback (&UanMacFamaNW::RxPacket, this));
}

bool
UanMacFamaNW::SendAck (const UanAddress& dest)
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
UanMacFamaNW::RxPacket (Ptr<Packet> pkt,  double sinr, UanTxMode mode)
{
  UanHeaderCommon header;
  pkt->RemoveHeader (header);

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
		
		if (m_useAck) SendAck (m_rxSrc);
        NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" RX DATA from " << header.GetSrc ()<<"********************************");
        m_macState = IDLE;
		m_forUpCb (pkt, m_rxSrc);
      }
	else{
	   StopTimer();
	     if(m_useAck){
	       m_timerBackoff.Schedule(Seconds(2*m_maxPropTime+0.1));
		 }
		 else{
		   m_timerBackoff.Schedule(Seconds(m_maxPropTime+0.1));
		 }
		 NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" rx xDATA " );
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
				//if(m_sendDataCallback.IsNull()){
                //  m_sendDataCallback (sendPkt);
				//}
				//		else{
		  
				uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
			    m_macState = WFACK;
		        m_timerWfACK.Schedule(Seconds(m_maxPropTime * 2 + (m_maxPacketSize+20 + 5) * 8 / dataRate));

              }
          }
        else if (m_sendQueue.size () >= 1
            && !m_timerWaitToBackoff.IsRunning()
            && !m_timerCONTEND.IsRunning ())
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
UanMacFamaNW::StartContend(){
    m_macState = CONTEND;
	NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds ()  <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Schedule Next packet. Queue size"<< m_sendQueue.size());
    // Size
    uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
    uint32_t size = 10;//DynamicCast<UanMacWakeup> (m_mac)->GetHeadersSize ();
    m_timerWaitToBackoff.Schedule (Seconds(m_maxPropTime * 2 + (size + m_maxPacketSize) * 8 / dataRate));
}

void
UanMacFamaNW::StopTimer(){
  m_timerBackoff.Cancel();
  m_timerCONTEND.Cancel();
  m_timerWaitToBackoff.Cancel();
  m_timerWfACK.Cancel();
  m_timerWfCTS.Cancel();
  m_timerWfDATA.Cancel();
}
void
UanMacFamaNW::RxPacketGood (Ptr<Packet> pkt, double sinr, UanTxMode txMode)
{


}

void
UanMacFamaNW::RxRTS (Ptr<Packet> pkt)
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
UanMacFamaNW::RxCTS (Ptr<Packet> pkt)
{
  if (m_rxDest == GetAddress() && m_sendQueue.size() > 0)
    {
	  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" RX CTS ");
	  //NS_ASSERT (m_sendQueue.size () > 0);
        Ptr<Packet> sendPkt = m_sendQueue.front();
		Ptr<Packet> sendPktCb = sendPkt -> Copy();
		Ptr<Packet> sendPktCb2 = sendPktCb -> Copy();
		StopTimer();
		m_macState = IDLE;
        if (Send (sendPkt->Copy()))
          {
		    Ptr<Packet> sendPktCb = Create<Packet> (m_maxPacketSize);
			
            //m_sendDataCallback (sendPktCb);
			NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Sent DATA");
			//m_sendQueue.pop ();
            m_sendingData = true;
			m_macState = SDATA;
			if(m_useAck){
		      NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () <<" MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Sent DATA, WFACK");
		      uint32_t dataRate = m_phy->GetMode(0).GetDataRateBps();
		      m_macState = WFACK;
		      m_timerWfACK.Schedule(Seconds(m_maxPropTime * 2 + (m_maxPacketSize+20 + 5) * 8 / dataRate));
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
UanMacFamaNW::RxPacketError (Ptr<Packet> pkt, double sinr)
{
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Received packet in error with sinr " << sinr);
}

Address
UanMacFamaNW::GetBroadcast (void) const
{
  UanAddress broadcast (255);
  return broadcast;
}

void
UanMacFamaNW::On_timerWaitToBackoff (void)
{
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Schedule RTS timer");
  m_timerCONTEND.Schedule (Seconds (m_rand->GetValue(0.1, m_maxBackoff)));
}

void
UanMacFamaNW::On_timerCONTEND (void)
{
  SendRTS();
}

void 
UanMacFamaNW::On_timerWfCTS(void){
  double bkoffNum=m_rand->GetValue(0.1,m_maxBackoff);
  m_timerBackoff.Schedule(Seconds(10*bkoffNum*(2*m_maxPropTime+0.1)));
  m_macState = BACKOFF;
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " not receive CTS");
}
void 
UanMacFamaNW::On_timerWfDATA(void){
  m_macState = IDLE;
}
void 
UanMacFamaNW::On_timerBackoff(void){
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " Exit Backoff");
  m_macState = IDLE;
  if(m_sendQueue.size()){
    StopTimer();
	StartContend();
  }
}

void
UanMacFamaNW::On_timerWfACK(void){
  double bkoffNum=m_rand->GetValue(0.1,m_maxBackoff);
  m_timerBackoff.Schedule(Seconds(10*bkoffNum*(2*m_maxPropTime+0.1)));
  m_macState = BACKOFF;
  m_sendingData = false;
  NS_LOG_DEBUG ("" << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) << " not receive ACK");
}
void
UanMacFamaNW::SetSendDataCallback (Callback<void, Ptr<Packet> > sendDataCallback)
{
  m_sendDataCallback = sendDataCallback;
}

int64_t
UanMacFamaNW::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_rand->SetStream (stream);
  return 1;
}



}







