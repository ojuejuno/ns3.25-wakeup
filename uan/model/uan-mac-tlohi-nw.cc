/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
#include "uan-header-common.h"
#include "uan-header-wakeup.h"

#include "uan-mac-tlohi-nw.h"
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
NS_LOG_COMPONENT_DEFINE ("UanMacTlohiNW");
NS_OBJECT_ENSURE_REGISTERED (UanMacTlohiNW);
  
  
UanMacTlohiNW::UanMacTlohiNW()
  :m_Ttlohi(0.01),
  m_Tmax(0.33),
  m_state(IDLE),
  m_blocking(false),
  m_CTC(1),
  m_sizeCTD(3)
{
  Clear();
  m_CRWindow = (m_Tmax + m_Ttlohi)* 1.0 ;
  m_timerCR.SetFunction (&UanMacTlohiNW::on_timerCR,this);
  m_timerBkoffCR.SetFunction (&UanMacTlohiNW::on_timerBkoffCR,this);

}

UanMacTlohiNW::~UanMacTlohiNW(){
  m_timerBkoffCR.Cancel();
  m_timerCR.Cancel();
  
}
  
void
UanMacTlohiNW::Clear ()
{
  if (m_cleared)
    {
      return;
    }
  m_cleared = true;
  m_timerBkoffCR.Cancel();
  m_timerCR.Cancel();
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
UanMacTlohiNW::DoDispose ()
{
  Clear ();
  UanMac::DoDispose ();
}

TypeId
UanMacTlohiNW::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanMacTlohiNW")
    .SetParent<Object> ()
	.SetGroupName ("Uan")
    .AddConstructor<UanMacTlohiNW> ()
  ;
  return tid;
}
  
bool
UanMacTlohiNW::Enqueue(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber){
  UanHeaderWakeup header;
  header.SetDest(UanAddress::ConvertFrom (dest));
  packet->AddHeader(header);
  if(m_sendQueue.size() < 10){
    m_sendQueue.push(packet);
    NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" packet queueing increase to "<<m_sendQueue.size());
  }
  if(m_blocking == true)
    m_state = CONTEND;//Wait for frame end
  else if(m_state == IDLE){
    NS_ASSERT(!(m_timerCR.IsRunning() || m_timerBkoffCR.IsRunning()) );
	TxCTD();
  }
  else if(!(m_timerCR.IsRunning() || m_timerBkoffCR.IsRunning())){
    TxCTD();
  }
  return true;
}
  
void
UanMacTlohiNW::RxPacket(Ptr<Packet> pkt,  double sinr, UanTxMode mode){
  m_pkt = pkt;
  UanHeaderWakeup header;
  pkt->RemoveHeader (header);
  UanAddress dest = header.GetDest();
  
  if(dest == GetBroadcast())
    RxCTD();
  else
    RxData(dest);
}

void
UanMacTlohiNW::RxCTD(){
  if(!m_blocking){
    switch(m_state){
      case IDLE:
	    m_blocking = true;//Wait for frame end
	    break;
      case CONTEND:
        if (m_timerCR.IsRunning())
		  m_CTC++;
  	    break;
      case BKOFF:
        NS_ASSERT(m_timerBkoffCR.IsRunning());
        m_blocking = true;//Wait for frame end
        m_timerBkoffCR.Cancel();
	    m_state = CONTEND;
	    break;
      default:
        NS_LOG_DEBUG ("MAC " << GetAddress () << " RxCTD in strange state");
    }
  }
}

void
UanMacTlohiNW::RxData(UanAddress dest){
  
  if(dest == GetAddress()){
    m_forUpCb(m_pkt, dest);
    NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" RxData");
  }
  if(m_blocking){
    m_blocking = false;	
	if(m_state != IDLE){
	  TxCTD();
	}
  }
  
}

void
UanMacTlohiNW::TxCTD(){

  
  Ptr<Packet> packetCTD = Create<Packet> (m_sizeCTD);
  UanHeaderWakeup headerCTD;
  headerCTD.SetDest(UanAddress::ConvertFrom(GetAddress()));
  packetCTD->AddHeader(headerCTD);
  
  if(!m_phy->IsStateBusy()){
    NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" CONTENDING");
    m_phy->SendPacket (packetCTD, 0);
    m_timerCR.Schedule(Seconds(m_CRWindow));
    m_state = CONTEND;
    m_CTC = 1;
  }
}

bool
UanMacTlohiNW::TxData(){
  Ptr<Packet> packetData = m_sendQueue.front();
  if(!m_phy->IsStateBusy()){
    m_phy->SendPacket(packetData->Copy(),0);
	NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" SentData");
	//m_sendDataCallback(packetData);
    m_sendQueue.pop();
	return true;
  }
  return false;
}

void
UanMacTlohiNW::on_timerCR(){
  if(m_CTC > 1){
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>() ;
    uint32_t backoffCR = (uint32_t)rand-> GetValue(0,double(m_CTC));
    m_timerBkoffCR.Schedule (Seconds((double)backoffCR*(m_CRWindow)));
    m_state = BKOFF ;
	NS_LOG_DEBUG(" " << Simulator::Now ().GetSeconds () << " MAC " << UanAddress::ConvertFrom (GetAddress ()) <<" Backoff for "<< backoffCR <<"  CRs ");
  }
  else if(m_CTC == 1){
    TxData();
	
  }
}

void
UanMacTlohiNW::on_timerBkoffCR(){
  NS_ASSERT( !m_blocking );
  TxCTD();
}

void
UanMacTlohiNW::AttachPhy (Ptr<UanPhy> phy)
{
  m_phy = phy;
  DynamicCast<UanPhyGen> (m_phy) -> SetReceiveOkCallback (MakeCallback (&UanMacTlohiNW::RxPacket, this));	
}

Address
UanMacTlohiNW::GetAddress (void)
{
  return this->m_address;
}

void
UanMacTlohiNW::SetAddress (UanAddress addr)
{
  m_address = addr;
}


void
UanMacTlohiNW::SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress& > cb){
  m_forUpCb = cb;
}

void
UanMacTlohiNW::SetSendDatacb (Callback<void, Ptr<Packet> > cb){
  m_sendDataCallback = cb;
}

Address
UanMacTlohiNW::GetBroadcast (void) const
{
  UanAddress broadcast (255);
  return broadcast;
}

int64_t
UanMacTlohiNW::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  //m_rand->SetStream (stream);
  return 1;
}

}