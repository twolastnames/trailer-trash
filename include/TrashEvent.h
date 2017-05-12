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
 * File:		TrashEvent.h
 * Description:	interfaces for sharing trash events
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Aug 6 2010
 **********************************************************************************************/

#ifndef TRASHEVENT_H_
#define TRASHEVENT_H_

#include <iostream>
#include <algorithm>
#include <set>

using namespace std;

namespace trailer {
namespace trashevent {

struct EventListener {
	virtual ~EventListener() {}
	virtual void add(const string& trashname)=0;
	virtual void schedule(const string& trashname)=0;
	virtual void unschedule(const string& trashname)=0;
	virtual void remove(const string& trashname)=0;
	virtual void undefined(const char event, const string& trashname)=0;
};

struct DummyEventListener : public EventListener {
	void add(const string& trashname) {}
	void schedule(const string& trashname) {}
	void unschedule(const string& trashname) {}
	void remove(const string& trashname) {}
	void undefined(const char event, const string& trashname) {}
};

typedef void (EventListener::*ListenerMember)(const string& trashname);

struct MultiEventListenerHelper;

class MultiEventDispatcher : protected EventListener {
public:
	MultiEventDispatcher() {pthread_mutex_init(&lock_,NULL);}
	virtual ~MultiEventDispatcher() {pthread_mutex_destroy(&lock_);}
	virtual void addListener(EventListener* l)
	{
		MutexLock mut(lock_);
		if(listeners_.find(l)==listeners_.end()) listeners_.insert(l);
	}
	virtual void removeListener(EventListener* l)
	{
		MutexLock mut(lock_);
		if(listeners_.find(l)!=listeners_.end()) listeners_.erase(l);
	}
	bool hasListeners() {MutexLock mut(lock_);return !listeners_.empty();}
	void clear() {listeners_.clear();}
protected:
	void add(const string& trashname);
	void schedule(const string& trashname);
	void unschedule(const string& trashname);
	void remove(const string& trashname);
	void undefined(const char event,const string& trashname)
	{
		MutexLock l(lock_);
		for(set<EventListener*>::iterator i=listeners_.begin();i!=listeners_.end();i++) {
			(*i)->undefined(event,trashname);
		}
	}
private:
	friend class MultiEventListenerHelper;
	MultiEventDispatcher& operator = (const MultiEventDispatcher&);
	MultiEventDispatcher(const MultiEventDispatcher&);
	set<EventListener*> listeners_;
	pthread_mutex_t lock_;
};

struct MultiEventListenerHelper {
	MultiEventListenerHelper(const string& t, ListenerMember m)
		: trashname(t), method(m) {}
	void operator () (EventListener* l)
	{
		(l->* method)(trashname);
	}
	const string& trashname;
	ListenerMember method;
	static void execute(MultiEventDispatcher* l, const string& t, ListenerMember m)
	{
		MutexLock mut(l->lock_);
		for_each(l->listeners_.begin(),l->listeners_.end(),MultiEventListenerHelper(t,m));
	}
};

inline void MultiEventDispatcher::add(const string& trashname)
	{MultiEventListenerHelper::execute(this,trashname,&EventListener::add);}
inline void MultiEventDispatcher::schedule(const string& trashname)
	{MultiEventListenerHelper::execute(this,trashname,&EventListener::schedule);}
inline void MultiEventDispatcher::unschedule(const string& trashname)
	{MultiEventListenerHelper::execute(this,trashname,&EventListener::unschedule);}
inline void MultiEventDispatcher::remove(const string& trashname)
	{MultiEventListenerHelper::execute(this,trashname,&EventListener::remove);}

}
}

#endif /* TRASHEVENT_H_ */
