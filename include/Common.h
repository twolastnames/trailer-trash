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
 * File:		Common.h
 * Description:	generic routines
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#ifndef COMMON_H_
#define COMMON_H_

#define RESTRICT __restrict

#include <time.h>

#include <iostream>
#include <list>
#include <queue>

#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "config.h"

using namespace std;

namespace trailer {

template <typename TYPE> inline void deleter(TYPE* todelete) {delete todelete;}

struct ExceptionBase {virtual ~ExceptionBase() {}};

struct InternalException : public ExceptionBase {
	InternalException(const string& e) : error(e) {}
	const string error;
};

class MutexLock {
public:
	MutexLock(pthread_mutex_t& m) {pthread_mutex_lock(mutex_=&m);}
	~MutexLock() {pthread_mutex_unlock(mutex_);}
private:
	MutexLock(const MutexLock&);
	MutexLock& operator = (const MutexLock&);
	pthread_mutex_t* mutex_;
};

template <typename Object, typename Member>
struct MemberCallerArg {
	Object* object;
	Member member;
};

template <typename Object, typename Member>
inline void* threadMemberCaller(void* v)
{
	MemberCallerArg<Object,Member>* arg=reinterpret_cast<MemberCallerArg<Object,Member>*>(v);
	(arg->object->*arg->member)();
	return v;
}

struct NoEntryPresent : public ExceptionBase {};

template <typename SomethingToCopy>
class ThreadSafeQueue {
public:
	ThreadSafeQueue() {pthread_mutex_init(&m_,NULL);}
	~ThreadSafeQueue() {pthread_mutex_destroy(&m_);}
	void push(const SomethingToCopy& d) {MutexLock l(m_);q_.push_back(d);}
	void push_front(const SomethingToCopy& d) {MutexLock l(m_);q_.push_front(d);}
	SomethingToCopy pop()
	{
		MutexLock l(m_);
		if(q_.empty()) throw NoEntryPresent();
		SomethingToCopy p=q_.front();
		q_.pop_front();
		return p;
	}
	size_t size() {MutexLock l(m_);return q_.size();}
private:
	ThreadSafeQueue(const ThreadSafeQueue&);
	ThreadSafeQueue& operator =(const ThreadSafeQueue&);
	pthread_mutex_t m_;
	deque<SomethingToCopy> q_;
};

template <typename Noncopied>
class CallableProxy {
public:
	virtual ~CallableProxy() {}
	CallableProxy(Noncopied& nc) : noncopied_(nc) {}
	template <typename Passed>
	void operator () (Passed& p) {noncopied_(p);}
	template <typename Passed1,typename Passed2>
	void operator () (Passed1& p1,Passed2& p2) {noncopied_(p1,p2);}
private:
	Noncopied& noncopied_;
};

template <typename OfInterest>
struct PointerFinder {
	const bool operator () (const OfInterest& a,const OfInterest& b)
	{
		return &a<&b;
	}
};

template <typename CALLABLE>
class NonCommittedAction {
public:
	NonCommittedAction(CALLABLE& todo) : commited_(false), todo_(todo) {}
	~NonCommittedAction() {if(!commited_) todo_();}
	void commit() {commited_=true;}
private:
	NonCommittedAction(const NonCommittedAction<CALLABLE>&);
	void operator () (const NonCommittedAction<CALLABLE>&);
	bool commited_;
	CALLABLE& todo_;
};

struct VerboseListener {
	virtual ~VerboseListener() {}
	virtual void sayVerbose(string& toSay)=0;
};

struct CantMakeDirectory : public ExceptionBase {
	CantMakeDirectory(const string d) : directory(d) {}
	const string directory;
};

inline void ensureDirectory(const mode_t permission, const string& want)
{
	list<string> neededDirs;
	// deduce directories that need to be created for files directory
	for(size_t i=want.size();i>=0;i--) {
		string dir;
		if(want.size()>0 && i==0 && want.at(0)=='/') {
			break;
		} else if(i==want.size()-1) {
			dir=want;
		} else if(want.size()!=i && want.at(i)=='/') {
			dir=want.substr(0,i);
		} else {
			continue;
		}
		{
			struct stat st;
			if(stat(dir.c_str(),&st)==0) break;
			else neededDirs.push_front(dir);
		}
	}
	// generate needed directories
	for(list<string>::iterator i=neededDirs.begin();i!=neededDirs.end();i++) {
		if(mkdir(i->c_str(),00)==-1) {
			throw CantMakeDirectory(*i);
		}
		if(chmod(i->c_str(),permission)!=0) {
			throw CantMakeDirectory(*i);
		}
	}
}

/**
 * for sorting string pointers in an STL container
 */
struct StringPointerFunctor {
	const bool operator ()(const string* const & a,const string* const & b)
	{
		return *a < *b;
	}
};

struct MessageListener {
	virtual ~MessageListener() {}
	virtual void operator() (const string& message)=0;
};

inline string stripTrailingFileSeprator(const string& s)
{
	return s.size()>0 && s.at(s.size()-1)=='/' ?
			s.substr(0,s.size()-1):s;
}

struct DataListener {
	virtual ~DataListener() {}
	virtual void onData(const char* beginning, size_t length)=0;
};

#ifdef DEBUG
#define DEBUGLOG( log ) \
{ \
	time_t raw; \
	struct tm* ti; \
	time(&raw); \
	ti=localtime(&raw); \
	ofstream o("/tmp/trailer-debug.log",std::ios_base::app); \
	o << asctime(ti) << ": " << log << endl; \
	o.flush(); \
}
#else
#define DEBUGLOG( log )
#endif

}


#endif /* COMMON_H_ */
