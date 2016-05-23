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

#ifndef UAN_MAC_FAMA_NW_H_
#define UAN_MAC_FAMA_NW_H_

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

class UanMacFamaNW : public UanMac
{
public:
  enum TYPE {RTS, CTS, DATA, ACK};
  enum MACSTATE {IDLE, CONTEND, WFCTS, SDATA, WFDATA, WFACK, BACKOFF, RX};

  UanMacFamaNW ();
  virtual ~UanMacFamaNW ();
  static TypeId GetTypeId (void);


  //Inheritted functions
  Address GetAddress (void);
  virtual void SetAddress (UanAddress addr);
  virtual bool Enqueue (Ptr<Packet> pkt, const Address &dest, uint16_t protocolNumber);
  virtual void SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress& > cb);
  virtual void AttachPhy (Ptr<UanPhy> phy);
  void AttachMacWakeup (Ptr<UanMacWakeup> mac);
  virtual Address GetBroadcast (void) const;
  virtual void Clear (void);

  void TxEnd ();
  void StartContend();
  void StopTimer();
  void SetUseAck (bool ack);
  bool GetUseAck () const;

  void RxPacket (Ptr<Packet> pkt, double sinr, UanTxMode mode);
  bool Send ();

  void SetSendDataCallback (Callback<void, Ptr<Packet> > sendDataCallback);

  void PhyStateCb (UanMacWakeup::PhyState phyState);

  void SetBackoffTime (double backoff);
  double GetBackoffTime ();
  void SetRtsSize (uint32_t size);
  uint32_t GetRtsSize () const;

  void SetBulkSend (uint8_t bulkSend);
  int64_t AssignStreams(int64_t stream);
private:
  Ptr<UniformRandomVariable> m_rand;

  std::queue<Ptr<Packet> > m_sendQueue;

  Callback<void, Ptr<Packet> > m_sendDataCallback;

  UanAddress m_address;
  Ptr<UanMac> m_mac;
  Ptr<UanPhy> m_phy;
  Callback<void, Ptr<Packet>, const UanAddress& > m_forUpCb;
  bool m_cleared;


  Ptr<Packet> m_pkt;
  UanAddress m_dest;

  Timer m_timerCONTEND;
  Timer m_timerWaitToBackoff;
  Timer m_timerWfCTS;
  Timer m_timerWfDATA;
  Timer m_timerBackoff;
  Timer m_timerWfACK;

  UanMacWakeup::PhyState m_state;
  MACSTATE m_macState;

  double m_maxBackoff;
  double m_maxPropTime;
  uint32_t m_maxPacketSize;
  uint32_t m_rtsSize;
  bool m_useAck;
  bool m_sendingData;

  // Working packet
  UanAddress m_rxSrc;
  UanAddress m_rxDest;
  uint8_t m_rxType;
  uint32_t m_rxSize;

  uint8_t m_maxBulkSend;
  uint8_t m_bulkSend;

  void On_timerCONTEND (void);
  void On_timerWaitToBackoff (void);
  void On_timerWfCTS(void);
  void On_timerWfDATA(void);
  void On_timerBackoff(void);
  void On_timerWfACK(void);

  bool SendRTS ();
  bool SendCTS (const Address &dest, const double duration);
  bool Send (Ptr<Packet> pkt);
  bool SendAck (const UanAddress& dest);
  void RxRTS (Ptr<Packet> pkt);
  void RxCTS (Ptr<Packet> pkt);
  //bool SendWakeupBroadcast (Ptr<Packet> pkt);

  /**
   * \brief Receive packet from lower layer (passed to PHY as callback)
   * \param pkt Packet being received
   * \param sinr SINR of received packet
   * \param txMode Mode of received packet
   */
  void RxPacketGood (Ptr<Packet> pkt, double sinr, UanTxMode txMode);

  /**
   * \brief Packet received at lower layer in error
   * \param pkt Packet received in error
   * \param sinr SINR of received packet
   */
  void RxPacketError (Ptr<Packet> pkt, double sinr);
protected:
  virtual void DoDispose ();
};

}


#endif /* UAN_MAC_FAMA_NW_H_ */
