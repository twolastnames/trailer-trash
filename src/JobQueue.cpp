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
 * File:		JobQueue.cpp
 * Description:	a thread safe job queue
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#include "JobQueue.h"

namespace trailer {


void JobScheduler::init()
{
	sem_init(&availible_,0,0);
	pthread_mutex_init(&exist_,NULL);
	pthread_mutex_init(&wait_,NULL);
	pthread_mutex_init(&cancel_,NULL);
}

JobScheduler::~JobScheduler()
{
	sem_close(&availible_);
	pthread_mutex_destroy(&exist_);
	pthread_mutex_destroy(&wait_);
	pthread_mutex_destroy(&cancel_);
}

void JobScheduler::schedule(const Job j)
{
	{
		MutexLock l(cancel_);
		if(cancelled_.find(j) != cancelled_.end()) {
			cancelled_.erase(j);
			goto Uncancelled;
		}
	}
	{
		MutexLock le(exist_);
		if(existing_.find(j)!=existing_.end()) throw JobAlreadyScheduled(j);
		else existing_.insert(j);
		MutexLock lex(wait_);
		waiting_.push_back(j);

	}
	sem_post(&availible_);
	Uncancelled:
	listener_->onEnterWait(j);
}

class SemaphorePoster {
public:
	SemaphorePoster(sem_t* s) : sem(s) {}
	void operator ()() { sem_post(sem); }
private:
	sem_t* sem;
};

void JobScheduler::executeNext()
{
	sem_wait(&availible_);
	{
		MutexLock lwa(wait_);
		if(waiting_.empty()) throw JobsQueueClosed();
	}
	stack<Job> jobHolder;
	{
		SemaphorePoster sem(&availible_);
		NonCommittedAction<SemaphorePoster> semAction(sem);
		MutexLock lwa(wait_);
		Job& j=waiting_.front();
		jobHolder.push(j);
		waiting_.pop_front();
		semAction.commit();
	}
	// must be exception free from here to when the semaphore lock was claimed
	const Job& job=jobHolder.top();
	listener_->onDoneWait(job);
	listener_->onStartAction(job);
	{
		MutexLock l(cancel_);
		set<Job>::iterator ji=cancelled_.find(job);
		if(ji!=cancelled_.end()) {
			cancelled_.erase(ji);
			return;
		}
	}
	{
		Canceller cancel(job,this);
		NonCommittedAction<Canceller> cancelCommiter(cancel);
		job.action(*can_,job.trash);
		cancelCommiter.commit();
	}
	{
		MutexLock le(exist_);
		existing_.erase(job);
	}
	listener_->onEndAction(job);
}

void JobScheduler::close()
{
	MutexLock lwa(wait_);
	for(int i=0;i<maxThreads_;i++) sem_post(&availible_);
}

void JobScheduler::cancel(const Job j)
{
	MutexLock l(cancel_);
	if(cancelled_.find(j)==cancelled_.end()) {
		cancelled_.insert(j);
	}
}

}

