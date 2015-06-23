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

#include "ndn-cdnpublisher.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include "ndn-cdnfile.hpp"
#include "model/ndn-app-face.hpp"
#include "model/ndn-ns3.hpp"
#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"
#include "ndn-cxx/name-component.hpp"
#include <memory>

NS_LOG_COMPONENT_DEFINE("ndn.CDNPublisher");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(CDNPublisher);

TypeId
CDNPublisher::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::CDNPublisher")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<CDNPublisher>()
      .AddAttribute("Prefix", "Prefix, for which producer has the data", StringValue("/"),
                    MakeNameAccessor(&CDNPublisher::m_prefix), MakeNameChecker())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
         StringValue("/"), MakeNameAccessor(&CDNPublisher::m_postfix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&CDNPublisher::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&CDNPublisher::m_freshness),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&CDNPublisher::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&CDNPublisher::m_keyLocator), MakeNameChecker())
	    .AddAttribute("m_MaxSize", "Maximum sequence number to request",
                    IntegerValue(0),
                    MakeIntegerAccessor(&CDNPublisher::m_MaxSize), MakeIntegerChecker<uint32_t>())
      .AddAttribute("LifeTime", "LifeTime for interest packet", StringValue("2s"),
                    MakeTimeAccessor(&CDNPublisher::m_interestLifeTime), MakeTimeChecker())
					
					;
  return tid;
}

CDNPublisher::CDNPublisher()
:m_rand(0, std::numeric_limits<uint32_t>::max())
{
  NS_LOG_FUNCTION_NOARGS();
  m_file = CDNFile(m_prefix);
  m_file.setMaxSize(m_MaxSize);
}

// inherited from Application base class.
void
CDNPublisher::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);
  SendPacket(1, 1);
}

void
CDNPublisher::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
CDNPublisher::OnInterest(shared_ptr<const Interest> interest)
{
  App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << interest);

  if (!m_active || interest->getName().getPrefix(interest->getName().size()-1)!= m_prefix || interest->getName().at(-1).toNumber()>m_MaxSize)
    return;
  
  Name dataName(interest->getName());
  // dataName.append(m_postfix);
  // dataName.appendVersion();

  auto data = make_shared<Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

  data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));
  data->setFinalBlockId(::ndn::name::Component::fromNumber(m_MaxSize));//wait to check

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  signature.setInfo(signatureInfo);
  signature.setValue(Block(&m_signature, sizeof(m_signature)));

  data->setSignature(signature);

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") respodning with Data: " << data->getName());

  // to create real wire encoding
  data->wireEncode();

  m_transmittedDatas(data, this, m_face);
  m_face->onReceiveData(*data);
}

void
CDNPublisher::SendPacket(uint32_t cdn_id, uint32_t interaction_type)
{
  if (!m_active)
    return;

  NS_LOG_FUNCTION_NOARGS();
  
  shared_ptr<Name> name = make_shared<Name>("CDN/Interaction/");
  name->appendNumber(cdn_id);	//CDN_id
  name->appendNumber(interaction_type);	//Interaction type
  name->append(m_prefix);
  //

  // shared_ptr<Interest> interest = make_shared<Interest> ();
  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand.GetValue());
  interest->setName(*name);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  // NS_LOG_INFO ("Requesting Interest: \n" << *interest);
  NS_LOG_INFO("send interaction interest to publish a file"<< cdn_id << interaction_type );

  m_transmittedInterests(interest, this, m_face);
  m_face->onReceiveInterest(*interest);
}


} // namespace ndn
} // namespace ns3
