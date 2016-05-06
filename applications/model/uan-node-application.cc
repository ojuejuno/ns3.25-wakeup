/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Copyright (c) 2006 Georgia Tech Research Corporation
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Author: George F. Riley<riley@ece.gatech.edu>
//

// ns3 - On/Off Data Source Application class
// George F. Riley, Georgia Tech, Spring 2007
// Adapted from ApplicationOnOff in GTNetS.

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "uan-node-application.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/double.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("UanNodeApplication");

NS_OBJECT_ENSURE_REGISTERED (UanNodeApplication);

TypeId
UanNodeApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UanNodeApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<UanNodeApplication> ()
    .AddAttribute ("DataRate", "The data rate in on state.",
                   DataRateValue (DataRate ("500kb/s")),
                   MakeDataRateAccessor (&UanNodeApplication::m_cbrRate),
                   MakeDataRateChecker ())
    .AddAttribute ("PacketSize", "The size of packets sent in on state",
                   UintegerValue (512),
                   MakeUintegerAccessor (&UanNodeApplication::m_pktSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&UanNodeApplication::m_peer),
                   MakeAddressChecker ())
    /*.AddAttribute ("OnTime", "A RandomVariableStream used to pick the duration of the 'On' state.",
                   StringValue ("ns3::UniformRandomVariable"),
                   MakePointerAccessor (&UanNodeApplication::m_onTime),
                   MakePointerChecker <RandomVariableStream>())*/
    /*.AddAttribute ("OffTime", "A RandomVariableStream used to pick the duration of the 'Off' state.",
                   StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                   MakePointerAccessor (&UanNodeApplication::m_offTime),
                   MakePointerChecker <RandomVariableStream>())*/
    .AddAttribute ("MaxBytes", 
                   "The total number of bytes to send. Once these bytes are sent, "
                   "no packet is sent again, even in on state. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&UanNodeApplication::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&UanNodeApplication::m_tid),
                   MakeTypeIdChecker ())
    
	.AddAttribute ("PktsPerTime", 
                   "number of packets generated per time cycle.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&UanNodeApplication::m_pktsPerTime),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("MinIntervalTime", 
                   "Min packet generate interval time.",
                   DoubleValue (0.01),
                   MakeDoubleAccessor (&UanNodeApplication::m_minTime),
                   MakeDoubleChecker<double> ())
	.AddAttribute ("MaxIntervalTime", 
                   "Max packet generate interval time.",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&UanNodeApplication::m_maxTime),
                   MakeDoubleChecker<double> ())				   
	.AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&UanNodeApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")				 
  ;
  return tid;
}



UanNodeApplication::UanNodeApplication ()
  : m_socket (0),
    m_connected (false),
    m_residualBits (0),
    m_lastStartTime (Seconds (0)),
    m_totBytes (0),
	m_residualPkts (0)
{
  NS_LOG_FUNCTION (this);
}

UanNodeApplication::~UanNodeApplication()
{
  NS_LOG_FUNCTION (this);
}

void 
UanNodeApplication::SetMaxBytes (uint32_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

Ptr<Socket>
UanNodeApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

int64_t 
UanNodeApplication::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  //m_onTime->SetStream (stream);
  //m_offTime->SetStream (stream + 1);
  return 2;
}

void
UanNodeApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void UanNodeApplication::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind6 ();
        }
      else if (InetSocketAddress::IsMatchingType (m_peer) ||
               PacketSocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind ();
        }
      m_socket->Connect (m_peer);
      m_socket->SetAllowBroadcast (true);
      m_socket->ShutdownRecv ();

      m_socket->SetConnectCallback (
        MakeCallback (&UanNodeApplication::ConnectionSucceeded, this),
        MakeCallback (&UanNodeApplication::ConnectionFailed, this));
    }
  m_cbrRateFailSafe = m_cbrRate;
  m_rng=CreateObject<ExponentialRandomVariable> ();
  m_rng->SetAttribute ("Mean", DoubleValue (m_maxTime));
  //m_rng->SetAttribute ("Min", DoubleValue (m_minTime));
  //m_rng->SetAttribute ("Max", DoubleValue (m_maxTime));
  // Insure no pending event
  //CancelEvents ();
  // If we are not yet connected, there is nothing to do here
  // The ConnectionComplete upcall will start timers at that time
  //if (!m_connected) return;
  ScheduleNextGen();
}

void UanNodeApplication::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  //CancelEvents ();
  if(m_socket != 0)
    {
      m_socket->Close ();
	  m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("UanNodeApplication found null socket to close in StopApplication");
    }
}

/*void UanNodeApplication::CancelEvents ()
{
  NS_LOG_FUNCTION (this);

  if (m_sendEvent.IsRunning () && m_cbrRateFailSafe == m_cbrRate )
    { // Cancel the pending send packet event
      // Calculate residual bits since last packet sent
      Time delta (Simulator::Now () - m_lastStartTime);
      int64x64_t bits = delta.To (Time::S) * m_cbrRate.GetBitRate ();
      m_residualBits += bits.GetHigh ();
    }
  m_cbrRateFailSafe = m_cbrRate;
  Simulator::Cancel (m_sendEvent);
  Simulator::Cancel (m_startStopEvent);
}
*/

// Private helpers
void UanNodeApplication::ScheduleNextGen()
{
  NS_LOG_FUNCTION(this);
  
  
  if(m_maxBytes==0||m_totBytes<m_maxBytes)
  {
	Time nextTime(Seconds(m_rng->GetValue()));

    m_genEvent=Simulator::Schedule(nextTime,&UanNodeApplication::GenPacket,this);	
  }
  else
  {
    StopApplication();
  }
  SendData();
}

void UanNodeApplication::GenPacket()
{
  m_residualPkts+=m_pktsPerTime;
  ScheduleNextGen();
  
}


void UanNodeApplication::SendData (void)
{
  NS_LOG_FUNCTION (this);

  while (m_residualPkts > 0 && !(m_sendEvent.IsRunning ()))
    { // Time to send more

      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
      Ptr<Packet> packet = Create<Packet> ( m_pktSize);
      m_txTrace (packet);
      uint32_t actual = m_socket->Send (packet);
	  uint32_t loopCount = 0;
      if (actual > 0)
        {
          m_totBytes += actual;
		  m_residualPkts -- ;
        }
      // We exit this loop when actual < toSend as the send side
      // buffer is full. The "DataSent" callback will pop when
      // some buffer space has freed ip.
      /*if ((unsigned)actual != toSend)
        {
          break;
        }*/
		loopCount++;
		if(loopCount>10)
		{
		  break;
		}
		
    }
	
  // Check if time to close (all sent)

}

/*void UanNodeApplication::SetOnTimeScale(double min, double max)
{
  m_rng>SetAttribute ("Min", DoubleValue (min));
  m_rng->SetAttribute ("Max", DoubleValue (max));
}
*/
void UanNodeApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  m_connected = true;
}

void UanNodeApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}


} // Namespace ns3
