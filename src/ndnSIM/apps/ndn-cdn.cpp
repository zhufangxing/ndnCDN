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

#include "ndn-cdn.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include "model/ndn-app-face.hpp"
#include "model/ndn-ns3.hpp"
#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include <memory>

NS_LOG_COMPONENT_DEFINE("ndn.CDN");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(CDN);

TypeId
CDN::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::CDN")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<CDN>()
      .AddAttribute("Prefix", "Prefix, for which CDN has the data", StringValue("/"),
                    MakeNameAccessor(&CDN::m_prefix), MakeNameChecker())
	  .AddAttribute("Prefix2", "Prefix2, for interacting with all the CDN, added by zfx", StringValue("/"),
                    MakeNameAccessor(&CDN::m_prefix2), MakeNameChecker())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding CDN-uniqueness)",
         StringValue("/"), MakeNameAccessor(&CDN::m_postfix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&CDN::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&CDN::m_freshness),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&CDN::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&CDN::m_keyLocator), MakeNameChecker())
	
	;
	
  return tid;
}

CDN::CDN()
:m_CDNProducer(m_face, this)
,m_CDNConsumer(m_face, this)
{
  NS_LOG_FUNCTION_NOARGS();
  m_CDNStore=CDNStore();
  m_CDNStore.setNode(GetNode());
  m_CDNStore.setFace(m_face);

}

// inherited from Application base class.
void
CDN::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);
  FibHelper::AddRoute(GetNode(), m_prefix2, m_face, 0);
}

void
CDN::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

   App::StopApplication();
}

void
CDN::OnInterest(shared_ptr<const Interest> interest)
{
  //App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << interest);
  
  if (!m_active)
    return;
	
  Name prefix = interest->getName().getSubName(0,3);
  if (prefix == m_prefix || prefix == m_prefix2)
  {
	OnPushInterest(interest->getName());
	// push file or publish file, type = 0
	if (interest->getName().at(3).toNumber() == 0)		
	{
		shared_ptr<CDNFile> file(new CDNFile(interest->getName().getSubName(4,100)));
		m_transFiles.push(file);
		//no file is trasmitted now
		if (m_transFiles.size()==1)
			m_CDNConsumer.CDNPull(m_transFiles.front());
  }
	// publish a file , type = 1
	else if (interest->getName().at(3).toNumber() == 1)
	{
		shared_ptr<CDNFile> file(new CDNFile(interest->getName().getSubName(4,100)));
		file->isPublish();
		m_transFiles.push(file);
		//no file is trasmitted now
		if (m_transFiles.size()==1)
			m_CDNConsumer.CDNPull(m_transFiles.front());
	}
  }
  else
	m_CDNProducer.OnInterest(interest, m_CDNStore);
  
  
}

void
CDN::OnPushInterest(const Name &interestName)
{
  // respond a data only name to clear PIT, added by zfx
	Name dataName(interestName);
  // dataName.append(m_postfix);
  // dataName.appendVersion();

  auto data = make_shared<Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

  data->setContent(make_shared< ::ndn::Buffer>(0));

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  signature.setInfo(signatureInfo);
  signature.setValue(Block(&m_signature, sizeof(m_signature)));

  data->setSignature(signature);

  //NS_LOG_INFO("node(" << CDNConsumer::App::GetNode()->GetId() << ") respodning Push Information with Data: " << data->getName());

  // to create real wire encoding
  data->wireEncode();

  m_transmittedDatas(data, this, m_face);
  m_face->onReceiveData(*data);
}

void
CDN::OnData(shared_ptr<const Data> data)
{
  if (!m_active)
    return;
  Name dataName=data->getName();
  if (data->getName().getSubName(0,2) == m_postfix.getSubName(0, 2) )
	// 反馈的CDN内部通讯的交流信息，不作处理，data无意义
	return ;
  else
  {
	m_CDNConsumer.OnData(data, m_transFiles.front());
	// a file has been transmitted
	if (m_transFiles.front()->getMaxSize() == m_transFiles.front()->getSize())
	{
		m_CDNStore.insert(m_transFiles.front());
    m_transFiles.pop();
		
		//add route
		FibHelper::AddRoute(GetNode(), data->getName().getPrefix(data->getName().size()-1), m_face, 0);
		
		//transmit next file
		if (!m_transFiles.empty())
			m_CDNConsumer.CDNPull(m_transFiles.front());
	}
	
  }//else end
		
}

CDNStore&
CDN::getCDNStore()
{
  return m_CDNStore;
}

queue<shared_ptr<CDNFile>>&
CDN::getCDNFile()
{
  return m_transFiles;
}

} // namespace ndn
} // namespace ns3
