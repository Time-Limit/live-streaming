#include "server/command_message.h"

namespace live {
namespace util {
namespace rtmp {

void ActionScriptObject::Serialize(ByteStream& bs) const {
  switch (marker) {
    case DOUBLE: {
      bs << marker << double_value;
      break;
    }
    case BOOLEAN: {
      bs << marker << bool_value;
      break;
    }
    case STRING: {
      bs << marker << uint16_t(string_value.size())
         << ByteStream::ConstRawPtrWrapper(
                reinterpret_cast<const uint8_t*>(&string_value[0]),
                string_value.size());
      break;
    }
    case OBJECT: {
      bs << marker;
      for (auto& pr : dict_value) {
        bs << uint16_t(pr.first.size()) << *pr.second.get();
      }
      bs << uint16_t(0) << uint8_t(END);
      break;
    }
    case NULL_TYPE: {
      break;
    }
    default: {
      LOG_ERROR << "not handler this marker " << uint16_t(marker);
      assert(false);
    }
  }
}

void ActionScriptObject::Deserialize(ByteStream& bs) {
  marker = UNINIT;
  if (!bs.Remain()) {
    return;
  }
  marker = bs.Pop<uint8_t>();
  switch (marker) {
    case DOUBLE: {
      double_value = bs.Pop<double>();
      break;
    }
    case BOOLEAN: {
      bool_value = bs.Pop<bool>();
      break;
    }
    case STRING: {
      string_value = bs.PopString(bs.Pop<uint16_t>());
      break;
    }
    case OBJECT: {
      while (true) {
        std::string name = bs.PopString(bs.Pop<uint16_t>());
        std::shared_ptr<ActionScriptObject> obj(new ActionScriptObject());
        bs >> *obj;
        if (obj->marker == END) {
          break;
        }
        if (obj->marker == UNINIT) {
          throw ByteStream::NotEnoughException();
        }
        dict_value[name] = obj;
      }
      break;
    };
    case NULL_TYPE: {
      break;
    }
    case END: {
      break;
    }
    default: {
      LOG_ERROR << "not handler this marker " << uint32_t(marker)
                << ", discard remain data";
      bs >> ByteStream::Discard(bs.Remain());
      break;
    }
  }
}

void CommandMessage::Serialize(ByteStream& bs) const {
  bs << uint8_t(ActionScriptObject::Type::STRING) << uint16_t(name.size())
     << ByteStream::ConstRawPtrWrapper(
            reinterpret_cast<const uint8_t*>(&name[0]), name.size());
  bs << uint8_t(ActionScriptObject::Type::DOUBLE) << id;

  bs << obj1 << obj2;
}

void CommandMessage::Deserialize(ByteStream& bs) {
  // 丢弃 marker
  bs.Pop<uint8_t>();
  name = bs.PopString(bs.Pop<uint16_t>());

  // 丢弃 marker
  bs.Pop<uint8_t>();
  id = bs.Pop<double>();

  LOG_ERROR << "name: " << name << ", id: " << id;

  bs >> obj1 >> obj2;

  obj1.Output();
  obj2.Output();
}

}  // namespace rtmp
}  // namespace util
}  // namespace live
