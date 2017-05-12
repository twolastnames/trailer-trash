/* *****************************************************************************************
 * Copyright 2010 Trailer Trash. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY TRAILER TRASH ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL TRAILER TRASH OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Trailer Trash.
 *
 *********************************************************************************************/

/**********************************************************************************************
 * File:		JobQueue.h
 * Description:	a thread safe job queue
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#ifndef JOBQUEUE_H_
#define JOBQUEUE_H_

#include <list>
#include <queue>
#include <set>
#include <stack>

#include <semaphore.h>
#include <errno.h>
#include <time.h>

#include "Common.h"
#include "TrashCan.h"
#include "TrashEvent.h"

using namespace std;

namespace trailer {

struct CantReadSystemTime : public ExceptionBase {};

struct Job {
	Job(const candelegate::delegate action, const string& trash) :
		action(action), trash(trash) {}
	const candelegate::delegate action;
	/**
	filename for trash, trash name otherwise
	*/
	const string trash;
	const bool operator () (const Job& a, const Job& b) const
	{
		return a<b;
	}
	const bool operator < (const Job& b) const
	{
		if(action!=b.action) return action<b.action;
		return trash<b.trash;
	}
};

struct JobFunctor {
	bool operator () (const Job& a, const Job& b)
	{
		if(a.action!=b.action) return a.action < b.action;
		return a.trash < b.trash;
	}
};

struct FileCollision : public ExceptionBase {
	FileCollision(string& f) : filename(f) {}
	const string filename;
};

struct JobListener {
	virtual ~JobListener() {}
	/** ordered first to do to last */
	virtual void onEnterWait(const Job&)=0;
	virtual void onDoneWait(const Job&)=0;
	virtual void onStartAction(const Job&)=0;
	virtual void onEndAction(const Job&)=0;
	virtual void onCancel(const Job&)=0;
};

/**
 * common ones EINVAL,EDEADLK,EINTR
 */
struct SemaphoreError : public ExceptionBase {
	SemaphoreError(const int& num) :errorno(num) {}
	const int errorno;
};

struct DummyJobListener : public JobListener {
	void onEnterWait(const Job&) {}
	void onDoneWait(const Job&) {}
	void onStartAction(const Job&) {}
	void onEndAction(const Job&) {}
	void onCancel(const Job&) {}
};

/**
* Likely thrown at event similar to a program trying to exit
*/
struct JobsQueueClosed : public ExceptionBase {};

struct DefaultWaitTime : public timespec {
	DefaultWaitTime() {tv_nsec=100000000;tv_sec=0;}
};

struct JobAlreadyScheduled : public ExceptionBase {
	JobAlreadyScheduled(const Job& j) : job(j) {}
	const Job job;
};

class Canceller;

/**
* note: very thread safe
*/
class JobScheduler {
public:
	JobScheduler(TrashCan* can,JobListener* l,const int maxThreads) :
		maxThreads_(maxThreads), can_(can), listener_(l) {init();}
	virtual ~JobScheduler();
	/**
	@throws JobsQueueClosed if calling thread should exit
	*/
	void executeNext();
	void schedule(const Job);
	void cancel(const Job);
	void close();
	const int threadCount() {return maxThreads_;}
private:
	friend class Canceller;
	void init();
	JobScheduler& operator = (const JobScheduler&); // non-copyable
	JobScheduler(const JobScheduler&); // non-copyable
	const int maxThreads_;
	pthread_mutex_t wait_; // LOCK #2
	list<Job> waiting_;
	pthread_mutex_t exist_;  // LOCK #1
	set<Job,JobFunctor> existing_;
	TrashCan* can_;
	JobListener* listener_;
	sem_t availible_;
	timespec donePollTime_;
	pthread_mutex_t cancel_;  // LOCK #3
	set<Job> cancelled_;
};

class Canceller {
public:
	Canceller(const Job j,JobScheduler* s) : job(j), scheduler(s) {}
	void operator ()()
	{
		scheduler->listener_->onCancel(job);
	}
private:
	const Job job;
	JobScheduler* scheduler;
};

}

#endif
