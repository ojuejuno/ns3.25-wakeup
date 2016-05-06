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

#ifndef UAN_PHY_HEADER_H_
#define UAN_PHY_HEADER_H_

#include "ns3/header.h"
#include "ns3/trailer.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/tag.h"

namespace ns3 {

class UanPhyHeader : public Header
{
public:
  UanPhyHeader ();
  virtual ~UanPhyHeader ();

  static TypeId GetTypeId (void);

  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;
};

class UanPhyWUHeader : public Header
{
public:
  UanPhyWUHeader ();
  virtual ~UanPhyWUHeader ();

  static TypeId GetTypeId (void);

  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;
};

class UanPhyTrailer : public Trailer
{
public:
  UanPhyTrailer ();
  virtual ~UanPhyTrailer ();

  static TypeId GetTypeId (void);

  // Inherrited methods
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;
};



}


#endif /* UAN_HEADER_WAKEUP_H_ */
