#pragma once

#include "server/chunk_message.h"
#include "server/net.h"
#include "server/stream.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace live {
namespace util {
namespace rtmp {

struct ActionScriptObject : public Protocol {
  enum Type {
    DOUBLE = 0,
    BOOLEAN = 1,
    STRING = 2,
    OBJECT = 3,
    NULL_TYPE = 5,
    ECMA_ARRAY = 8,
    END = 9,
    UNINIT = 255,
  };

  uint8_t marker = UNINIT;

  std::string name;
  union {
    bool bool_value;
    double double_value;
  };
  std::string string_value;

  using DictType =
      std::unordered_map<std::string, std::shared_ptr<ActionScriptObject>>;
  DictType dict_value;

  void Serialize(ByteStream& bs) const override;
  void Deserialize(ByteStream& bs) override;

  void Output() const {
    switch (marker) {
      case UNINIT: {
        LOG_ERROR << "UNINIT";
        break;
      }
      case DOUBLE: {
        LOG_ERROR << "double: " << double_value;
        break;
      }
      case BOOLEAN: {
        LOG_ERROR << "bool: " << bool_value;
        break;
      }
      case STRING: {
        LOG_ERROR << "string: " << string_value;
        break;
      }
      case ECMA_ARRAY:
      case OBJECT: {
        LOG_ERROR << "dict, size: " << dict_value.size();
        for (const auto& pr : dict_value) {
          LOG_ERROR << "name: " << pr.first;
          pr.second->Output();
        }
        break;
      }
      case NULL_TYPE: {
        LOG_ERROR << "null";
        break;
      }
      default: {
        LOG_ERROR << "not handler this marker " << uint32_t(marker);
      }
    }
  }

  virtual ~ActionScriptObject() {}
};

struct CommandMessage : public Message {
  std::string name;
  double id;
  ActionScriptObject obj1;
  ActionScriptObject obj2;
  ActionScriptObject obj3;
  ActionScriptObject obj4;
  ActionScriptObject obj5;
  ActionScriptObject obj6;
  ActionScriptObject obj7;
  ActionScriptObject obj8;

  CommandMessage(const std::string& n = "", double i = 0) : name(n), id(i) {
    Message::type = 20;
  }

  void Serialize(ByteStream& bs) const override;
  void Deserialize(ByteStream& bs) override;
};

}  // namespace rtmp
}  // namespace util
}  // namespace live
