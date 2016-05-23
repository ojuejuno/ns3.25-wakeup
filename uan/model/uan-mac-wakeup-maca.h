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

#ifndef UAN_MAC_WAKEUP_MACA_H_
#define UAN_MAC_WAKEUP_MACA_H_

#include "uan-mac.h"
#include "uan-address.h"
#include "ns3/random-variable-stream.h"
#include "ns3/event-id.h"
#include "ns3/timer.h"
#include "ns3/uan-phy-gen.h"
#include "ns3/simulator.h"

#include <queue>

namespace ns3
{


class UanPhy;
class UanTxMode;

class UanMacWakeupMaca : public UanMac, public UanPhyListener
{
public:
  enum PhyState { BUSY, IDLE };

  typedef Callback<void> TxEndCallback;
  typedef Callback<void> ToneRxCallback;

  UanMacWakeupMaca ();
  virtual ~UanMacWakeupMaca ();
  static TypeId GetTypeId (void);


  //Inherited functions
  Address GetAddress (void);
  virtual void SetAddress (UanAddress addr);
  virtual bool Enqueue (Ptr<Packet> pkt, const Address &dest, uint16_t protocolNumber);
  virtual void SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress& > cb);
  void SetTxEndCallback (TxEndCallback cb);
  void SetToneRxCallback (ToneRxCallback cb);
  virtual void AttachPhy (Ptr<UanPhy> phy);
  void AttachWakeupPhy (Ptr<UanPhy> phy);
  //void AttachWakeupHEPhy (Ptr<UanPhy> phy);
  virtual Address GetBroadcast (void) const;
  virtual void Clear (void);

  bool Send ();
  void SendWU (UanAddress dst);
  bool SendBroadcastCTS(UanAddress dest);
  bool SendBroadcastCTD(UanAddress dest);
  
  //void SendWUHE ();
  //void SendBroadcastWU ();

  //void SetHighEnergyMode ();
  void SetToneMode ();

  void SetSendPhyStateChangeCb (Callback<void, PhyState> cb);
  void SetRxRTSCb(Callback<void,Ptr<Packet>,const UanAddress& > cb );
  void SetRxCTSCb(Callback<void,Ptr<Packet>,const UanAddress& > cb );
  
  
  void SetSleepMode(bool isSleep);
  void SetUseWakeup(bool useWakeup);

  uint32_t GetHeadersSize () const;

  //PhyListener
  /**
   * \brief Function called when Phy object begins receiving packet
   */
  virtual void NotifyRxStart (void);
  /**
   * \brief Function called when Phy object finishes receiving packet without error
   */
  virtual void NotifyRxEndOk (void);
  /**
   * \brief Function called when Phy object finishes receiving packet in error
   */
  virtual void NotifyRxEndError (void);
  /**
   * \brief Function called when Phy object begins sensing channel is busy
   */
  virtual void NotifyCcaStart (void);
  /**
   * \brief Function called when Phy object stops sensing channel is busy
   */
  virtual void NotifyCcaEnd (void);
  /**
   * \param duration Duration of transmission
   * \brief Function called when transmission starts from Phy object
   */
  virtual void NotifyTxStart (Time duration);

  int64_t AssignStreams (int64_t stream);
private:
  enum State { WU, DATA };

  Ptr<UniformRandomVariable> m_rand;

  Callback<void> m_sendDataCallback;

  UanAddress m_address;
  Ptr<UanPhyGen> m_phy;
  Ptr<UanPhy> m_wakeupPhy;
  //Ptr<UanPhy> m_wakeupPhyHE;
  Callback<void, Ptr<Packet>, const UanAddress& > m_forUpCb;
  Callback<void, PhyState> m_stateChangeCb;
  bool m_cleared;
  bool m_wuAlone;
  bool m_useWakeup;

  Ptr<Packet> m_pkt;
  UanAddress m_dest;

  Timer m_timerEndTx;
  Timer m_timerDelayTx;
  //Timer m_timerDelayTxWUHE;
  Timer m_timerTxFail;


  uint64_t m_timeDelayTx; //Miliseconds

  bool m_dataSent;
  //bool m_highEnergyMode;
  bool m_toneMode;
  State m_state;
  PhyState m_phyState;

  TxEndCallback m_txEnd;
  ToneRxCallback m_toneRxCallback;
  Callback<void,Ptr<Packet>,const UanAddress&> m_rxRTSCb;
  Callback<void,Ptr<Packet>,const UanAddress&> m_rxCTSCb;
  void On_timerEndTx (void);
  void On_timerDelayTx (void);
  //void On_timerDelayTxWUHE (void);
  void On_timerTxFail (void);



  /**
   * \brief Receive packet from lower layer (passed to PHY as callback)
   * \param pkt Packet being received
   * \param sinr SINR of received packet
   * \param txMode Mode of received packet
   */
  void RxPacketGood (Ptr<Packet> pkt, double sinr, UanTxMode txMode);
  //void RxPacketGoodData (Ptr<Packet> pkt, double sinr, UanTxMode txMode);
  void RxPacketGoodW (Ptr<Packet> pkt, double sinr, UanTxMode txMode);
  //void RxPacketGoodWHE (Ptr<Packet> pkt, double sinr, UanTxMode txMode);


  /**
   * \brief Packet received at lower layer in error
   * \param pkt Packet received in error
   * \param sinr SINR of received packet
   */
  void RxPacketError (Ptr<Packet> pkt, double sinr);
  void RxPacketErrorW (Ptr<Packet> pkt, double sinr);
  //void RxPacketErrorWHE (Ptr<Packet> pkt, double sinr);

protected:
  virtual void DoDispose ();
};

}


#endif /* UAN_MAC_WAKEUP_MACA_H_ */
