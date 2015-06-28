/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ndn-cdnconsumer.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include "utils/ndn-ns3-packet-tag.hpp"
#include "model/ndn-app-face.hpp"
#include "utils/ndn-rtt-mean-deviation.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/ref.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.CDNConsumer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(CDNConsumer);

TypeId
CDNConsumer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::CDNConsumer")
      .SetGroupName("Ndn")
      .AddAttribute("StartSeq", "Initial sequence number", IntegerValue(0),
                    MakeIntegerAccessor(&CDNConsumer::m_seq), MakeIntegerChecker<int32_t>())

      .AddAttribute("Prefix", "Name of the Interest", StringValue("/"),
                    MakeNameAccessor(&CDNConsumer::m_interestName), MakeNameChecker())
      .AddAttribute("LifeTime", "LifeTime for interest packet", StringValue("2s"),
                    MakeTimeAccessor(&CDNConsumer::m_interestLifeTime), MakeTimeChecker())

      .AddAttribute("RetxTimer",
                    "Timeout defining how frequent retransmission timeouts should be checked",
                    StringValue("50ms"),
                    MakeTimeAccessor(&CDNConsumer::GetRetxTimer, &CDNConsumer::SetRetxTimer),
                    MakeTimeChecker())

      .AddTraceSource("LastRetransmittedInterestDataDelay",
                      "Delay between last retransmitted Interest and received Data",
                      MakeTraceSourceAccessor(&CDNConsumer::m_lastRetransmittedInterestDataDelay))

      .AddTraceSource("FirstInterestDataDelay",
                      "Delay between first transmitted Interest and received Data",
                      MakeTraceSourceAccessor(&CDNConsumer::m_firstInterestDataDelay))
					  
	  .AddAttribute("Frequency", "Frequency of interest packets", StringValue("1.0"),
                    MakeDoubleAccessor(&CDNConsumer::m_frequency), MakeDoubleChecker<double>())

      .AddAttribute("Randomize",
                    "Type of send time randomization: none (default), uniform, exponential",
                    StringValue("none"),
                    MakeStringAccessor(&CDNConsumer::SetRandomize, &CDNConsumer::GetRandomize),
                    MakeStringChecker())

      .AddAttribute("MaxSeq", "Maximum sequence number to request",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeIntegerAccessor(&CDNConsumer::m_seqMax), MakeIntegerChecker<uint32_t>())
					
	 ;
	 
  return tid;
}

CDNConsumer::CDNConsumer()
  : m_rand(0, std::numeric_limits<uint32_t>::max())
  , m_seq(0)
  , m_seqMax(0) // don't request anything
  , m_frequency(1.0)
  , m_firstTime(true)
  , m_random(0)
{
  NS_LOG_FUNCTION_NOARGS();
  
  m_seqMax = std::numeric_limits<uint32_t>::max();
  m_rtt = CreateObject<RttMeanDeviation>();
}

CDNConsumer::CDNConsumer(shared_ptr<Face> face, Ptr<App> app)
  : m_rand(0, std::numeric_limits<uint32_t>::max())
  , m_seq(0)
  , m_seqMax(0) // don't request anything
  , m_frequency(1.0)
  , m_firstTime(true)
  , m_random(0)
{
  NS_LOG_FUNCTION_NOARGS();
  
  m_seqMax = std::numeric_limits<uint32_t>::max();
  m_rtt = CreateObject<RttMeanDeviation>();
  m_face = face;
  m_app = app;
}

void
CDNConsumer::ScheduleNextPacket()
{
  // double mean = 8.0 * m_payloadSize / m_desiredRate.GetBitRate ();
  // std::cout << "next: " << Simulator::Now().ToDouble(Time::S) + mean << "s\n";

  if (m_firstTime) {
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &CDNConsumer::SendPacket, this);
    m_firstTime = false;
  }
  else if (!m_sendEvent.IsRunning())
    m_sendEvent = Simulator::Schedule((m_random == 0) ? Seconds(1.0 / m_frequency)
                                                      : Seconds(m_random->GetValue()),
                                      &CDNConsumer::SendPacket, this);
}

void
CDNConsumer::SetRandomize(const std::string& value)
{
  if (m_random)
    delete m_random;

  if (value == "uniform") {
    m_random = new UniformVariable(0.0, 2 * 1.0 / m_frequency);
  }
  else if (value == "exponential") {
    m_random = new ExponentialVariable(1.0 / m_frequency, 50 * 1.0 / m_frequency);
  }
  else
    m_random = 0;

  m_randomType = value;
}

std::string
CDNConsumer::GetRandomize() const
{
  return m_randomType;
}

void
CDNConsumer::SetRetxTimer(Time retxTimer)
{
  m_retxTimer = retxTimer;
  if (m_retxEvent.IsRunning()) {
    // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
    Simulator::Remove(m_retxEvent); // slower, but better for memory
  }

  // schedule even with new timeout
  m_retxEvent = Simulator::Schedule(m_retxTimer, &CDNConsumer::CheckRetxTimeout, this);
}

Time
CDNConsumer::GetRetxTimer() const
{
  return m_retxTimer;
}

