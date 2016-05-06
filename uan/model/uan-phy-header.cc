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

#include "uan-phy-header.h"
#include "uan-address.h"

namespace ns3 {

  NS_OBJECT_ENSURE_REGISTERED (UanPhyHeader);

  UanPhyHeader::UanPhyHeader ()
{
}

TypeId
UanPhyHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanPhyHeader")
  
    .SetParent<Header> ()
    .SetGroupName ("Uan")
    .AddConstructor<UanPhyHeader> ()
  ;
  return tid;
}

TypeId
UanPhyHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
UanPhyHeader::~UanPhyHeader ()
{
}

// Inherrited methods

uint32_t
UanPhyHeader::GetSerializedSize (void) const
{
  //
  uint32_t size = 4 //sync bytes
      + 1 // start byte
      + 1 // payload long
      ;
  return size;
}

void
UanPhyHeader::Serialize (Buffer::Iterator start) const
{
  start.WriteU32(0);
  start.WriteU8(0);
  start.WriteU8(0);
}

uint32_t
UanPhyHeader::Deserialize (Buffer::Iterator start)
{

  return GetSerializedSize ();
}

void
UanPhyHeader::Print (std::ostream &os) const
{
  os << "UAN Phy Header";
}


NS_OBJECT_ENSURE_REGISTERED (UanPhyWUHeader);

UanPhyWUHeader::UanPhyWUHeader ()
{
}

TypeId
UanPhyWUHeader::GetTypeId (void)
{
static TypeId tid = TypeId ("ns3::UanPhyWUHeader")
  .SetParent<Header> ()
  .SetGroupName ("Uan")
  .AddConstructor<UanPhyWUHeader> ()
;
return tid;
}

TypeId
UanPhyWUHeader::GetInstanceTypeId (void) const
{
return GetTypeId ();
}
UanPhyWUHeader::~UanPhyWUHeader ()
{
}

// Inherrited methods

uint32_t
UanPhyWUHeader::GetSerializedSize (void) const
{
//
uint32_t size = 1 //carrier byte
    + 1 // preamble byte
    ;
return size;
}

void
UanPhyWUHeader::Serialize (Buffer::Iterator start) const
{
  start.WriteU8(0);
  start.WriteU8(0);
}

uint32_t
UanPhyWUHeader::Deserialize (Buffer::Iterator start)
{


return GetSerializedSize ();
}

void
UanPhyWUHeader::Print (std::ostream &os) const
{
os << "UAN Phy WU Header";
}



NS_OBJECT_ENSURE_REGISTERED (UanPhyTrailer);

UanPhyTrailer::UanPhyTrailer ()
{
}

TypeId
UanPhyTrailer::GetTypeId (void)
{
static TypeId tid = TypeId ("ns3::UanPhyTrailer")
  .SetParent<Trailer> ()
  .SetGroupName ("Uan")
  .AddConstructor<UanPhyTrailer> ()
;
return tid;
}

TypeId
UanPhyTrailer::GetInstanceTypeId (void) const
{
return GetTypeId ();
}
UanPhyTrailer::~UanPhyTrailer ()
{
}

// Inherrited methods

uint32_t
UanPhyTrailer::GetSerializedSize (void) const
{
//
uint32_t size = 1 //crc
    + 1 // stop
    + 2 // sync
    ;
return size;
}

void
UanPhyTrailer::Serialize (Buffer::Iterator start) const
{
  start.Prev (GetSerializedSize ());
  start.WriteU8(0);
  start.WriteU8(0);
  start.WriteU16(0);
}

uint32_t
UanPhyTrailer::Deserialize (Buffer::Iterator start)
{

return GetSerializedSize ();
}

void
UanPhyTrailer::Print (std::ostream &os) const
{
os << "UAN Phy Trailer";
}

}


