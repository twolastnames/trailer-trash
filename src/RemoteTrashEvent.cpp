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
 * File:		RemoteTrashEvent.cpp
 * Description:	TCP implementation for can updates
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Aug 10 2010
 **********************************************************************************************/

#include "RemoteTrashEvent.h"

namespace trailer {
namespace remote {

void Connection::doSomething(void (serialization::CanEventWriter::*duty)(const string&), const string& trashname)
{
	DEBUGLOG("Connection::doSomething(begin): trashname(" << trashname << ")")
	MutexLock l(events_->writer_.lock);
	((writer_).*(duty))(trashname);
	for(;;) {
		if(writer_.availible()<=0) return;
		BufferInfo bi=events_->buffer();
		int wrote=writer_.output(bi.buffer,bi.size);
		events_->writer_.availible.push_back(Availible(bi,wrote,socket_));
		DEBUGLOG("Connection::doSomething(wrote): trashname(" << string(bi.buffer,wrote) << ")")
		if(events_->writer_.references.find(socket_)==events_->writer_.references.end()) {
			events_->writer_.references[socket_]=0;
		}
		events_->writer_.references[socket_]++;
	}
}

void Connection::undefined(const char event,const string& trashname)
{
	writer_.undefined(event,trashname);
}

Connection::~Connection()
{
	//TODO: clean writing socket up on disconnect
}

RemoteTrashEvent::RemoteTrashEvent(int bindPort, RemoteTrashEventSettings* settings)
	:settings_(settings), bindPort_(bindPort), threadArgs_(NULL), shutdown_(false)
{
	if((bindSocket_ = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		throw BindError(BindError::NO_ALLOC_SOCK);
	}
	static const int yes=1;
	if(setsockopt(bindSocket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		close(bindSocket_);
		throw BindError(BindError::NO_ALLOC_SOCK);
	}
	{
		struct sockaddr_in a;
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = INADDR_ANY;
		a.sin_port = htons(bindPort);
		memset(&(a.sin_zero), '\0', 8);
		if(bind(bindSocket_, (struct sockaddr *)&a, sizeof(a)) == -1) {
			close(bindSocket_);
			throw CantBind(bindPort);
		}
	}
	if(listen(bindSocket_, 16) == -1) {
		close(bindSocket_);
		throw BindError(BindError::CANT_LISTEN);
	}
	int onArg=0;
	if(settings_->oneNetworkThread) {
		threadArgs_=new MemberCallerArg<RemoteTrashEvent,ThreadRunner>[2];
		threadArgs_[onArg].object=this;
		threadArgs_[onArg].member=&RemoteTrashEvent::oneThreadedNetwork;
		threads_.push(0);
		pthread_create(&threads_.top(),NULL,&threadMemberCaller<RemoteTrashEvent,ThreadRunner>,&threadArgs_[onArg]);
		onArg++;
	} else {
		threadArgs_=new MemberCallerArg<RemoteTrashEvent,ThreadRunner>[3];
		threadArgs_[onArg].object=this;
		threadArgs_[onArg].member=&RemoteTrashEvent::writeThreadNetwork;
		threads_.push(0);
		pthread_create(&threads_.top(),NULL,&threadMemberCaller<RemoteTrashEvent,ThreadRunner>,&threadArgs_[onArg]);
		onArg++;
		threadArgs_[onArg].object=this;
		threadArgs_[onArg].member=&RemoteTrashEvent::readThreadNetwork;
		threads_.push(0);
		pthread_create(&threads_.top(),NULL,&threadMemberCaller<RemoteTrashEvent,ThreadRunner>,&threadArgs_[onArg]);
		onArg++;
	}
	threadArgs_[onArg].object=this;
	threadArgs_[onArg].member=&RemoteTrashEvent::readerWork;
	threads_.push(0);
	pthread_create(&threads_.top(),NULL,&threadMemberCaller<RemoteTrashEvent,ThreadRunner>,&threadArgs_[onArg]);
}

RemoteTrashEvent::~RemoteTrashEvent()
{
	shutdown();
	while(!threads_.empty()) {
		pthread_join(threads_.top(),NULL);
		threads_.pop();
	}
	if (threadArgs_!=NULL) delete [] threadArgs_;
}

void RemoteTrashEvent::writer(const string& host, int port,Connection& con)
{
	struct addrinfo hints;
	struct addrinfo *ai;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	{
		ostringstream p;
		p << port;
		getaddrinfo(host.c_str(), p.str().c_str(), &hints, &ai);
	}
	if ((con.socket_ = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) < 0) {
		throw ConnectError(ConnectError::NO_SOCK);
	}
	if (connect(con.socket_,reinterpret_cast<struct sockaddr *>(ai->ai_addr), sizeof(*ai->ai_addr)) < 0) {
		close(con.socket_);
		freeaddrinfo(ai);
		throw ConnectError(ConnectError::NO_CONNECT);
	}
	freeaddrinfo(ai);
	con.events_=this;
	{
		MutexLock l(con.events_->writer_.lock);
		con.events_->writer_.references[con.socket_]=1;
	}
}

void RemoteTrashEvent::readerWork()
{
	for(;;) {
		sem_wait(&reader_.availSem);
		{
			if(shutdown_) return;
			Availible* avail=NULL;
			list<Availible>::iterator i;
			{
				MutexLock l(reader_.lock);
				avail=&reader_.availible.front();
			}
			int tries=3;
			for(;;) {
				try {
					(*reader_.readers[avail->sock])(avail->base,avail->count);
					break;
				} catch(const TrashInfoReadError& e) {
					if (tries<=0) throw e;
					sleep(1);
				}
			}
			{
				MutexLock l(reader_.lock);
				reader_.availible.pop_front();
			}
		}
	}
}

struct WriteDescriptor {
	WriteDescriptor() : max(-1) {FD_ZERO(&set);}
	void operator () (Availible& p)
	{
		FD_SET(p.sock,&set);
		if(p.sock>max) max=p.sock;
		DEBUGLOG("WriteDescriptor::operator():  " << string(p.base,p.count))
		availible.push(p);
	}
	fd_set set;
	int max;
	ThreadSafeQueue<Availible> availible;
};

struct ReadDescriptor {
	ReadDescriptor(int bind) : bound(bind), max(bind) {FD_ZERO(&set);FD_SET(bound,&set);}
	void operator () (const pair<int,serialization::CanEventReader*>& p)
	{
		FD_SET(p.first,&set);
		if(p.first>max) max=p.first;
		existing.push_back(p);
	}
	int bound;
	fd_set set;
	int max;
	// descriptor, reader pairs
	list<pair<int,serialization::CanEventReader*> > existing;
};

struct DescriptorDeleteViewer {
	void operator () (const pair<int,int>& p)
	{
		if(p.second<1) {
			toRemove.push_back(p.first);
		}
	}
	list<int> toRemove;
};

struct DescriptorDeleter {
	DescriptorDeleter(map<int,int>* d) : descriptors(d) {}
	void operator () (const int& i)
	{
		//TODO: do cleanup (close socket)
		descriptors->erase(i);
	}
	map<int,int>* descriptors;
};

struct AvailibleListRemover {
	AvailibleListRemover(map<int,int>* r,ThreadSafeQueue<BufferInfo>* e) : empties(e),references(r) {}
	bool operator () (Availible& a)
	{
		for(list<Availible>::iterator i=toclean.begin();i!=toclean.end();i++) {
			if(a==*i) { goto Found; }
		}
		return false;
		Found:
		if(--(*references)[a.sock] <= 0) {
			references->erase(a.sock);
			// TODO: do cleanup (close socket)
		}
		empties->push(a.buffer);
		return true;
	}
	ThreadSafeQueue<BufferInfo>* empties;
	map<int,int>* references;
	list<Availible> toclean;
};

struct WriteAgent {
	WriteDescriptor writeDesc;
	void init()
	{
		{
			MutexLock l(set->lock);
			for_each(set->availible.begin(),set->availible.end(),
					CallableProxy<WriteDescriptor>(writeDesc));
		}
		{
			MutexLock l(set->lock);
			DescriptorDeleteViewer remover;
			for_each(set->references.begin(),set->references.end(),
					CallableProxy<DescriptorDeleteViewer>(remover));
			DescriptorDeleter dd(&set->references);
			for_each(remover.toRemove.begin(),remover.toRemove.end(),dd);
		}
	}
	void work()
	{
		AvailibleListRemover writeRemover(&set->references,empties);
		try {
			while(true) {
				Availible a=writeDesc.availible.pop();
				#ifdef DEBUG
					{
						string tolog;
						for(int i=0;i<a.count;i++) tolog += a.base[i];
						DEBUGLOG("WriteAgent::work(popped): " << a.count << " : " << tolog)
					}
				#endif
				int availible;
				{
					fd_set writers;
					FD_ZERO(&writers);
					FD_SET(a.sock,&writers);
					availible=select(a.sock+1, NULL, &writers, NULL, &tv);
				}
				if(availible == -1) {
					throw CantReadWrite(CantReadWrite::BAD_SELECT);
				}
				if(availible>0) {
					int sent;
					if((sent=send(a.sock, a.base, a.count, 0)) < 0) {
						throw WriteError();
					}
					#ifdef DEBUG
						{
							string tolog;
							for(int i=0;i<a.count;i++) tolog += a.base[i];
							DEBUGLOG("WriteAgent::work(wrote): " << a.count << " : " << tolog)
						}
					#endif
					if(a.count==sent) {
						writeRemover.toclean.push_back(a);
					} else {
						a.base=a.base + (sizeof(char*) * sent);
						a.count=a.count - sent;
						writeDesc.availible.push_front(a);
					}
				} else {
					writeDesc.availible.push_front(a);
				}
			}
		} catch (const NoEntryPresent& e) {
			list<Availible>::iterator i=writeRemover.toclean.begin();
			DEBUGLOG("WriteAgent::work(cleanSize): " << writeRemover.toclean.size())
			while(i!=writeRemover.toclean.end()) {
				DEBUGLOG("WriteAgent::work(eraseChecking): " << string(i->base,i->count))
				set->availible.remove(*i++);
			}
		}
	}
	fd_set* descriptors() {return &writeDesc.set;}
	int maxDescriptor() {return writeDesc.max;}
	WriterSet* set;
	ThreadSafeQueue<BufferInfo>* empties;
	struct timeval tv;
};

struct DummyWriteAgent {
	void init() {}
	void work() {}
	fd_set* descriptors() {return NULL;}
	int maxDescriptor() {return 0;}
	WriterSet* set;
	ThreadSafeQueue<BufferInfo>* empties;
	struct timeval tv;
};

struct ReaderRearranger {
	ReaderRearranger(const int l) : looking(l) {}
	bool operator () (Availible& v) { return v.sock==looking; }
	const int looking;
};

struct ReadAgent {
	ReadAgent(int bindSocket) : readDesc(bindSocket) {}
	ReadDescriptor readDesc;
	void init()
	{
		MutexLock l(rs->lock);
		for_each(rs->readers.begin(),rs->readers.end(),CallableProxy<ReadDescriptor>(readDesc));
	}
	void work()
	{
		// accepts
		{
			int availible;
			{
				fd_set acceptor;
				FD_ZERO(&acceptor);
				FD_SET(readDesc.bound,&acceptor);
				availible=select(readDesc.bound+1, &acceptor, NULL, NULL, &tv);
			}
			if(availible == -1) {
				throw CantReadWrite(CantReadWrite::BAD_SELECT);
			}
			while(0<availible--) {
				int newSock;
				{
					struct sockaddr_in clientAddress;
					socklen_t addressSize = sizeof(clientAddress);
					if((newSock = accept(readDesc.bound, (struct sockaddr *)&clientAddress, &addressSize)) < 0) {
						AcceptError ae=AcceptError();
						throw ae;
					}
				}
				{
					MutexLock l(rs->lock);
					rs->readers.insert(make_pair(newSock,new serialization::CanEventReader(listener)));
				}
			}
		}
		// reads
		bool found;
		do {
			found=false;
			for(list<pair<int,serialization::CanEventReader*> >::iterator it=readDesc.existing.begin();
				it!=readDesc.existing.end();it++) {
				BufferInfo bi=(events->*buffer)();
				{
					fd_set reader;
					FD_ZERO(&reader);
					FD_SET(it->first,&reader);
					if(1>select(it->first+1, &reader, NULL, NULL, &tv)) {continue;}
				}
				int readCount;
				if((readCount = recv(it->first, bi.buffer, bi.size, 0)) <= 0) {
					if(readCount == 0) {
						MutexLock l(rs->lock);
						// TODO: cleanup
						continue;
					} else {
						throw ReadError();
					}
				}
				found=true;
				{
					MutexLock l(rs->lock);
					rs->availible.push_back(Availible(bi,readCount,it->first));
					sem_post(&rs->availSem);
				}
			}
		} while (found);
	}
	fd_set* descriptors() {return &readDesc.set;}
	int maxDescriptor() {return readDesc.max;}
	ReaderSet* rs;
	BufferInfo (RemoteTrashEvent::*buffer)();
	RemoteTrashEvent* events;
	EventListener* listener;
	struct timeval tv;
};

struct DummyReadAgent {
	DummyReadAgent(int bindSocket) {}
	void init() {}
	void work() {}
	fd_set* descriptors() {return NULL;}
	int maxDescriptor() {return 0;}
	ReaderSet* rs;
	BufferInfo (RemoteTrashEvent::*buffer)();
	RemoteTrashEvent* events;
	EventListener* listener;
	struct timeval tv;
};

template <typename READER, typename WRITER>
void RemoteTrashEvent::network()
{
	WRITER writer;
	writer.set=&writer_;
	writer.empties=&empties_;
	writer.tv=settings_->tv;
	writer.init();
	READER reader(bindSocket_);
	reader.buffer=&RemoteTrashEvent::buffer;
	reader.events=this;
	reader.listener=this;
	reader.rs=&reader_;
	reader.tv=settings_->tv;
	reader.init();
	usleep(200000); // TODO: make this configurable
	int max=writer.maxDescriptor()>reader.maxDescriptor() ? writer.maxDescriptor() : reader.maxDescriptor();
	if(select(max+1, reader.descriptors(), writer.descriptors(), NULL, &settings_->tv) == -1) {
		throw CantReadWrite(CantReadWrite::BAD_SELECT);
	}
	reader.work();
	writer.work();
}

void RemoteTrashEvent::writeThreadNetwork()
{
	for(;;) {
		{
			MutexLock l(writer_.lock);
			if(shutdown_ && !hasListeners() && writer_.availible.empty()) return;
		}
		try {
			network<DummyReadAgent, WriteAgent>();
		} catch (const CantReadWrite& e) {}
	}
}

void RemoteTrashEvent::readThreadNetwork()
{
	for(;;) {
		{
			MutexLock l(writer_.lock);
			if(shutdown_ && !hasListeners() && writer_.availible.empty()) return;
		}
		try {
			network<ReadAgent, DummyWriteAgent>();
		} catch (const CantReadWrite& e) {}
	}
}

void RemoteTrashEvent::oneThreadedNetwork()
{
	for(;;) {
		{
			MutexLock l(writer_.lock);
			if(shutdown_ && !hasListeners() && writer_.availible.empty()) return;
		}
		try {
			network<ReadAgent, WriteAgent>();
		} catch (const CantReadWrite& e) {}
	}

}

BufferInfo RemoteTrashEvent::buffer()
{
	for(;;)
	try {
		return empties_.pop();
	} catch (const NoEntryPresent& e) {
		char* position=new char[settings_->bufferPerPage*settings_->bufferSize];
		bufferPages_.push(position);
		const char* pastLast=position + sizeof(char)*settings_->bufferPerPage*settings_->bufferSize;
		for(;position!=pastLast;position+=sizeof(char)*settings_->bufferSize) {
			empties_.push(BufferInfo(position,settings_->bufferSize));
		}
	}
}

}
}