void
CDNConsumer::CheckRetxTimeout()
{
  Time now = Simulator::Now();

  Time rto = m_rtt->RetransmitTimeout();
  // NS_LOG_DEBUG ("Current RTO: " << rto.ToDouble (Time::S) << "s");

  while (!m_seqTimeouts.empty()) {
    SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
      m_seqTimeouts.get<i_timestamp>().begin();
    if (entry->time + rto <= now) // timeout expired?
    {
      uint32_t seqNo = entry->seq;
      m_seqTimeouts.get<i_timestamp>().erase(entry);
      OnTimeout(seqNo);
    }
    else
      break; // nothing else to do. All later packets need not be retransmitted
  }

  m_retxEvent = Simulator::Schedule(m_retxTimer, &CDNConsumer::CheckRetxTimeout, this);
}

// Application Methods
void
CDNConsumer::StartApplication() // Called at time specified by Start
{
  NS_LOG_FUNCTION_NOARGS();

  // do base stuff
 // App::StartApplication();

  ScheduleNextPacket();
}

void
CDNConsumer::StopApplication() // Called at time specified by Stop
{
  NS_LOG_FUNCTION_NOARGS();

  // cancel periodic packet generation
  Simulator::Cancel(m_sendEvent);

  // cleanup base stuff
  //App::StopApplication();
}

void
CDNConsumer::CDNPull(shared_ptr<CDNFile> m_transFile)
{
	m_seq=0;
	m_seqMax=0;
	m_firstTime=true;
	m_interestName=m_transFile->getName();
	SendPacket();
}

void
CDNConsumer::SendPacket()
{
  //if (!m_active)
    //return;

  NS_LOG_FUNCTION_NOARGS();

  uint32_t seq = std::numeric_limits<uint32_t>::max(); // invalid

  while (m_retxSeqs.size()) {
    seq = *m_retxSeqs.begin();
    m_retxSeqs.erase(m_retxSeqs.begin());
    break;
  }

  if (seq == std::numeric_limits<uint32_t>::max()) {
    if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
      if (m_seq >= m_seqMax) {
        return; // we are totally done
      }
    }

    seq = m_seq++;
  }

  //
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
  nameWithSequence->appendSequenceNumber(seq);
  //

  // shared_ptr<Interest> interest = make_shared<Interest> ();
  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand.GetValue());
  interest->setName(*nameWithSequence);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  // NS_LOG_INFO ("Requesting Interest: \n" << *interest);
  NS_LOG_INFO("> Interest for " << seq);

  WillSendOutInterest(seq);

  m_transmittedInterests(interest, m_app, m_face);
  m_face->onReceiveInterest(*interest);

  ScheduleNextPacket();
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
CDNConsumer::OnData(shared_ptr<const Data> data, shared_ptr<CDNFile> m_transFile)
{
  //if (!m_active)
    //return;

  //App::OnData(data); // tracing inside

  NS_LOG_FUNCTION(this << data);

  // NS_LOG_INFO ("Received content object: " << boost::cref(*data));
  //data is not the part of the first transmitting file, only one file is transmitted at one time
  if (data->getName().getSubName((data->getName().size()-1)) != m_transFile->getName())
	return;
  // This could be a problem......
  uint32_t seq = data->getName().at(-1).toSequenceNumber();
  NS_LOG_INFO("< DATA for " << seq);
  if (seq == 1)
  {
	m_seqMax = data->getFinalBlockId().toSequenceNumber();
	m_transFile->setMaxSize(m_seqMax);
  }
  int hopCount = -1;
  auto ns3PacketTag = data->getTag<Ns3PacketTag>();
  if (ns3PacketTag != nullptr) {
    FwHopCountTag hopCountTag;
    if (ns3PacketTag->getPacket()->PeekPacketTag(hopCountTag)) {
      hopCount = hopCountTag.Get();
      NS_LOG_DEBUG("Hop count: " << hopCount);
    }
  }

  SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find(seq);
  if (entry != m_seqLastDelay.end()) {
    m_lastRetransmittedInterestDataDelay(m_app, seq, Simulator::Now() - entry->time, hopCount);
	//added by zfx
	m_transFile->setData(*data,seq);
  }
  
  entry = m_seqFullDelay.find(seq);
  if (entry != m_seqFullDelay.end()) {
    m_firstInterestDataDelay(m_app, seq, Simulator::Now() - entry->time, m_seqRetxCounts[seq], hopCount);
  }

  m_seqRetxCounts.erase(seq);
  m_seqFullDelay.erase(seq);
  m_seqLastDelay.erase(seq);

  m_seqTimeouts.erase(seq);
  m_retxSeqs.erase(seq);
  
  m_rtt->AckSeq(SequenceNumber32(seq));
}

void
CDNConsumer::OnTimeout(uint32_t sequenceNumber)
{
  NS_LOG_FUNCTION(sequenceNumber);
  // std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " <<
  // m_rtt->RetransmitTimeout ().ToDouble (Time::S) << "s\n";

  m_rtt->IncreaseMultiplier(); // Double the next RTO
  m_rtt->SentSeq(SequenceNumber32(sequenceNumber),
                 1); // make sure to disable RTT calculation for this sample
  m_retxSeqs.insert(sequenceNumber);
  ScheduleNextPacket();
}

void
CDNConsumer::WillSendOutInterest(uint32_t sequenceNumber)
{
  NS_LOG_DEBUG("Trying to add " << sequenceNumber << " with " << Simulator::Now() << ". already "
                                << m_seqTimeouts.size() << " items");

  m_seqTimeouts.insert(SeqTimeout(sequenceNumber, Simulator::Now()));
  m_seqFullDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqLastDelay.erase(sequenceNumber);
  m_seqLastDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqRetxCounts[sequenceNumber]++;

  m_rtt->SentSeq(SequenceNumber32(sequenceNumber), 1);
}

} // namespace ndn
} // namespace ns3
