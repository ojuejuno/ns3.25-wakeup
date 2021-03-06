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

#ifndef UAN_FAMA_TEST_EXPOSED_H
#define UAN_FAMA_TEST_EXPOSED_H

#include "ns3/network-module.h"
#include "ns3/stats-module.h"
#include "ns3/uan-module.h"

using namespace ns3;

/**
 * \ingroup uan
 * \brief Helper class for UAN CW MAC example.
 *
 * An experiment measures the average throughput for a series of CW values.
 *
 * \see uan-fama-test.cc
 */
class Experiment
{
public:
  /**
   * Run an experiment across a range of congestion window values.
   *
   * \param uan The Uan stack helper to configure nodes in the model.
   * \return The data set of CW values and measured throughput
   */
  void Run (UanHelper &uan);
  /**
   * Receive all available packets from a socket.
   *
   * \param socket The receive socket.
   */
  void ReceivePacket (Ptr<Packet> packet, const UanAddress &src);
  /**
   * Assign new random positions to a set of nodes.  New positions
   * are randomly assigned within the bounding box.
   *
   * \param nodes The nodes to reposition.
   */
  void UpdatePositions (NodeContainer &nodes);
  /** Save the throughput from a single run. */
  void ResetData ();
  /**
   * Compute average throughput for a set of runs, then increment CW.
   *
   * \param cw CW value for completed runs.
   */
  void IncrementCw (uint32_t cw);
  
  void ReportPower (DeviceEnergyModelContainer cont);
  void ReportData();
  void Send (NetDeviceContainer &devices, UanAddress &uanAddress, uint8_t src);
  uint32_t m_numNodes;                //!< Number of transmitting nodes.
  uint32_t m_dataRate;                //!< DataRate in bps.
  double m_depth;                     //!< Depth of transmitting and sink nodes.
  double m_boundary;                  //!< Size of boundary in meters.
  uint32_t m_packetSize;              //!< Generated packet size in bytes.
  uint32_t m_bytesTotal;              //!< Total bytes received.
  uint32_t m_cwMin;                   //!< Min CW to simulate.
  uint32_t m_cwMax;                   //!< Max CW to simulate.
  uint32_t m_cwStep;                  //!< CW step size, default 10.
  uint32_t m_avgs;                    //!< Number of topologies to test for each cw point.
                                 
  Time m_slotTime;                    //!< Slot time duration.
  Time m_simTime;                     //!< Simulation run time, default 1000 s.

  std::string m_gnudatfile;           //!< Name for GNU Plot output, default uan-fama-test.gpl.
  std::string m_asciitracefile;       //!< Name for ascii trace file, default uan-fama-test.asc.
  std::string m_bhCfgFile;            //!< (Unused)

  double m_maxOfferedLoad;
  double m_offeredLoad;
  
  Gnuplot3dDataset m_data;            //!< Container for the simulation data.
  std::vector<double> m_throughputs;  //!< Throughput for each run.
  
  std::vector<double> m_powerBuffer;
  std::vector<double> m_sinkPower;
  
  bool m_doExposed;
  
  Ptr<ExponentialRandomVariable> m_randExp;
  /** Default constructor. */
  Experiment ();
};

#endif /* UAN_FAMA_TEST_EXPOSED_H */
