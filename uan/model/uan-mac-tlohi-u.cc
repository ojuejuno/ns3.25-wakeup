/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
#include "uan-header-common.h"
#include "uan-header-wakeup.h"

#include "uan-mac-tlohi-u.h"
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
#include "ns3/uan-module.h"
#include <iostream>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE ("UanMacTlohiU");
NS_OBJECT_ENSURE_REGISTERED (UanMacTlohiU);
  
  
UanMacTlohiU::UanMacTlohiU()
  :m_Ttlohi(0.024),
  m_Tmax(0.47),
  m_state(IDLE),
  m_blocking(false),
  m_CTC(1),
  m_sizeCTD(3)
{
  m_cleared = false;
  Clear();
  m_CRWindow = (m_Tmax + m_Ttlohi)* 1.0 ;
  m_timerCR.SetFunction (&UanMacTlohiU::on_timerCR,this);
  m_timerBkoffCR.SetFunction (&UanMacTlohiU::on_timerBkoffCR,this);
  m_timerMaxFrame.SetFunction (&UanMacTlohiU::on_timerMaxFrame,this);

}

UanMacTlohiU::~UanMacTlohiU(){
  m_timerBkoffCR.Cancel();
  m_timerCR.Cancel();
  m_timerMaxFrame.Cancel();
  }
  
void
UanMacTlohiU::Clear ()
{
  if (m_cleared)
    {
      return;
    }
  m_cleared = true;
  m_timerBkoffCR.Cancel();
  m_timerCR.Cancel();
  m_timerMaxFrame.Cancel();
  //NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" CLEAR");
  if (m_phy)
    {
      m_phy->Clear ();

      m_phy = 0;
    }
  while (!m_sendQueue.empty())
    {
        m_sendQueue.pop();
    }
}

void
UanMacTlohiU::DoDispose ()
{
  Clear ();
  UanMac::DoDispose ();
}

TypeId
UanMacTlohiU::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanMacTlohiU")
    .SetParent<Object> ()
	.SetGroupName ("Uan")
    .AddConstructor<UanMacTlohiU> ()
  ;
  return tid;
}
  
bool
UanMacTlohiU::Enqueue(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber){
  UanHeaderCommon header;
  header.SetDest(UanAddress::ConvertFrom (dest));
  packet->AddHeader(header);
  DynamicCast<UanMacWakeupTlohi> (m_mac)->SetSleepMode (false);
  if(m_sendQueue.size() < 10){
    m_sendQueue.push(packet);
    NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" packet queueing increase to "<<m_sendQueue.size());
  }
  if(m_blocking){
    if (!m_timerMaxFrame.IsRunning()){
	
      m_timerMaxFrame.Schedule(Seconds(m_CRWindow*30));
      m_state = ENDFRAME;//Wait for frame end
	  NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" quit frame from idle(enqueue) This should not happen too !!!!!!");
	  DynamicCast<UanMacWakeupTlohi> (m_mac)->SetSleepMode (true);
  }}
  if(m_state == IDLE && (!m_blocking)){
    NS_ASSERT(!(m_timerCR.IsRunning() || m_timerBkoffCR.IsRunning() || m_timerMaxFrame.IsRunning()) );
	TxCTD();
  }
  else if(!(m_timerCR.IsRunning() || m_timerBkoffCR.IsRunning()|| m_timerMaxFrame.IsRunning())&& (!m_blocking)){
    NS_ASSERT(m_state != BKOFF);
	NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" This should logically not appear!!!!!!!!!!!!!!!");
    TxCTD();
  }
  return true;
}

/*void
UanMacTlohiU::RxPacket(Ptr<Packet> pkt, const UanAddress& add){
  m_pkt = pkt;
  UanHeaderCommon header;
  pkt->RemoveHeader (header);
  UanAddress dest = header.GetDest();
  
  //if(dest != GetBroadcast()){
    RxData(dest);
  }
}*/

void
UanMacTlohiU::RxCTD(){
  if(!m_blocking){
    switch(m_state){
      case IDLE:
	    m_blocking = true;
		m_timerMaxFrame.Schedule(Seconds(m_CRWindow*30));
		m_state = ENDFRAME;//Wait for frame end
		NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" quit frame from idle");
	    break;
      case CONTEND:
        NS_ASSERT (m_timerCR.IsRunning());
		m_CTC++;
		NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" CTC++");
  	    break;
      case BKOFF:
        NS_ASSERT(m_timerBkoffCR.IsRunning());
        m_timerBkoffCR.Cancel();
		
        m_blocking = true;//Wait for frame end
		m_timerMaxFrame.Schedule(Seconds(m_CRWindow*30));
	    m_state = ENDFRAME;
		NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" quit frame from bkoff");
	    break;
	  case ENDFRAME:
        NS_LOG_DEBUG ("MAC " << GetAddress () << " RxCTD in strange state"); 
		break;
    }
  }
}

