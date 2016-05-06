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

#include "uan-header-wakeup.h"
#include "uan-address.h"

namespace ns3 {

  NS_OBJECT_ENSURE_REGISTERED (UanHeaderWakeup);

UanHeaderWakeup::UanHeaderWakeup ()
{
}

UanHeaderWakeup::UanHeaderWakeup (const uint8_t addr)
  : Header (),
    m_dest (addr)
{

}

TypeId
UanHeaderWakeup::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanHeaderWakeup")
    .SetParent<Header> ()
    .SetGroupName ("Uan")
    .AddConstructor<UanHeaderWakeup> ()
  ;
  return tid;
}

TypeId
UanHeaderWakeup::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
UanHeaderWakeup::~UanHeaderWakeup ()
{
}


void
UanHeaderWakeup::SetDest (UanAddress dest)
{
  m_dest = dest;
}

UanAddress
UanHeaderWakeup::GetDest (void) const
{
  return m_dest;
}

// Inherrited methods

uint32_t
UanHeaderWakeup::GetSerializedSize (void) const
{
  return sizeof(uint16_t);
}

void
UanHeaderWakeup::Serialize (Buffer::Iterator start) const
{
  start.WriteU8 (m_dest.GetAsInt());
  start.WriteU8 (0);
}

uint32_t
UanHeaderWakeup::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator rbuf = start;

  UanAddress addr (rbuf.ReadU8());
  m_dest = addr;

  rbuf.ReadU8 ();
  return rbuf.GetDistanceFrom (start);
}

void
UanHeaderWakeup::Print (std::ostream &os) const
{
  os << "UAN wakeup addr=" << (uint32_t) m_dest.GetAsInt ();
}


/*
 * UanHeaderDuration
 */
NS_OBJECT_ENSURE_REGISTERED (UanHeaderDuration);


UanHeaderDuration::UanHeaderDuration ()
{
}

UanHeaderDuration::UanHeaderDuration (const float time)
  : Header (),
    m_time (time)
{

}

TypeId
UanHeaderDuration::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanHeaderDuration")
    .SetParent<Header> ()
	.SetGroupName ("Uan")
    .AddConstructor<UanHeaderDuration> ()
  ;
  return tid;
}

TypeId
UanHeaderDuration::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
UanHeaderDuration::~UanHeaderDuration ()
{
}


void
UanHeaderDuration::SetTxDuration (float time)
{
  m_time = time;
}

float
UanHeaderDuration::GetTxDuration (void) const
{
  return m_time;
}

// Inherrited methods

uint32_t
UanHeaderDuration::GetSerializedSize (void) const
{
  return sizeof(float);
}

void
UanHeaderDuration::Serialize (Buffer::Iterator start) const
{
  uint8_t* buffer = (uint8_t*) &m_time;
  start.Write(buffer, sizeof(float));
}

uint32_t
UanHeaderDuration::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator rbuf = start;
  uint8_t* buffer = (uint8_t*) &m_time;

  rbuf.Read (buffer, sizeof(float));

  return rbuf.GetDistanceFrom (start);
}

void
UanHeaderDuration::Print (std::ostream &os) const
{
  os << "UAN tx duration=" << m_time;
}


/*
 * UanHeaderData
 */
NS_OBJECT_ENSURE_REGISTERED (UanData);


UanData::UanData ()
{
  m_dataSize = 0;
}

UanData::UanData (uint32_t dataSize)
  : Header (),
    m_dataSize (dataSize)
{

}

TypeId
UanData::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanData")
    .SetParent<Header> ()
	.SetGroupName ("Uan")
    .AddConstructor<UanData> ()
  ;
  return tid;
}

TypeId
UanData::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
UanData::~UanData ()
{
}


// Inherrited methods

uint32_t
UanData::GetSerializedSize (void) const
{
  return m_dataSize;
}

void
UanData::Serialize (Buffer::Iterator start) const
{

}

uint32_t
UanData::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator rbuf = start;

  return rbuf.GetDistanceFrom (start);
}

void
UanData::Print (std::ostream &os) const
{

}


/*
 * UanHeaderIdPacket
 */
NS_OBJECT_ENSURE_REGISTERED (UanIdPacket);


UanIdPacket::UanIdPacket ()
{
  m_idPacket = 0;
}

UanIdPacket::UanIdPacket (uint8_t idPacket)
  : Header (),
    m_idPacket (idPacket)
{

}

TypeId
UanIdPacket::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanIdPacket")
    .SetParent<Header> ()
	.SetGroupName ("Uan")
    .AddConstructor<UanIdPacket> ()
  ;
  return tid;
}

TypeId
UanIdPacket::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
UanIdPacket::~UanIdPacket ()
{
}

uint8_t
UanIdPacket::GetIdPacket()
{
  return m_idPacket;
}

// Inherrited methods

uint32_t
UanIdPacket::GetSerializedSize (void) const
{
  return sizeof(m_idPacket);
}

void
UanIdPacket::Serialize (Buffer::Iterator start) const
{
  start.WriteU8 (m_idPacket);
}

uint32_t
UanIdPacket::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator rbuf = start;

  m_idPacket = start.ReadU8();

  return rbuf.GetDistanceFrom (start);
}

void
UanIdPacket::Print (std::ostream &os) const
{

}



/*
 * UanWakeupTag
 */

NS_OBJECT_ENSURE_REGISTERED (UanWakeupTag);


UanWakeupTag::UanWakeupTag ()
{
}

TypeId
UanWakeupTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanWakeupTag")
    .SetParent<Tag> ()
	.SetGroupName ("Uan")
    .AddConstructor<UanWakeupTag> ()
  ;
  return tid;
}

TypeId
UanWakeupTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}



// Inherrited methods

uint32_t
UanWakeupTag::GetSerializedSize (void) const
{
  return 1;
}

void
UanWakeupTag::Serialize (TagBuffer start) const
{
  start.WriteU8(m_type);
}

void
UanWakeupTag::Deserialize (TagBuffer start)
{
  m_type = (UanWakeupTag::Type) start.ReadU8();
}

void
UanWakeupTag::Print (std::ostream &os) const
{
  std::string print;

  switch (m_type)
  {
  case WU:
    print = "WU";
    break;
  case WUHE:
    print = "WUHE";
    break;
  case DATA:
    print = "DATA";
    break;
  }

  os << "type = " << print;
}

}


