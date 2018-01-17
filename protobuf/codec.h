


#ifndef PROTOBUF_CODEC_H
#define PROTOBUF_CODEC_H

#include<functional>


/*
ProtobufCodec负责decode和encode：
TcpConnection::onMessage()调用ProtobufCodec的 messageCallback_,
ProtobufCodec解析好message，调用ProtobufDispatcher的 回调函数，
ProtobufDispatcher的回调函数根据消息类型 调用 用户注册的回调函数，
ProtobufCodec负责encode，调用TcpConnection::send()
*/

// struct ProtobufTransportFormat __attribute__ ((__packed__))
// {
//   int32_t  len;
//   int32_t  nameLen;
//   char     typeName[nameLen];
//   char     protobufData[len-nameLen-8];
//   int32_t  checkSum; // adler32 of nameLen, typeName and protobufData
// }


typedef boost::shared_ptr<google::protobuf::Message> MesagePtr;



class ProtobufCodec{
public:
	enum ErrorCode
  {
    kNoError = 0,
    kInvalidLength,
    kCheckSumError,
    kInvalidNameLen,
    kUnknownMessageType,
    kParseError,
  };

  typedef boost::function<void ()> ProtobufMessageCallback;
  typedef boost::function<void ()> ErrorCallback;
  
  typedef std::function<void ()> ProtobufMessageCallback;
  typedef std::function<void ()> ErrorCallback;

  explicit ProtobufCodec(const ProtobufMessageCallback& messageCb)
    : messageCallback_(messageCb),
      errorCallback_(defaultErrorCallback)
  {
  }

  ProtobufCodec(const ProtobufMessageCallback& messageCb, const ErrorCallback& errorCb)
    : messageCallback_(messageCb),
      errorCallback_(errorCb)
  {
  }

  void onMessage(const muduo::net::TcpConnectionPtr& conn, 
  				muduo::net::Buffer* buf, 
  				muduo::Timestamp receiveTime);//对Buffer类型参数decode

  void send(const muduo::net::TcpConnection& conn, const google::protobuf::Message& message)
  {

  }



private:

	ProtobufMessageCallback messageCallback_;
	ErrorCallback errorCallback_;

  const static int kHeaderLen = sizeof(int32_t);
  const static int kMinMessageLen = 2*kHeaderLen + 2; // nameLen + typeName + checkSum
  const static int kMaxMessageLen = 64*1024*1024; // same as codec_stream.h kDefaultTotalBytesLimit
};

#endif