/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */


#ifndef UAN_MAC_TLOHI_U_H_
#define UAN_MAC_TLOHI_U_H_

#include "uan-mac.h"
#include "uan-address.h"
#include "ns3/random-variable-stream.h"
#include "ns3/event-id.h"
#include "ns3/timer.h"
#include "ns3/uan-phy.h"
#include "uan-mac-wakeup.h"

#include <queue>

namespace ns3
{
class UanPhy;
class UanTxMode;

class UanMacTlohiU : public UanMac
{
public:
  enum State{IDLE, CONTEND, BKOFF, ENDFRAME};
  double m_Ttlohi;
  double m_Tmax;
  State m_state;
  bool m_blocking;
  uint32_t m_CTC;
  uint32_t m_sizeCTD;
  
  double m_CRWindow;
  Ptr<Packet> m_pkt;
  std::queue<Ptr<Packet> > m_sendQueue;
  Callback<void, Ptr<Packet>, const UanAddress& > m_forUpCb;
  Callback<void, Ptr<Packet> > m_sendDataCallback;
  Address m_address;
  Ptr<UanPhy> m_phy;
  Ptr<UanMac> m_mac;
  bool m_cleared;
  
  Timer m_timerBkoffCR;
  Timer m_timerCR;
  Timer m_timerMaxFrame;
  
  UanMacTlohiU ();
  virtual ~UanMacTlohiU ();
  static TypeId GetTypeId (void);
  
  //Inheritted functions
  Address GetAddress (void);
  virtual void SetAddress (UanAddress addr);
  virtual bool Enqueue (Ptr<Packet> pkt, const Address &dest, uint16_t protocolNumber);
  virtual void SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress& > cb);
  virtual void AttachPhy (Ptr<UanPhy> phy);
  void AttachMacWakeup (Ptr<UanMac> mac);
  virtual Address GetBroadcast (void) const;
  virtual void Clear (void);
  int64_t AssignStreams(int64_t stream);
  
  //void RxPacket (Ptr<Packet> pkt, double sinr, UanTxMode mode);
  void SetSendDatacb (Callback<void, Ptr<Packet> > cb);
  
  void TxEnd();
  void RxData(Ptr<Packet> pkt, const UanAddress& add);
  void RxCTD();
private:


  void TxCTD();
  bool TxData();
  
  void on_timerCR();
  void on_timerBkoffCR();
  void on_timerMaxFrame();
  
  
  
protected:
  virtual void DoDispose ();
};
}

#endif /* UAN_MAC_TLOHI_U_H_ */
