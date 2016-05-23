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

#ifndef UAN_HEADER_WAKEUP_H_
#define UAN_HEADER_WAKEUP_H_

#include "ns3/header.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/tag.h"
#include "uan-address.h"

namespace ns3 {

class UanHeaderWakeup : public Header
{
public:
  UanHeaderWakeup ();

  UanHeaderWakeup (const uint8_t addr);
  virtual ~UanHeaderWakeup ();

  static TypeId GetTypeId (void);

  /**
   * \param src Address of packet source node
   */
  void SetDest (UanAddress dest);

  UanAddress GetDest (void) const;




  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;
private:
  UanAddress m_dest;
};

class UanHeaderDuration : public Header
{
public:
  UanHeaderDuration ();

  UanHeaderDuration (const float time);
  virtual ~UanHeaderDuration ();

  static TypeId GetTypeId (void);

  /**
   * \param src Address of packet source node
   */
  void SetTxDuration (float time);

  float GetTxDuration (void) const;




  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;
private:
  float m_time;
};

class UanData : public Header
{
public:
  UanData ();

  UanData (uint32_t dataSize);
  virtual ~UanData ();

  static TypeId GetTypeId (void);


  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;
private:
  uint32_t m_dataSize;
};

class UanIdPacket : public Header
{
public:
  UanIdPacket ();

  UanIdPacket (uint8_t idPacket);
  virtual ~UanIdPacket ();

  static TypeId GetTypeId (void);

  uint8_t GetIdPacket ();

  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;

private:
  uint8_t m_idPacket;
};

class UanWakeupTag : public Tag
{
public:
  enum Type {WU,WUHE, DATA, CTD, CTS};
  Type m_type;

  static TypeId GetTypeId (void);

  UanWakeupTag ();

  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer start) const;
  virtual void Deserialize (TagBuffer start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;
};

}


#endif /* UAN_HEADER_WAKEUP_H_ */
