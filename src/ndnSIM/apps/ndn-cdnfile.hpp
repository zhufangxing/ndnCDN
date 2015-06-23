

#ifndef NDN_CDNFILE_H
#define NDN_CDNFILE_H
#include <vector>

//#include "NFD/common.hpp"
#include "ns3/ndnSIM/model/ndn-common.hpp"
using namespace std;

namespace ns3 {
namespace ndn {

class CDNFile;

/** \brief represents a base class for CDNFile
 */
class CDNFile
{
public:
  CDNFile(); 
  
  CDNFile(const Name& name);  
  
  CDNFile(const Name& name, uint32_t size);
  /** \brief returns the name of the Data packet stored in the CDNFile
   *  \return{ NDN name }
   */

  ~CDNFile();

  const Name&
  getName() const;
  
  void 
  setName(const Name& name) ;
  
  const uint32_t
  getSize() const;
  /** \brief returns the Data packet stored in the CDNFile
   */
  const Data&
  getData(uint32_t seq) const;

  /** \brief changes the content of CDNFile and recomputes digest
   */
  bool
  setData(const Data& data, uint32_t seq);

  void
  setMaxSize(uint32_t size);

  const uint32_t
  getMaxSize() const;
  /** \brief returns the absolute time when Data becomes expired
   *  \return{ Time (resolution up to time::milliseconds) }
   */
  const time::steady_clock::TimePoint&
  getStaleTime() const;

  /** \brief refreshes the time when Data becomes expired
   *  according to the current absolute time.
   */
  void
  updateStaleTime();

  /** \brief checks if the stored Data is stale
   */
  bool
  isStale() const;
	
  void
  isPublish();
  /** \brief clears CDNFile
   *  After reset, *this == CDNFile()
   */
  void
  reset();
  
  inline bool
  isUnsolicited() const;

private:
  bool m_isUnsolicited;

  Name m_fileName;
  std::vector<Data> m_CDNFile;
  uint32_t m_MaxSize; 
  uint32_t m_size;
  bool m_publish;  
  //uint32_t visits;
  //not been used yet
  time::steady_clock::TimePoint m_staleAt;
  
};

inline const Name&
CDNFile::getName() const
{
  //BOOST_ASSERT(m_fileName != nullptr);
  return m_fileName;
}

inline void 
CDNFile::setName(const Name& name) 
{
  m_fileName = name;
} 

inline const uint32_t
CDNFile::getSize() const
{
  return m_size;
}

inline const uint32_t
CDNFile::getMaxSize() const
{
  return m_MaxSize;
}
inline const Data&
CDNFile::getData(uint32_t seq) const
{
  BOOST_ASSERT(m_CDNFile.size()>seq);
  return m_CDNFile.at(seq);
}

inline bool
CDNFile::isUnsolicited() const
{
  return m_isUnsolicited;
}

inline const time::steady_clock::TimePoint&
CDNFile::getStaleTime() const
{
  return m_staleAt;
}

} // namespace cs
} // namespace nfd

#endif // NFD_DAEMON_TABLE_CS_ENTRY_HPP
