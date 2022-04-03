#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "exptype.h"
#include "octets.h"

namespace TCORE {

class OctetsStream;

class Protocol {
 public:
  virtual OctetsStream& Deserialize(OctetsStream& os) = 0;
  virtual OctetsStream& Serialize(OctetsStream& os) const = 0;
  virtual ~Protocol() {}
};

}  // namespace TCORE

#endif
