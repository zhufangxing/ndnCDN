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

#ifndef NDN_CDNPRODUCER_H
#define NDN_CDNPRODUCER_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ndn-cdnfile.hpp"
#include "ndn-cdnstore.hpp"
#include "ndn-app.hpp"

#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/ndnSIM/model/ndn-app-face.hpp"

#include "ns3/callback.h"
#include "ns3/traced-callback.h"

namespace ns3 {
namespace ndn {

class CDNStore;

/**
 * @ingroup ndn-apps
 * @brief A simple Interest-sink applia simple Interest-sink application
 *
 * A simple Interest-sink applia simple Interest-sink application,
 * which replying every incoming Interest with Data packet with a specified
 * size and name same as in Interest.cation, which replying every incoming Interest
 * with Data packet with a specified size and name same as in Interest.
 */
class CDNProducer  {
public:
  static TypeId
  GetTypeId(void);

  CDNProducer();

  CDNProducer(shared_ptr<Face> face, Ptr<App> app);


  void 
  SetFace(shared_ptr<Face> face){ m_face = face; };


  

  // inherited from NdnApp
  virtual void
  OnInterest(shared_ptr<const Interest> interest, CDNStore& m_CDNStore);

protected:
  

   TracedCallback<shared_ptr<const Interest>, Ptr<App>, shared_ptr<Face>>
    m_receivedInterests; ///< @brief App-level trace of received Interests

  TracedCallback<shared_ptr<const Data>, Ptr<App>, shared_ptr<Face>>
    m_receivedDatas; ///< @brief App-level trace of received Data

  TracedCallback<shared_ptr<const Interest>, Ptr<App>, shared_ptr<Face>>
    m_transmittedInterests; ///< @brief App-level trace of transmitted Interests

  TracedCallback<shared_ptr<const Data>, Ptr<App>, shared_ptr<Face>>
    m_transmittedDatas; ///< @brief App-level trace of transmitted Data


private:
  Name m_prefix;    // not used
  Name m_postfix;   // not used
  uint32_t m_MaxSize;   //not used
  CDNFile m_file;       // not used
  uint32_t m_virtualPayloadSize;
  Time m_freshness;
  shared_ptr<Face> m_face;
  Ptr<App> m_app;

  uint32_t m_signature;
  Name m_keyLocator;
};

} // namespace ndn
} // namespace ns3

#endif // NDN_PRODUCER_H
