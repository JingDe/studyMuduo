
#include"codec.h"
#include<cstdio>
#include<muduo/base/Logging.h>
#include<muduo/net/Endian.h>
#include<muduo/net/protorpc/google-inl.h>

#include<google/protobuf/descriptor.h>

#include<zlib.h>

using namespace muduo;
using namespace muduo::net;

void ProtobufCodec::fillEmptyBuffer(muduo::net::Buffer* buf, const google::protobuf::Message& message)
{
  // buf->retrieveAll();
  assert(buf->readableBytes() == 0);

  const std::string& typeName = message.GetTypeName();
  int32_t nameLen = static_cast<int32_t>(typeName.size()+1);
  buf->appendInt32(nameLen);
  buf->append(typeName.c_str(), nameLen); 

  // code copied from MessageLite::SerializeToArray() and MessageLite::SerializePartialToArray().
  GOOGLE_DCHECK(message.IsInitialized()) << InitializationErrorMessage("serialize", message);

  int byte_size = message.ByteSize();
  buf->ensureWritableBytes(byte_size);

  uint8_t* start = reinterpret_cast<uint8_t*>(buf->beginWrite());
  uint8_t* end = message.SerializeWithCachedSizesToArray(start);
  if (end - start != byte_size)
  {
    ByteSizeConsistencyError(byte_size, message.ByteSize(), static_cast<int>(end - start));
  }
  buf->hasWritten(byte_size);

  int32_t checkSum = static_cast<int32_t>(
      ::adler32(1,
                reinterpret_cast<const Bytef*>(buf->peek()),
                static_cast<int>(buf->readableBytes())));
  buf->appendInt32(checkSum);
  assert(buf->readableBytes() == sizeof nameLen + nameLen + byte_size + sizeof checkSum);
  int32_t len = sockets::hostToNetwork32(static_cast<int32_t>(buf->readableBytes()));
  buf->prepend(&len, sizeof len);
}


namespace
{
  const muduo::string kNoErrorStr = "NoError";
  const muduo::string kInvalidLengthStr = "InvalidLength";
  const muduo::string kCheckSumErrorStr = "CheckSumError";
  const muduo::string kInvalidNameLenStr = "InvalidNameLen";
  const muduo::string kUnknownMessageTypeStr = "UnknownMessageType";
  const muduo::string kParseErrorStr = "ParseError";
  const muduo::string kUnknownErrorStr = "UnknownError";
}


const muduo::string& ProtobufCodec::errorCodeToString(ErrorCode errorCode)
{
  switch (errorCode)
  {
   case kNoError:
     return kNoErrorStr;
   case kInvalidLength:
     return kInvalidLengthStr;
   case kCheckSumError:
     return kCheckSumErrorStr;
   case kInvalidNameLen:
     return kInvalidNameLenStr;
   case kUnknownMessageType:
     return kUnknownMessageTypeStr;
   case kParseError:
     return kParseErrorStr;
   default:
     return kUnknownErrorStr;
  }
}


void ProtobufCodec::defaultErrorCallback(const muduo::net::TcpConnectionPtr& conn,
                                         muduo::net::Buffer* buf,
                                         muduo::Timestamp,
                                         ErrorCode errorCode)
{
  LOG_ERROR << "ProtobufCodec::defaultErrorCallback - " << errorCodeToString(errorCode);
  if (conn && conn->connected())
  {
    conn->shutdown();
  }
}



int32_t asInt32(const char* buf)
{
  int32_t be32 = 0;
  ::memcpy(&be32, buf, sizeof(be32));
  
  return sockets::networkToHost32(be32);
}

// 注册给 QueryServer 调用，直接处理TcpConnection接受的Buffer
void ProtobufCodec::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime)
{
  while (buf->readableBytes() >= kMinMessageLen + kHeaderLen)
  {
    const int32_t len = buf->peekInt32();
    if (len > kMaxMessageLen || len < kMinMessageLen) // 消息长度不对
    {
      errorCallback_(conn, buf, receiveTime, kInvalidLength);
      break;
    }
	else if (buf->readableBytes() >= implicit_cast<size_t>(len + kHeaderLen)) // 此条消息已完整接受
    {
		ErrorCode errorCode = kNoError;
		MessagePtr message = parse(buf->peek()+kHeaderLen, len, &errorCode);
		 if (errorCode == kNoError && message)
		  {
			messageCallback_(conn, message, receiveTime); // 成功解析消息，调用QueryServer 注册的回调函数：dispatcher 的 onProtobufMessage(MessagePtr&)
			// 
			buf->retrieve(kHeaderLen+len);
		  }
		  else
		  {
			errorCallback_(conn, buf, receiveTime, errorCode);
			break;
		  }
	}
    else // 消息不完整
    {
      break;
    }
  }
}


google::protobuf::Message* ProtobufCodec::createMessage(const std::string& typeName)
{
  google::protobuf::Message* message = NULL;
  const google::protobuf::Descriptor* descriptor =
    google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
  if (descriptor)
  {
    const google::protobuf::Message* prototype =
      google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
    if (prototype)
    {
      message = prototype->New();
    }
  }
  return message;
}

//decode函数
// [buf, buf+len] 去除了 总长度len 消息
MessagePtr ProtobufCodec::parse(const char* buf, int len, ErrorCode* error)
{
  MessagePtr message;

  // check sum
  int32_t expectedCheckSum = asInt32(buf + len - kHeaderLen);
  int32_t checkSum = static_cast<int32_t>(
      ::adler32(1,
                reinterpret_cast<const Bytef*>(buf),
                static_cast<int>(len - kHeaderLen)));
  if (checkSum == expectedCheckSum)
  {
    // get message type name
    int32_t nameLen = asInt32(buf); // networkToHost32
    if (nameLen >= 2 && nameLen <= len - 2*kHeaderLen)
    {
      std::string typeName(buf + kHeaderLen, buf + kHeaderLen + nameLen - 1);
      // create message object
      message.reset(createMessage(typeName));
      if (message)
      {
        // parse from buffer
        const char* data = buf + kHeaderLen + nameLen;
        int32_t dataLen = len - nameLen - 2*kHeaderLen;
        if (message->ParseFromArray(data, dataLen))
        {
          *error = kNoError;
        }
        else
        {
          *error = kParseError;
        }
      }
      else
      {
        *error = kUnknownMessageType;
      }
    }
    else 
    {
		fprintf(stdout, "nameLen=%d\n", nameLen);
        *error = kInvalidNameLen;
		if(nameLen <2)
			fprintf(stdout, "SHORT\n");
		else if(nameLen > len-2*kHeaderLen)
			fprintf(stdout, "LONG\n");
    }
  }
  else
  {
    *error = kCheckSumError;
  }

  return message;
}
