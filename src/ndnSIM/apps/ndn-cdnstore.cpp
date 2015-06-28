/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014,  Regents of the University of California,
 *                      Arizona Board of Regents,
 *                      Colorado State University,
 *                      University Pierre & Marie Curie, Sorbonne University,
 *                      Washington University in St. Louis,
 *                      Beijing Institute of Technology,
 *                      The University of Memphis
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * \author Ilya Moiseenko <http://ilyamoiseenko.com/>
 * \author Junxiao Shi <http://www.cs.arizona.edu/people/shijunxiao/>
 * \author Alexander Afanasyev <http://lasr.cs.ucla.edu/afanasyev/index.html>
 */

#include "ndn-cdnstore.hpp"
#include "ns3/ndnSIM/NFD/core/logger.hpp"
#include "ns3/ndnSIM/NFD/core/random.hpp"
#include "ns3/ndnSIM/model/ndn-app-face.hpp"
#include "ns3/node.h"
#include "helper/ndn-fib-helper.hpp"

#include <ndn-cxx/util/crypto.hpp>
#include <ndn-cxx/security/signature-sha256-with-rsa.hpp>
#include <ndn-cxx/management/nfd-control-parameters.hpp>

#include <boost/random/bernoulli_distribution.hpp>
#include <boost/concept/assert.hpp>
#include <boost/concept_check.hpp>
#include <type_traits>
#include "core/logger.hpp"

namespace ndn {
namespace nfd {
class ControlParameters;
} // namespace nfd
} // namespace ndn

namespace ns3 {
namespace ndn {
using ::ndn::nfd::ControlParameters;


//NFD_LOG_INIT(CDNStore);




CDNStore::CDNStore(size_t nMaxPackets)
  : m_nMaxPackets(nMaxPackets)
  , m_nPackets(0)
{

}

CDNStore::~CDNStore()
{
  // evict all items from CS
  while (evictItem())
    ;

  BOOST_ASSERT(m_CDNStore.size() == m_nMaxPackets);

  while (!m_CDNStore.empty())
    {
      evictItem();
    }
}

size_t
CDNStore::size() const
{
  return m_nPackets; // size of the first layer in a skip list
}

void
CDNStore::setLimit(size_t nMaxPackets)
{
  m_nMaxPackets = nMaxPackets;

}

size_t
CDNStore::getLimit() const
{
  return m_nMaxPackets;
}



bool
CDNStore::insert(shared_ptr<CDNFile> file)
{
  //NFD_LOG_TRACE("insert() " << file.getName());
  if (file->getMaxSize()>m_nMaxPackets)
	return false;
  m_nPackets += file->getMaxSize();
  while ( m_nPackets>m_nMaxPackets)
    {
      evictItem();
    }
  m_CDNStore.push_back(file);
  return true;
 
}



bool
CDNStore::isFull() const
{
  if (size() >= m_nMaxPackets) //size of the first layer vs. max size
    return true;

  return false;
}



bool
CDNStore::evictItem()
{
	if (m_CDNStore.empty())
		return false;
	shared_ptr<CDNFile> file = m_CDNStore.front();
	m_nPackets -= file->getMaxSize();
	const Name& filename = file->getName();
	// remove route
	::ndn::nfd::ControlParameters parameters;
	parameters.setName(filename);
	parameters.setFaceId(m_face->getId());
	parameters.setCost(0);
	FibHelper::RemoveRoute(parameters,m_node);
	// remove file
	file->reset();
	file->~CDNFile();
	m_CDNStore.pop_front();
	return true;
}

void
CDNStore::setFace(shared_ptr<Face> face)
{
	m_face = face;
}

void
CDNStore::setNode(Ptr<Node> node)
{
	m_node = node;
}

std::pair<bool, const CDNFile&>
CDNStore::find(const Name& fileName) const
{
  //NFD_LOG_TRACE("find() " << interest.getName());
  
  for (list<shared_ptr<CDNFile>>::const_iterator it = m_CDNStore.begin(); it!=m_CDNStore.end(); it++)
  { 
	if ((*it)->getName()==fileName)
		return {true, **it};
  }
  return {false, **m_CDNStore.end()};
}


void
CDNStore::erase(const Name& exactName)
{
  for (list<shared_ptr<CDNFile>>::iterator it = m_CDNStore.begin(); it!=m_CDNStore.end(); it++)
  {
	if ((*it)->getName()==exactName)
		m_CDNStore.erase(it);
		break;
  }
}



} //namespace nfd
}