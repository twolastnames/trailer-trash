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
 * File:		CanSet.cpp
 * Description:	logical trash cans
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#include "CanSet.h"

namespace trailer {

#define HOMETRASHDIRECTORY(d) \
{ \
	char * env=getenv("XDG_DATA_HOME"); \
	if(env!=NULL) { \
		d=env; \
		goto EndTrash; \
	} \
} \
{ \
	char* home=getenv("HOME"); \
	if(home==NULL) throw NoUsefullHomeTrashEnvVar(); \
	d=home; \
	d+="/.local/share"; \
} \
EndTrash:

void homeTrashDirectory(string& d) { HOMETRASHDIRECTORY(d); }

string* homeTrashDirectory()
{
	string* d=new string();
	HOMETRASHDIRECTORY( *d );
	return d;
}

#undef HOMETRASHDIRECTORY

PhysicalTrashCan homeTrashCan(string& homeBaseDir)
{
	const static HomeDirPermission homeDirPermission;
	homeTrashDirectory(homeBaseDir);
	return PhysicalTrashCan(&homeDirPermission, &homeBaseDir);
}


PhysicalTrashCan deviceTopDirectoryTrashCan(const string& filename, string& topDir)
{
	const static TopDirPermission perm;
	topDir = topDeviceDirectory(filename);
	return PhysicalTrashCan(&perm,&topDir);
}

const string CanList::add(const string& filename)
{
	{
		struct : public DirectoryListener {
			void onDirectory(const string& d)
			{
				if(!trashed.empty()) return;
				PhysicalTrashCan can=nonHomeTrashCan(d);
				try {
					trashed=can.add(*fn);
				} catch(const CanNotMoveToTrash& e) {
				} catch(const NonRenamableMove& e) {
				}
			}
			string trashed;
			const string* fn;
		} dl;
		dl.fn=&filename;
		fallBackBases_.get(dl);
		if(!dl.trashed.empty()) {
			return dl.trashed;
		}
	}
	{
		string topDir;
		PhysicalTrashCan c=deviceTopDirectoryTrashCan(filename, topDir);
		fallBackBases_.add(topDir);
		return c.add(filename);
	}
}

class CanAndNameItem : public TrashItem {
public:
	CanAndNameItem() : trashItem_(&errorItem_) {}
	CanAndNameItem(TrashItem& ti, const string& tl) :
		trashItem_(&ti), trashLocation_(&tl) {trySettingTrashName();}
	void item(const TrashItem& ti) {trashItem_=&ti; trySettingTrashName();}
	void location(const string& tl) {trashLocation_=&tl; trySettingTrashName();}
	const TimeStamp& time() const {return trashItem_->time();}
	const string& filename() const {return trashItem_->filename();}
	const string& trashname() const
	{
		if(trashItem_==&errorItem_) return errorItem_.trashname();
		return trashname_;
	}
private:
	void trySettingTrashName()
	{
		if(trashItem_==&errorItem_) return;
		trashname_.clear();
		trashname_+=*trashLocation_;
		trashname_+=":";
		trashname_+=trashItem_->trashname();
	}
	string trashname_;
	ErrorItem errorItem_;
	const TrashItem* trashItem_;
	const string* trashLocation_;
	
};

void CanList::items(TrashItemListener& l) const
{
	// translate directory and trashname to a higher name
	struct : public TrashItemListener {
		void handleTrashItem(const TrashItem& i)
		{
			CanAndNameItem ti;
			ti.item(i);
			ti.location(base);
			l->handleTrashItem(ti);
		}
		TrashItemListener* l;
		string base;
	} ll;
	ll.l=&l;
	{ // give all trash directories to translator
		struct : public DirectoryListener {
			void onDirectory(const string& d)
			{
				try {
					PhysicalTrashCan can=nonHomeTrashCan(d);
					can.items(*l);
				} catch(const CanNotMoveToTrash& e) {
				} catch(const NonRenamableMove& e) {
				}
			}
			TrashItemListener* l;
		} dl;
		dl.l=&ll;
		fallBackBases_.get(dl);
	}
}

void allKnownTrashCans(TrashDirectories& listed, CustomDirectory& custom, CanDirectoryListener& listener)
{
	{
		struct : public DirectoryListener, public CustomDirectorySetListener {
			void onDirectory(const string& d)
			{
				listener->top(d);
			}
			void handle(const CustomDirectorySet& s)
			{
				listener->top(s.can);
			}
			CanDirectoryListener* listener;
		} handler;
		handler.listener=&listener;
		listed.get(handler);
		custom.iterate(handler);
	}
	{
		string home;
		homeTrashDirectory(home);
		listener.home(home);
	}
}

/**
* meant to combine what is commmonly used by CanList's unlink, shred & restore commands
* @arg trashanme name of trash can:infoname
* @arg physical method to perform when trash can is located
*/
void doTrashWork(
		CanDirectoryListener& canListener,
		const string& trashname,
		void (PhysicalTrashCan::*physical)(const string&),
		FileMover mover=&onlyRenamableMove
){
	string can;
	string trash;
	{
		size_t seperatorIndex=trashname.find_first_of(':');
		if(seperatorIndex==string::npos) {
			string dir;
			PhysicalTrashCan c=homeTrashCan(dir);
			c.moveMethod(mover);
			(c.*physical)(trashname);
			canListener.home(dir);
			return;
		}
		can=trashname.substr(seperatorIndex+1);
		trash=trashname.substr(0,seperatorIndex);
	}
	{
		PhysicalTrashCan topCan=nonHomeTrashCan(can);
		topCan.moveMethod(mover);
		(topCan.*physical)(trash);
		canListener.top(can);
	}
}

void CanList::unlink(const string& trashname)
{
	DummyCanDirectoryListener l;
	doTrashWork(l,trashname,&PhysicalTrashCan::unlink);
}

void CanList::shred(const string& trashname)
{
	DummyCanDirectoryListener l;
	doTrashWork(l,trashname,&PhysicalTrashCan::shred);
}

void CanList::restore(const string& trashname)
{
	DummyCanDirectoryListener l;
	doTrashWork(l,trashname,&PhysicalTrashCan::restore);
}

void CanList::cleanup()
{
	struct : public DirectoryListener {
		void onDirectory(const string& d)
		{
			try {
				PhysicalTrashCan topCan=nonHomeTrashCan(d);
				topCan.cleanup();
			} catch(const CanNotMoveToTrash& e) {
			} catch(const NonRenamableMove& e) {
			}
		}
	} dl;
	fallBackBases_.get(dl);
}

FileTrashDirectories::FileTrashDirectories(const string& filename) : modified_(false), filename_(filename)
{
	ifstream in(filename_.c_str());
	string s;
	while(getline(in,s)) {
		if(s.size()>0) list_.push_back(s);
	}
}

class DirectoryListenerProxy {
public:
	DirectoryListenerProxy(DirectoryListener& dl) : dl_(dl) {}
	void operator () (const string& d)
	{
		try { dl_.onDirectory(d); }
		catch (const CantMakeDirectory& e) {}
	}
private:
	DirectoryListener& dl_;
};

void FileTrashDirectories::get(DirectoryListener& dl) const
{
	for_each(list_.begin(),list_.end(),DirectoryListenerProxy(dl));
}

void FileTrashDirectories::add(const string& dir)
{
	struct : public DirectoryListener {
		void onDirectory(const string& d)
		{
			if(*dir==d) { found = true; }
		}
		const string* dir;
		bool found;
	} dl;
	dl.found=false;
	dl.dir=&dir;
	get(dl);
	if(dl.found==false) {
		list_.push_back(*dl.dir);
		modified_=true;
	}
}

FileTrashDirectories::~FileTrashDirectories()
{
	if(modified_) {
		ofstream out(filename_.c_str());
		for(FileTrashDirectoryList::iterator i=list_.begin();i!=list_.end();i++) {
			out << (*i) << endl;
		}
	}
}

void FileCustomDirectory::add(const CustomDirectorySet& cds)
{
	ofstream out(filename_.c_str(),ios::app);
	out << cds.can << ':' << cds.target << endl;
}

CustomDirectorySet FileCustomDirectory::get(const string& target) const
{
	struct : public CustomDirectorySetListener {
		void handle(const CustomDirectorySet& s) {
			if(found) return;
			if(s.target.size() >= target->size()) return;
			{
				string t=target->substr(0,s.target.size());
				if(s.target==t) {
					dirSet=s;
					found=true;
				}
			}
		}
		bool found;
		CustomDirectorySet dirSet;
		const string* target;
	} listener;
	listener.found=false;
	listener.target=&target;
	iterate(listener);
	if(!listener.found) {
		throw NoDirectoryForTarget(target);
	}
	return listener.dirSet;
}

void FileCustomDirectory::remove(const CustomDirectorySet& removable)
{
	struct : public CustomDirectorySetListener {
		void handle(const CustomDirectorySet& s) {
			if(found) return;
			if(s.can==looking.can && s.target==looking.target) {
				found=true;
				return;
			}
			seen.push_back(s);
		}
		CustomDirectorySet looking;
		bool found;
		list<CustomDirectorySet> seen;
	} listener;
	listener.found=false;
	listener.looking=removable;
	iterate(listener);
	if(!listener.found) throw NoDirectoryForTarget(removable.target);
	{ ofstream out(filename_.c_str()); }
	for(
			list<CustomDirectorySet>::iterator i=listener.seen.begin();
			i!=listener.seen.end();
			i++) {
		add(*i);
	}
}

void FileCustomDirectory::iterate(CustomDirectorySetListener& l) const
{
	ifstream in(filename_.c_str());
	string s;
	while(getline(in,s)) {
		size_t sep=s.find_first_of(':');
		if(sep!=string::npos) {
			CustomDirectorySet cds;
			cds.target=stripTrailingFileSeprator(s.substr(0,sep));
			cds.can=stripTrailingFileSeprator(s.substr(sep+1));
			l.handle(cds);
		}
	}
	if(in.bad()) throw NoDirectoryForTarget(s);
}

const string CustomToHomeToList::add(const string& name)
{
	try {
		CustomDirectorySet set=customDirectory_.get(name);
		PhysicalTrashCan trash=nonHomeTrashCan(set.can);
		trash.moveMethod(&onlyRenamableMove);
		string trashname=trash.add(name);
		usageListener_->top(set.can);
		return trashname;
	} catch (const FileToTrashDoesNotExist& e) {
		throw e;
	} catch (const ExceptionBase& e) {}
	try {
		string base;
		PhysicalTrashCan trash=homeTrashCan(base);
		trash.moveMethod(&onlyRenamableMove);
		string trashname=trash.add(name);
		usageListener_->home(base);
		return trashname;
	} catch (const FileToTrashDoesNotExist& e) {
		throw e;
	} catch (const NonRenamableMove& e) { // do nothing
	} catch (const ExceptionBase& e) {
		throw e;
	}
	try {
		CanList canList(trashDirectory_);
		return canList.add(name);
	} catch (const ExceptionBase& e) {}
	try {
		string base;
		PhysicalTrashCan trash=homeTrashCan(base);
		trash.moveMethod(&tryAnythingMove);
		string trashname=trash.add(name);
		usageListener_->home(base);
		return trashname;
	} catch (const FileToTrashDoesNotExist& e) {
		throw e;
	}
	throw NoDirectoryForTarget(name);
}

void CustomToHomeToList::items(TrashItemListener& l) const
{
	struct : public DirectoryListener {
		void onDirectory(const string& dir)
		{
			try {
				PhysicalTrashCan physical=nonHomeTrashCan(dir);
				NameAppendedCan can(physical,dir);
				can.items(*itemListener);
				dirListener->top(dir);
			} catch (const CantMakeDirectory& e) {
			}
		}
		TrashItemListener* itemListener;
		CanDirectoryListener* dirListener;
	} echoerListener;
	echoerListener.itemListener=&l;
	echoerListener.dirListener=usageListener_;
	OneTimeDirectoryEchoer echoer(echoerListener);
	string homeTrashDir;
	PhysicalTrashCan homeCan = homeTrashCan(homeTrashDir);
	echoer.markSeen(homeTrashDir);
	echoer.handle(customDirectory_);
	homeCan.items(l);
	usageListener_->home(homeTrashDir);
	echoer.handle(trashDirectory_);
}

void CustomToHomeToList::cleanup()
{
	{
		string homeTrashDir;
		PhysicalTrashCan homeCan = homeTrashCan(homeTrashDir);
		homeCan.cleanup();
		usageListener_->home(homeTrashDir);
	}
	{
		struct : public DirectoryListener {
			void onDirectory(const string& dir)
			{
				PhysicalTrashCan can=nonHomeTrashCan(dir);
				can.cleanup();
				dirListener->top(dir);
			}
			CanDirectoryListener* dirListener;
		} echoerListener;
		echoerListener.dirListener=usageListener_;
		OneTimeDirectoryEchoer echoer(echoerListener);
		echoer.handle(customDirectory_);
		echoer.handle(trashDirectory_);
	}
}

namespace appendingtrashitem {
class AppendingItem : public TrashItem {
public:
	AppendingItem(const string& directory,const TrashItem& delegate) : delegate_(delegate)
	{
		trashname_+=delegate.trashname();
		trashname_+=":";
		trashname_+=directory;
	}
	const TimeStamp& time() const {return delegate_.time();}
	const string& filename() const {return delegate_.filename();}
	const string& trashname() const {return trashname_;}
	const TrashItem& delegate_;
	string trashname_;
};

class AppendingListener : public TrashItemListener {
public:
	AppendingListener(const string& directory, TrashItemListener& delegate) :
		directory_(directory), delegate_(delegate) {}
	void handleTrashItem(const TrashItem& ti)
	{
		AppendingItem item(directory_, ti);
		delegate_.handleTrashItem(item);
	}
private:
	const string& directory_;
	TrashItemListener& delegate_;
};
}

void NameAppendedCan::items(TrashItemListener& l) const
{
	appendingtrashitem::AppendingListener listener(directory_,l);
	can_.items(listener);
}

void NameAppendedCan::doSomethingDirectoryBased(
		const string& name,
		candelegate::delegate call
)
{
	size_t seperatorIndex=name.find_first_of(':');
	if(seperatorIndex==string::npos) {
		string home;
		homeTrashDirectory(home);
		if(home==directory_) {
			call(can_,name);
		} else {
			throw InvalidTrashNameFormat(name);
		}
		return;
	}
	if(name.substr(seperatorIndex+1)!=directory_) {
		return;
	}
	{
		string trash=name.substr(0,seperatorIndex);
		call(can_,trash);
	}
}

/**
 * @throws IsHomeTrash if it is home directory trash
 * @returns trashname/directory pair
 */
const pair<string,string> trashCanSet(const string& name)
{
	size_t seperatorIndex=name.find_first_of(':');
	if(seperatorIndex==string::npos) {
		throw IsHomeTrash(name);
	}
	return make_pair(name.substr(0,seperatorIndex),name.substr(seperatorIndex+1));
}

const string NameAppendedCan::add(const string& name)
{
	size_t seperatorIndex=name.find_first_of(':');
	if(seperatorIndex==string::npos) {
		string home;
		homeTrashDirectory(home);
		if(home==directory_) {
			return can_.add(name);
		} else {
			throw InvalidTrashNameFormat(name);
		}
	}
	{
		string trash=name.substr(0,seperatorIndex);
		return can_.add(trash);
	}
}



}