void
UanMacTlohiU::RxData(Ptr<Packet> pkt, const UanAddress& add){
  DynamicCast<UanMacWakeupTlohi> (m_mac)->SetSleepMode (false);
  m_pkt = pkt;
  UanHeaderCommon header;
  pkt->RemoveHeader (header);
  UanAddress dest = header.GetDest();
  
  if(dest == GetAddress()){
    m_forUpCb(m_pkt, dest);
    NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" RxData*****************************");
    return;
  }
  if(m_blocking){
    NS_ASSERT(m_state = ENDFRAME);
    m_blocking = false;
    m_timerMaxFrame.Cancel();	
	}
  m_timerCR.Cancel();
  m_timerBkoffCR.Cancel();
  if(m_sendQueue.size()){
	  TxCTD();
	  NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" restart contend after data");
	}
  else{
	m_state = IDLE;
	DynamicCast<UanMacWakeupTlohi> (m_mac)->SetSleepMode (true);
  }

}

void
UanMacTlohiU::TxCTD(){

  
  //Ptr<Packet> packetCTD = Create<Packet> ();
  //UanHeaderCommon headerCTD;
  //headerCTD.SetDest(UanAddress::ConvertFrom(GetBroadcast()));
  //packetCTD->AddHeader(headerCTD);
  
  //if(!m_phy->IsStateBusy()){
    DynamicCast<UanMacWakeupTlohi> (m_mac)->SetSleepMode (false);
    NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" CONTENDING");
    DynamicCast<UanMacWakeupTlohi> (m_mac) -> SendCTDTone();
	//m_phy->SendPacket (packetCTD, 0);
    m_timerCR.Schedule(Seconds(m_CRWindow));
    m_state = CONTEND;
    m_CTC = 1;
  //}
}

bool
UanMacTlohiU::TxData(){
  NS_ASSERT( !m_blocking );
  Ptr<Packet> packetData = m_sendQueue.front();
  UanHeaderCommon header ;
  packetData->PeekHeader (header);
  UanAddress dest =header.GetDest();
  
  if(!m_phy->IsStateBusy()){
    //m_phy->SendPacket(packetData->Copy(),0);
	m_mac->Enqueue(packetData->Copy(),dest,0);
	NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" SentData");
	 //m_sendDataCallback(packetData);
    m_sendQueue.pop();
	m_state = IDLE;
	DynamicCast<UanMacWakeupTlohi> (m_mac)->SetSleepMode (true);
	return true;
  }
  return false;
}

void
UanMacTlohiU::TxEnd(){
  if(!(m_sendQueue.size()) && m_state == IDLE){
	DynamicCast<UanMacWakeupTlohi> (m_mac)->SetSleepMode (true);
  }
}

void
UanMacTlohiU::on_timerCR(){
  if(m_CTC > 1){
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>() ;
    uint32_t backoffCR = (uint32_t)rand-> GetValue(0,double(m_CTC));
    m_timerBkoffCR.Schedule (Seconds((double)backoffCR*(m_CRWindow)));
    m_state = BKOFF ;
	NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" Backoff for "<< backoffCR <<"  CRs ");
  }
  else if(m_CTC == 1){
    if (!TxData()) TxCTD();
	
  }
}

void
UanMacTlohiU::on_timerBkoffCR(){
  NS_ASSERT( !m_blocking );
  TxCTD();
}
void
UanMacTlohiU::on_timerMaxFrame(){	
  m_blocking = false;
  DynamicCast<UanMacWakeupTlohi> (m_mac)->SetSleepMode (false);
  if(m_sendQueue.size()){
	TxCTD();
  }
  else{
	m_state = IDLE;
	DynamicCast<UanMacWakeupTlohi> (m_mac)->SetSleepMode (true);
  }
}
void
UanMacTlohiU::AttachPhy (Ptr<UanPhy> phy)
{
  m_phy = phy;
  //DynamicCast<UanPhyGen> (m_phy) -> SetReceiveOkCallback (MakeCallback (&UanMacTlohiU::RxPacket, this));	
}

void
UanMacTlohiU::AttachMacWakeup (Ptr<UanMac> mac)
{
  m_mac = mac;
  DynamicCast<UanMacWakeupTlohi> (m_mac) -> SetForwardUpCb (MakeCallback(&UanMacTlohiU::RxData,this));
  
  DynamicCast<UanMacWakeupTlohi> (m_mac) -> SetToneMode();
  
}

Address
UanMacTlohiU::GetAddress (void)
{
  return this->m_address;
}

void
UanMacTlohiU::SetAddress (UanAddress addr)
{
  m_address = addr;
}


void
UanMacTlohiU::SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress& > cb){
  m_forUpCb = cb;
}

void
UanMacTlohiU::SetSendDatacb (Callback<void, Ptr<Packet> > cb){
  m_sendDataCallback = cb;
}

Address
UanMacTlohiU::GetBroadcast (void) const
{
  UanAddress broadcast (255);
  return broadcast;
}

int64_t
UanMacTlohiU::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  //m_rand->SetStream (stream);
  return 1;
}

}