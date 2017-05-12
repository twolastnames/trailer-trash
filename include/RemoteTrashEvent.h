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
 * File:		RemoteTrashEvent.h
 * Description:	TCP implementation for can updates
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Aug 10 2010
 **********************************************************************************************/

#ifndef REMOTETRASHEVENT_H_
#define REMOTETRASHEVENT_H_

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <set>
#include <map>
#include <stack>
#include <sstream>

#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "Common.h"
#include "TrashEvent.h"
#include "EventSerialization.h"
#include "TrashCan.h"

namespace trailer {
namespace remote {

class RemoteTrashEvent;

struct BufferInfo {
	BufferInfo(char* b, const int s) : buffer(b), size(s) {}
	char* buffer;
	const int size;
	const bool operator == (const BufferInfo& other)
	{
		return other.buffer==buffer;
	}
};

struct NotConnected : public ExceptionBase {};

class Connection : public EventListener {
public:
	friend class RemoteTrashEvent;
	Connection() : events_(NULL) {}
	~Connection();
	void add(const string& trashname) {doSomething(&serialization::CanEventWriter::add,trashname);}
	void schedule(const string& trashname) {doSomething(&serialization::CanEventWriter::schedule,trashname);}
	void unschedule(const string& trashname) {doSomething(&serialization::CanEventWriter::unschedule,trashname);}
	void remove(const string& trashname) {doSomething(&serialization::CanEventWriter::remove,trashname);}
	void undefined(const char event,const string& trashname);
private:
	void doSomething(void (serialization::CanEventWriter::*duty)(const string&), const string& trashname);
	Connection& operator =(const Connection&);
	Connection(const Connection&);
	serialization::CanEventWriter writer_;
	RemoteTrashEvent* events_;
	int socket_;
};

struct RemoteTrashEventSettings {
	RemoteTrashEventSettings() : bufferSize(128), bufferPerPage(16), oneNetworkThread(true)
	{
		tv.tv_sec=0;
		tv.tv_usec=100000;
	}
	int bufferSize;
	int bufferPerPage;
	struct timeval tv;
	bool oneNetworkThread;
};

struct Availible {
	Availible(BufferInfo b, int c, int s) :
		buffer(b), base(b.buffer), count(c), sock(s) {}
	const BufferInfo buffer;
	char* base;
	int count;
	const int sock;
	const bool operator == (const Availible& other) const
	{
		return buffer.buffer==other.buffer.buffer;
	}
	const bool operator < (const Availible& other) const
	{
		return buffer.buffer<other.buffer.buffer;
	}
};

struct WriterSet {
	WriterSet() {pthread_mutex_init(&lock,NULL);}
	~WriterSet() {pthread_mutex_destroy(&lock);}
	WriterSet& operator =(const WriterSet&);
	WriterSet(const WriterSet&);
	list<Availible> availible;
	// key is socket, value is count of references (available & open)
	map<int,int> references;
	pthread_mutex_t lock;
};

struct ValueDeleter {
	void operator () (pair<int,serialization::CanEventReader*> p) {delete p.second;}
};

struct ReaderSet {
	ReaderSet()
	{
		sem_init(&availSem,0,0);
		pthread_mutex_init(&lock,NULL);
	}
	~ReaderSet()
	{
		pthread_mutex_destroy(&lock);
		sem_destroy(&availSem);
		ValueDeleter vd;
		for_each(readers.begin(),readers.end(),vd);
	}
	list<Availible> availible;
	map<int,serialization::CanEventReader*> readers;
	pthread_mutex_t lock;
	sem_t availSem;
};

struct RemoteWriter {
	virtual ~RemoteWriter() {}
	virtual void writer(const string& host, int port,Connection&)=0;
};

typedef void (RemoteTrashEvent::*ThreadRunner)();

// TODO: reap buffer buffer, not a leak but the largest buffer is always maintained
class RemoteTrashEvent : public trashevent::MultiEventDispatcher, public RemoteWriter {
public:
	RemoteTrashEvent(int bindPort, RemoteTrashEventSettings* settings);
	~RemoteTrashEvent();
	void writer(const string& host, int port,Connection&);
	/** This sends read data through listener.  This call is thread safe. */

	int boundAt() const {return bindPort_;}
	void shutdown()
	{
		shutdown_=true;
		clear();
		sem_post(&reader_.availSem);
	}
private:
	friend class Connection;
	RemoteTrashEvent& operator =(const RemoteTrashEvent&);
	RemoteTrashEvent(const RemoteTrashEvent&);
	template <typename READER, typename WRITER>
	void network();
	void readerWork();
	void writeThreadNetwork();
	void readThreadNetwork();
	void oneThreadedNetwork();
	RemoteTrashEventSettings* RESTRICT settings_;
	ReaderSet reader_;
	int bindSocket_;
	BufferInfo buffer();
	ThreadSafeQueue<BufferInfo> empties_;
	WriterSet writer_;
	ThreadSafeQueue<char*> bufferPages_;
	int bindPort_;
	stack<pthread_t> threads_;
	MemberCallerArg<RemoteTrashEvent,ThreadRunner>* threadArgs_;
	bool shutdown_;
};

struct NetworkingError {
	NetworkingError() : errornumber (errno) {}
	virtual ~NetworkingError() {}
	const int errornumber;
};

struct BindError : public NetworkingError {
	enum REASON {NO_ALLOC_SOCK, CANT_SET_OPT, CANT_BIND, CANT_LISTEN};
	BindError(REASON r) : NetworkingError(), reason(r) {}
	virtual ~BindError() {}
	const REASON reason;
};

struct CantBind : public BindError {
	CantBind(const int p) : BindError(CANT_BIND), port(p) {}
	const int port;
};

struct CantReadWrite : public NetworkingError {
	enum REASON {BAD_SELECT};
	CantReadWrite(REASON r) : NetworkingError(), reason(r) {}
	virtual ~CantReadWrite() {}
	const REASON reason;
};

struct AcceptError : public NetworkingError {};

struct ReadError : public NetworkingError {};

struct WriteError : public NetworkingError {};

struct ConnectError : public NetworkingError {
	enum REASON{NO_SOCK,NO_CONNECT,CANT_RESOLVE_HOSTNAME};
	ConnectError(const REASON r) : NetworkingError(), reason(r) {}
	const REASON reason;
};

}
}

#endif /* REMOTETRASHEVENT_H_ */
