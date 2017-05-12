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
 * File:		ConsoleExecuter.cpp
 * Description:	for executing console based features
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Aug 13 2010
 **********************************************************************************************/

#include "ConsoleExecuter.h"
namespace trailer {
namespace console {

struct ExecuterArg {
	ExecuterArg(JobScheduler& s,MessageListener& l) : scehduler(s), listener(l) {}
	JobScheduler& scehduler;
	MessageListener& listener;
};

void* jobQueueExecuter(void* s)
{
	ExecuterArg* arg=reinterpret_cast<ExecuterArg*>(s);
	while(true) {
		try {
			arg->scehduler.executeNext();
		}catch (const CanNotUnlinkFile& e) {
			string m;
			m+="Can't unlink file [";
			m+=e.file;
			m+="] do to [";
			m+=CantRemoveFile::errorString(e.error);
			m+="]";
			arg->listener(m);
		} catch (const CanNotMoveToTrash& e) {
			string m;
			m+="Can not move to trash with error ";
			m+=e.error;
			arg->listener(m);
		} catch (const NoDirectoryForTarget& e) {
			string m;
			m+="Can not find directory to put [";
			m+=e.target;
			m+="]";
			arg->listener(m);
		} catch (const FileToTrashDoesNotExist& e) {
			string m;
			m+="File to move to trash [";
			m+=e.filename;
			m+="] does not exist";
			arg->listener(m);
		} catch  (const CantMakeDirectory& e) {
			string m;
			m+="Can't make directory [";
			m+=e.directory;
			m+="]";
			arg->listener(m);
		} catch (const CanNotCreateTrashInfo& e) {
			string m;
			m+="CanNotCreateTrashInfo Error";
			arg->listener(m);
		} catch (const CanNotGetCWD& e) {
			string m;
			m+="CanNotGetCWD ";
			m+=e.error;
			arg->listener(m);
		} catch (const CanNotMoveFile& e) {
			string m;
			m+="Can't Move File from [";
			m+=e.source;
			m+="] to [";
			m+=e.destination;
			m+="] because of [";
			switch(e.error) {
			case CanNotMoveFile::BAD_SRC:
				m+="Bad Source File";
				break;
			case CanNotMoveFile::BAD_DEST:
				m+="Bad Destination File";
				break;
			case CanNotMoveFile::SRC_DEL_ERR:
				m+="Internal Error";
				break;
			case CanNotMoveFile::UNKOWN_TYPE:
				m+="Internal Error";
				break;
			default:
				m+="Unknown Reasons";
			}
			m+="]";
			arg->listener(m);
		} catch (const CantRemoveFile& e) {
			string m;
			m+="can't remove [";
			m+=e.file;
			m+="] because of ";
			m+=e.error;
			arg->listener(m);
		} catch (const TrashInfoReadError& e) {
			string m;
			m+="TrashInfoReadError ";
			m+=e.error;
			arg->listener(m);
		} catch (const TrashItemNotSet& e) {
			string m;
			m+="TrashItemNotSet";
			arg->listener(m);
		} catch (const JobsQueueClosed& e) {
			return NULL;
		} catch (const ExceptionBase& e) {
			string m;
			m+="Undefined Error";
			arg->listener(m);
		}
	}
	return NULL;
}

struct ThreadExecuter {
	ThreadExecuter(JobScheduler& s,MessageListener& l) : arg(s,l)
		{pthread_create(&t,NULL,&jobQueueExecuter,reinterpret_cast<void*>(&arg));}
	~ThreadExecuter() {pthread_join(t,NULL);}
	ExecuterArg arg;
	pthread_t t;
};

void fullScreenTrash(FullScreenInterface& fullScreen, CanDirectoryListenerExecuter& c, remote::RemoteTrashEvent& rte)
{
	using namespace remote;
	stack<ThreadExecuter*> executers;
	stack<ChangeNotifier*> changeNotifiers;
	for(int i=0;i<fullScreen.scheduler().threadCount();i++) {
		executers.push(new ThreadExecuter(fullScreen.scheduler(), fullScreen.messages()));
		changeNotifiers.push(new ChangeNotifier(rte,c));
		fullScreen.trashScreen().eventListener(*changeNotifiers.top());
	}
	fullScreen.execute();
	while(!executers.empty()) {
		delete executers.top();
		executers.pop();
	}
	while(!changeNotifiers.empty()) {
		delete changeNotifiers.top();
		changeNotifiers.pop();
	}
}

class MessageLoader {
public:
	MessageLoader(MessageListener& l) : listener_(l) {}
	void operator () (const string& l) { listener_(l);}
private:
	MessageListener& listener_;
};

void fullScreenTrash(TrashCan& action,FullScreenArgument* arg)
{
	using namespace remote;
	NcursesSetup _s;
	stack<string> extraMessages;
	auto_ptr<ColorDefinitions> cd;
	if(arg->colorFile.empty()) {
		cd.reset(new DefaultColors());
	} else try {
		cd.reset(new ColorFile(arg->colorFile));
	} catch (const colorfiletags::InvalidColorData& e) {
		ostringstream error;
		error << "InvalidColorData:  ";
		switch (e.reason) {
		case colorfiletags::InvalidColorData::FREE_TEXT:
			error << " extra text ";
			break;
		case colorfiletags::InvalidColorData::INVALID_COLOR:
			error << " invalid color ";
			break;
		case colorfiletags::InvalidColorData::INVALID_SHADE:
			error << " invalid shade ";
			break;
		case colorfiletags::InvalidColorData::INVALID_TAG:
			error << " invalid tag ";
			break;
		case colorfiletags::InvalidColorData::MISSING_SHADE:
			error << " missing shade ";
			break;
		case colorfiletags::InvalidColorData::MISSING_TAG:
			error << " invalid tag ";
			break;
		case colorfiletags::InvalidColorData::NO_WELL_FORM:
			error << " not well formed ";
			break;
		case colorfiletags::InvalidColorData::NO_INIT_TAG:
			error << " no initialization tag ";
			break;
		case colorfiletags::InvalidColorData::MULTIPLE_STARTS:
			error << " multiple starts ";
			break;
		case colorfiletags::InvalidColorData::INVALID_ATTRIBUTE:
			error << " invalid attribute ";
			break;
		case colorfiletags::InvalidColorData::MISSING_COLOR:
			error << " missing color ";
			break;
		};
		error << "... File " << arg->colorFile << " is not valid. Deleting it may help.";
		cd.reset(new DefaultColors());
		extraMessages.push(error.str().c_str());
	}
	FullScreenInterface i(&action,stdscr,*cd.get());
	while(!extraMessages.empty()) {
		i.messages()(extraMessages.top());
		extraMessages.pop();
	}
	for_each(arg->messages.begin(),arg->messages.end(),
		MessageLoader(i.messages()));
	AllKnownTrashCans aktc(*arg->trashDirectories,*arg->customDirectory);
	RemoteTrashEventSettings remoteSettings;
	auto_ptr<RemoteTrashEvent> rte;
	const int TRYTIMES=100;
	for(int t=1;t<=TRYTIMES;) {
		srand ( time(NULL) );
		try {
			rte.reset(new RemoteTrashEvent(rand() % 5000 + 15000,&remoteSettings));
			break;
		} catch (const remote::NetworkingError& e) {
			if (t==TRYTIMES) throw e;
		}
	}
	rte.get()->addListener(&i.trashScreen());
	BindNotifier bn(*rte.get());
	allKnownTrashCans(*arg->trashDirectories,*arg->customDirectory,bn);
	console::fullScreenTrash(i,aktc,*rte.get());
}

}
}



