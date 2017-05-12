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
 * File:		CommandLine.h
 * Description:	a command line implementation
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#ifndef COMMANDLINE_H_
#define COMMANDLINE_H_

#include <algorithm>
#include <vector>
#include <memory>

#include "TrashCan.h"
#include "RemoteDirectoryUpdater.h"

namespace trailer {
namespace commandline {

void listTrashContents(TrashCan& tc);

class AddNotifier {
public:
	AddNotifier(remote::ChangeNotifier& n,TrashCan& c) : notifier(n), can(c) {}
	void operator() (const string& v) {notifier.add(can.add(v));}
private:
	remote::ChangeNotifier& notifier;
	TrashCan& can;
};

/**
 * ForwardInputIterator are string filenames
 */
template <typename ForwardInputIterator>
inline void add(TrashCan& tc, CanDirectoryListenerExecuter& dirs, ForwardInputIterator fileB, ForwardInputIterator fileE)
{
	using namespace trailer::remote;
	RemoteTrashEventSettings remoteSettings;
	auto_ptr<RemoteTrashEvent> rte;
	const int TRYTIMES=100;
	for(int t=1;t<=TRYTIMES;t++) {
		srand ( time(NULL) );
		try {
			rte.reset(new RemoteTrashEvent(rand() % 5000 + 15000,&remoteSettings));
			break;
		} catch (const remote::NetworkingError& e) {
			if (t==TRYTIMES) throw e;
		}
	}
	ChangeNotifier cn(*rte.get(),dirs);
	AddNotifier ad(cn,tc);
	for_each(fileB,fileE,ad);
	rte.get()->shutdown();
}

class DelegateNotifier {
public:
	DelegateNotifier(remote::ChangeNotifier& n,void (trashevent::EventListener::*td)(const string&))
		: notifier(n), todo(td) {}
	void operator() (const string& v) {(notifier.*todo)(v);}
private:
	remote::ChangeNotifier& notifier;
	void (trashevent::EventListener::*todo)(const string&);
};

class DelegateDoer {
public:
	DelegateDoer(TrashCan& c, candelegate::delegate td)
		: can(c), todo(td) {}
	void operator() (const string& v) {todo(can,v);}
private:
	TrashCan& can;
	candelegate::delegate todo;
};

/**
 * ForwardInputIterator are string filenames
 */
template <typename ForwardInputIterator>
inline void removeTrashItem(
		TrashCan& tc,
		CanDirectoryListenerExecuter& dirs,
		candelegate::delegate todo,
		ForwardInputIterator fileB,
		ForwardInputIterator fileE
) {
	using namespace trailer::remote;
	RemoteTrashEventSettings remoteSettings;
	auto_ptr<RemoteTrashEvent> rte;
	const int TRYTIMES=100;
	for(int t=1;t<=TRYTIMES;t++) {
		srand ( time(NULL) );
		try {
			rte.reset(new RemoteTrashEvent(rand() % 5000 + 15000,&remoteSettings));
			break;
		} catch (const remote::NetworkingError& e) {
			if (t==TRYTIMES) throw e;
		}
	}
	ChangeNotifier cn(*rte.get(),dirs);
	{
		DelegateNotifier sched(cn,&trashevent::EventListener::schedule);
		for_each(fileB,fileE,sched);
	}
	{
		DelegateDoer dd(tc,todo);
		for_each(fileB,fileE,dd);
	}
	{
		DelegateNotifier rem(cn,&trashevent::EventListener::remove);
		for_each(fileB,fileE,rem);
	}
	rte.get()->shutdown();
}

}
}

#endif /* COMMANDLINE_H_ */
