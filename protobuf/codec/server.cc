
class QueryServer: boost::noncopyable{
public:
	QueryServer(EventLoop* loop, const InetAddress& listenAddr)
	:server_(loop, listenAddr, "QueryServer"),
	dispatcher_(boost::bind(&QueryServer::onUnknownMessage, this, _1, _2, _3)),
	//
	codec_(boost::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)) 
	// dispatcher_的onProtobufMessage() 作为 ProtobufCodec的消息处理函数，即ProtobufCodec处理完消息调用dispatcher_的onProtobufMessage()
	{
		dispatcher_.registerMessageCallback<muduo::Query>(boost::bind(&QueryServer::onQuery, this, _1, _2, _3));
		dispatcher_.registerMessageCallback<muduo::Answer>(boost::bind(&QueryServer::onAnswer, this, _1, _2, _3));
		
		server_.setConnectionCallback(boost::bind(&QueryServer::onConnection, this, _1));
		server_.setMessageCallback(boost::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
	}

	  void start()
	  {
		server_.start();
	  }

	
private:
	  void onConnection(const TcpConnectionPtr& conn)
	  {
		LOG_INFO << conn->localAddress().toIpPort() << " -> "
			<< conn->peerAddress().toIpPort() << " is "
			<< (conn->connected() ? "UP" : "DOWN");
	  }
	  
	  void onUnknownMessage(const TcpConnectionPtr& conn,
                        const MessagePtr& message,
                        Timestamp)
	  {
		LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
		conn->shutdown();
	  }
	
	  void onQuery(const muduo::net::TcpConnectionPtr& conn,
               const QueryPtr& message,
               muduo::Timestamp)
	  {
		LOG_INFO << "onQuery:\n" << message->GetTypeName() << message->DebugString();
		Answer answer;
		answer.set_id(1);
		answer.set_questioner("Chen Shuo");
		answer.set_answerer("blog.csdn.net/Solstice");
		answer.add_solution("Jump!");
		answer.add_solution("Win!");
		codec_.send(conn, answer); // 让 ProtobufCodec encode再发送

		conn->shutdown();
	  }

	  void onAnswer(const muduo::net::TcpConnectionPtr& conn,
					const AnswerPtr& message,
					muduo::Timestamp)
	  {
		LOG_INFO << "onAnswer: " << message->GetTypeName();
		conn->shutdown();
	  }
	
private:
	TcpServer server_;
	ProtobufDispatcher dispatcher_;
	ProtobufCodec codec_;
};