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
 * File:		RemoteDirectoryUpdater.h
 * Description:	for updating can information with other instances
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Aug 8 2010
 **********************************************************************************************/

#ifndef REMOTEDIRECTORYUPDATER_H_
#define REMOTEDIRECTORYUPDATER_H_

#include "time.h"

#include <stack>
#include <map>
#include <set>

#include <unistd.h>

#include "CanSet.h"
#include "EventSerialization.h"
#include "RemoteTrashEvent.h"

namespace trailer {
namespace remote {

struct RemoteInfo {
	RemoteInfo(const string& sv, const string& p, const string& h, const int prt,const string& d)
		: serverVersion(sv), protocol(p), hostname(h), port(prt), directory(d) {}
	const string serverVersion;
	const string protocol;
	const string hostname;
	const int port;
	const string directory;
};

struct BadRemoteInfo {
	enum REASON {STREAM,VERS, PROTOCOL, HOSTNAME, PORT, DIRECTORY};
	BadRemoteInfo(const REASON r) : reason(r) {}
	const REASON reason;
};

/** @throws BadRemoteInfo */
const RemoteInfo inputRemoteInfo(istream& in);

void outputRemoteInfo(ostream&, const RemoteInfo& ri);

/**
 * BindNotifier puts files near all trash directories to let other machines/processes
 * know of a listening binding
 */
class BindNotifier : public CanDirectoryListener {
public:
	BindNotifier(remote::RemoteTrashEvent& e);
	~BindNotifier();
	void home(const string& directory) {reportBind(directory,true);}
	void top(const string& directory) {reportBind(directory,false);}
private:
	BindNotifier& operator = (BindNotifier&);
	BindNotifier(BindNotifier&);
	void reportBind(const string& directory, bool isHome);
	RemoteTrashEvent& network_;
	pthread_mutex_t lock_;
	list<string> deletableFiles_;
};

struct ConnectionKey {
	ConnectionKey(const string& b, const string& h, const int p) :
		basedir(b), hostname(h), port(p) {}
	const string basedir;
	const string hostname;
	const int port;
	const bool operator < (const ConnectionKey& ck) const
	{
		if(hostname!=ck.hostname) return hostname<ck.hostname;
		if(basedir!=ck.basedir) return basedir<ck.basedir;
		return port<ck.port;
	}
};

struct ConnectionMap : public map<ConnectionKey,remote::Connection*> {
	ConnectionMap() {}
	~ConnectionMap()
	{
		for(map<ConnectionKey,remote::Connection*>::iterator i=begin();i!=end();i++) {
			delete i->second;
		}
	}
	ConnectionMap& operator =(ConnectionMap&);
	ConnectionMap(ConnectionMap&);
};

/**
 *
 */
class ChangeNotifier : public trashevent::EventListener {
public:
	ChangeNotifier(remote::RemoteTrashEvent& e, CanDirectoryListenerExecuter& c) :
		network_(e), cans_(c) {}
	void add(const string& trashname) {handleEvent(trashname,&trashevent::EventListener::add);}
	void schedule(const string& trashname)
	{
		if(scheduled_.find(trashname)!=scheduled_.end()) {
			scheduled_.insert(trashname);
		}
		handleEvent(trashname,&trashevent::EventListener::schedule);
	}
	void unschedule(const string& trashname)
	{
		ensureNotScheduled(trashname);
		handleEvent(trashname,&trashevent::EventListener::unschedule);
	}
	void remove(const string& trashname)
	{
		ensureNotScheduled(trashname);
		handleEvent(trashname,&trashevent::EventListener::remove);
	}
	void undefined(const char event,const string& trashname) {}
private:
	void handleEvent(const string& trashname,trashevent::ListenerMember m);
	void updateConnectionMap(const string& basdir);
	void ensureNotScheduled(const string& trashname)
	{
		if(scheduled_.find(trashname)!=scheduled_.end()) {
			scheduled_.erase(trashname);
		}
	}
	remote::RemoteTrashEvent& network_;
	CanDirectoryListenerExecuter& cans_;
	ConnectionMap connections_;
	// directory keyed
	map<string,clock_t> lastLooked_;
	set<string> scheduled_;
};

}
}

#endif /* REMOTEDIRECTORYUPDATER_H_ */
