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
 * File:		RemoteDirectoryUpdater.cpp
 * Description:	for communication between applications (can update information)
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Aug 8 2010
 **********************************************************************************************/

#include "RemoteDirectoryUpdater.h"

namespace trailer {
namespace remote {

const size_t hostsize=128;

const RemoteInfo inputRemoteInfo(istream& in)
{
	string hostname;
	string port;
	string version;
	string protocol;
	string directory;
	string line;
	if(in.bad()) {throw BadRemoteInfo(BadRemoteInfo::STREAM);}
	getline(in,version);
	if(in.bad()) {throw BadRemoteInfo(BadRemoteInfo::VERS);}
	getline(in,protocol);
	if(in.bad()) {throw BadRemoteInfo(BadRemoteInfo::PROTOCOL);}
	getline(in,hostname);
	if(in.bad()) {throw BadRemoteInfo(BadRemoteInfo::HOSTNAME);}
	getline(in,port);
	if(in.bad()) {throw BadRemoteInfo(BadRemoteInfo::PORT);}
	getline(in,directory);
	return RemoteInfo(version,protocol,hostname,atoi(port.c_str()),directory);
}

void outputRemoteInfo(ostream& out, const RemoteInfo& ri)
{
	out << ri.serverVersion << endl;
	out << ri.protocol << endl;
	out << ri.hostname << endl;
	out << ri.port << endl;
	out << ri.directory << endl;
}

BindNotifier::BindNotifier(remote::RemoteTrashEvent& e) : network_(e)
{
	pthread_mutex_init(&lock_,NULL);
}

void deleteFile(const string& f) {unlink(f.c_str());}

BindNotifier::~BindNotifier()
{
	for_each(deletableFiles_.begin(),deletableFiles_.end(),&deleteFile);
	pthread_mutex_destroy(&lock_);
}

string trashListenerDirectory(const string& base, bool isHome)
{
	ostringstream result;
	result << base;
	result << "/";
	result << (isHome?"":".");
	result << "Trash/.trailer/trashListeners";
	ensureDirectory(0700,result.str());
	return result.str();
}

void BindNotifier::reportBind(const string& directory, bool isHome)
{
	char hostname[hostsize];
	gethostname(hostname,hostsize);
	{
		ostringstream filename;
		filename << trashListenerDirectory(directory,isHome) << '/';
		filename << network_.boundAt() << '_' << hostname;
		string f=filename.str();
		{
			ofstream o(f.c_str());
			outputRemoteInfo(o,RemoteInfo(PACKAGE_VERSION,"tcp",hostname,network_.boundAt(),directory));
		}
		{
			MutexLock l(lock_);
			deletableFiles_.push_back(f);
		}
	}
	return;
}

void ChangeNotifier::updateConnectionMap(const string& basedir)
{
	struct ConnectionKeyListener : public CanDirectoryListener {
		void home(const string& directory) {handleDirectory(true,directory);}
		void top(const string& directory) {handleDirectory(false,directory);}
		void handleDirectory(bool isHome, const string& directory)
		{
			string d=trashListenerDirectory(directory,isHome);
			DIR *dp;
			if((dp = opendir(d.c_str())) == NULL) {
				throw TrashInfoReadError(TrashInfoReadError::CANT_OPEN_DIR);
			}
			struct dirent *de;
			while((de=readdir(dp)) != NULL) {
				string full=d +"/" + de->d_name;
				if(strcmp(".",de->d_name) == 0 || strcmp("..",de->d_name) == 0) { continue; }
				{
					struct stat st;
					if(stat(full.c_str(),&st) != 0 || !S_ISREG (st.st_mode)) { continue; }
				}
				ifstream in(full.c_str());
				const RemoteInfo ri=inputRemoteInfo(in);
				in.close();
				{
					char hostname[hostsize];
					gethostname(hostname,hostsize);
					// this would be me
					if(strcmp(hostname,ri.hostname.c_str())==0 && notifier->network_.boundAt()==ri.port) continue;
				}
				{
					ConnectionKey key(trashbase,ri.hostname,ri.port);
					if(seen.find(key)==seen.end()) { seen.insert(key); }
				}
			}
			closedir(dp);
		}
		string trashbase;
		ChangeNotifier* notifier;
		set<ConnectionKey> seen;
	} listener;
	listener.notifier=this;
	listener.trashbase=basedir;
	cans_.execute(listener);
	// generate new connections
	for(set<ConnectionKey>::iterator i=listener.seen.begin();i!=listener.seen.end();i++) {
		if(connections_.find(*i)==connections_.end()) {
			remote::Connection* con=new remote::Connection();
			try {
				network_.writer(i->hostname,i->port,*con);
				connections_.insert(make_pair(*i,con));
				for(set<string>::iterator it=scheduled_.begin();it!=scheduled_.end();it++) {
					con->schedule(*it);
				}
			} catch (const remote::ConnectError& e) {
				delete con;
			}
		}
	}
	// clean dead connections
	{
		stack<ConnectionKey> todel;
		for(ConnectionMap::iterator i=connections_.begin();i!=connections_.end();i++) {
			if(listener.seen.find(i->first)==listener.seen.end()) {
				todel.push(i->first);
			}
		}
		while(!todel.empty()) {
			delete connections_[todel.top()];
			todel.pop();
		}
	}
}

void ChangeNotifier::handleEvent(const string& trashname,trashevent::ListenerMember m){
	string basedir;
	try { basedir=trashCanSet(trashname).second; }
	catch (const IsHomeTrash& e) { homeTrashCan(basedir); }
	DEBUGLOG("ChangeNotifier::handleEvent: basedir(" << basedir << "): trashname(" << trashname << ")")
	{
		time_t now;
		time (&now);
		if(lastLooked_.find(basedir)==lastLooked_.end()) {
			lastLooked_[basedir]=0;
		}
		time_t last=lastLooked_[basedir];
		if(last+2 <= now) {
			updateConnectionMap(basedir);
			lastLooked_[basedir]=now;
		}
	}
	for(ConnectionMap::iterator i=connections_.begin();i!=connections_.end();i++) {
		if(i->first.basedir==basedir) {
			(i->second->*m)(trashname);
		}
	}
}

}
}
