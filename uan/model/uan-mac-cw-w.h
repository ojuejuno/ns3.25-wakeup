/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
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
 * Author: Leonard Tracy <lentracy@gmail.com>
 */

#ifndef UAN_MAC_CW_W_H
#define UAN_MAC_CW_W_H

#include "ns3/uan-mac.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/uan-phy.h"
#include "ns3/uan-tx-mode.h"
#include "ns3/uan-address.h"
#include "ns3/random-variable-stream.h"
#include "uan-mac-wakeup.h"

namespace ns3 {

/**
 * \ingroup uan
 *
 * CW-MAC protocol, similar in idea to the 802.11 DCF with
 * constant backoff window
 *
 * For more information on this MAC protocol, see:
 * Parrish, N.; Tracy, L.; Roy, S.; Arabshahi, P.; Fox, W.,
 * "System Design Considerations for Undersea Networks: Link and
 * Multiple Access Protocols," Selected Areas in Communications,
 * IEEE Journal on , vol.26, no.9, pp.1720-1730, December 2008
 */
class UanMacCwW : public UanMac
                 //,public UanPhyListener
{
public:
  /** Default constructor */
  UanMacCwW ();
  /** Dummy destructor, DoDispose. */
  virtual ~UanMacCwW ();
  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);

  /**
   * Set the contention window size.
   *   
   * \param cw Contention window size.
   */
  virtual void SetCw (uint32_t cw);
  /**
   * Set the slot time duration.
   *
   * \param duration Slot time duration.
   */
  virtual void SetSlotTime (Time duration);
  /**
   * Get the contention window size.
   *
   * \return Contention window size.
   */
  virtual uint32_t GetCw (void);
  /**
   * Get the slot time duration.
   *
   * \return Slot time duration.
   */
  virtual Time GetSlotTime (void);

  // Inherited methods from UanMac
  virtual Address GetAddress ();
  virtual void SetAddress (UanAddress addr);
  virtual bool Enqueue (Ptr<Packet> pkt, const Address &dest, uint16_t protocolNumber);
  virtual void SetForwardUpCb (Callback<void, Ptr<Packet>, const UanAddress&> cb);
  virtual void AttachPhy (Ptr<UanPhy> phy);
  virtual Address GetBroadcast (void) const;
  virtual void Clear (void);
  int64_t AssignStreams (int64_t stream);

  void SetSendDataCallback (Callback<void, Ptr<Packet> > sendDataCallback);
  void PhyStateCb (UanMacWakeup::PhyState phyState);

  void AttachMacWakeup (Ptr<UanMacWakeup> mac);
    /** End TX state. */
  void EndTx (void);
  /**
   *  TracedCallback signature for enqueue/dequeue of a packet.
   *
   * \param [in] packet The Packet being received.
   * \param [in] proto The protocol number.
   */
  typedef void (* QueueTracedCallback)
    (Ptr<const Packet> packet, uint16_t proto);

private:
  /** Enum defining possible Phy states. */
  typedef enum {
    IDLE,     //!< Idle state.
    CCABUSY,  //!< Channel busy.
    RUNNING,  //!< Delay timer running.
    TX        //!< Transmitting.
  } State;

  /** Forwarding up callback. */
  Callback <void, Ptr<Packet>, const UanAddress& > m_forwardUpCb;
  Callback<void, Ptr<Packet> > m_sendDataCallback;
  /** The MAC address. */
  UanAddress m_address;
  /** PHY layer attached to this MAC. */
  Ptr<UanPhy> m_phy;
  /** A packet destined for this MAC was received. */
  TracedCallback<Ptr<const Packet>, UanTxMode > m_rxLogger;
  /** A packet arrived at the MAC for transmission. */
  TracedCallback<Ptr<const Packet>, uint16_t  > m_enqueueLogger;
  /** A packet was passed down to the PHY from the MAC. */
  TracedCallback<Ptr<const Packet>, uint16_t  > m_dequeueLogger;

  // Mac parameters
  uint32_t m_cw;        //!< Contention window size.
  Time m_slotTime;      //!< Slot time duration.
  
  Ptr<UanMac> m_mac;
  
  // State variables
  /** Time to send next packet. */
  Time m_sendTime;
  /** Remaining delay until next send. */
  Time m_savedDelayS;
  /** Next packet to send. */
  Ptr<Packet> m_pktTx;
  /** Next packet protocol number (usage varies by MAC). */
  uint16_t m_pktTxProt;
  /** Scheduled SendPacket event. */
  EventId m_sendEvent;
  /** Scheduled EndTx event. */
  EventId m_txEndEvent;
  /** Current state. */
  State m_state;

  UanMacWakeup::PhyState m_wakeupState;
  /** Flag when we've been cleared */
  bool m_cleared;

  /** Provides uniform random variable for contention window. */
  Ptr<UniformRandomVariable> m_rv;

  /**
   * Receive packet from lower layer (passed to PHY as callback).
   *
   * \param packet Packet being received.
   * \param sinr SINR of received packet.
   * \param mode Mode of received packet.
   */
  void PhyRxPacketGood (Ptr<Packet> packet, const UanAddress& add);
  /**
   * Packet received at lower layer in error.
   *
   * \param packet Packet received in error.
   * \param sinr SINR of received packet.
   */
  void PhyRxPacketError (Ptr<Packet> packet, double sinr);
  /** Cancel SendEvent and save remaining delay. */
  void SaveTimer (void);
  /** Schedule SendPacket after delay. */
  void StartTimer (void);
  /** Send packet on PHY. */
  void SendPacket (void);


protected:
  virtual void DoDispose ();

};  // class UanMacCwW

} // namespace ns3

#endif /* UAN_MAC_CW_H */
