#include "ndn-cdnfile.hpp"
#include "core/logger.hpp"

namespace ns3 {
namespace ndn {

NFD_LOG_INIT("CDNFile");

CDNFile::CDNFile()
  : m_isUnsolicited(false)
  , m_MaxSize(0)
  , m_size(0)
  , m_publish(false)
{
}

 
CDNFile::CDNFile(const Name &name, uint32_t size)
  : m_isUnsolicited(false)
  , m_fileName(name)
  , m_MaxSize(size)
  , m_size(0)
  , m_publish(false)
{
  m_CDNFile.resize(m_size);
}

 
CDNFile::CDNFile(const Name &name)
  : m_isUnsolicited(false)
  , m_fileName(name)
  , m_MaxSize(0)
  , m_size(0)
  , m_publish(false)
{

}

CDNFile::~CDNFile()
{
  m_CDNFile.clear();
}

void 
CDNFile::isPublish()
{
	m_publish = true;
}
void 
CDNFile::setMaxSize(uint32_t size)
{
  m_MaxSize = size;
  m_CDNFile.resize(m_MaxSize);
}

bool
CDNFile::setData(const Data& data, uint32_t seq)
{
  if (seq >= m_MaxSize)
	   return false;
  m_CDNFile.at(seq) = data;
  m_size += 1;
  updateStaleTime();
  return true;
}

void
CDNFile::updateStaleTime()
{
  //m_staleAt = time::steady_clock::now() + m_dataPacket->getFreshnessPeriod();
}

bool
CDNFile::isStale() const
{
  return m_staleAt < time::steady_clock::now();
}

void
CDNFile::reset()
{
  m_staleAt = time::steady_clock::TimePoint();
  m_MaxSize = 0;
  m_size = 0;
  m_CDNFile.clear();
  m_CDNFile.resize (0);
  m_fileName = nullptr;
  m_isUnsolicited = false;
}

} // namespace cs
} // namespace nfd
