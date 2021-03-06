#ifndef TIMERQUEUE_H_
#define TIMERQUEUE_H_

#include"reactor.muduo/Channel.h"
#include"common/noncopyable.h"

#include<set>
#include<vector>


class EventLoop;
class TimerId;
class Timer;

class TimerQueue : public noncopyable {
public:
	typedef std::function<void(void)> TimerCallback;

	explicit TimerQueue(EventLoop* loop);
	~TimerQueue();

	TimerId addTimer(const TimerCallback& cb, Timestamp when, double interval);

	void cancel(TimerId timerId);

	int timerfd() const { return timerfd_;  }

private:
	//typedef std::unique_ptr<Timer> TimerPtr;
	typedef std::pair<Timestamp, Timer*> Entry; 
	typedef std::set<Entry> TimerList;

	typedef std::pair<Timer*, int> ActiveTimer; // sequence，TimerId
	typedef std::set<ActiveTimer> ActiveTimerSet;

	void addTimerInLoop(Timer* timer);
	void cancelInLoop(TimerId timer);
	bool insert(Timer*);
	void getExpired(Timestamp now);
	void handleRead(Timestamp pollReturnTime);
	//void reset(const std::vector<Entry>& expired, Timestamp now);
	void reset(Timestamp now);

	EventLoop * loop_;
	const int timerfd_;
	Channel timerfdChannel_;
	TimerList timers_; // 所有定时器

	bool callingExpiredTimers_;
	std::vector<Entry> expired_;

	ActiveTimerSet activeTimers_; // 包含有效的timer*，未到期的timer，和TimerList包含的Timer列表相同
	// timers_按到期时间排序，activeTimers_按timer指针排序
	
	ActiveTimerSet cancelingTimers_; // 取消了的Timer
};

#endif 