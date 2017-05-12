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

#ifndef CANSET_H_
#define CANSET_H_

#include <algorithm>
#include <fstream>
#include <set>
#include <vector>

#include "TrashCan.h"

using namespace std;

namespace trailer {

/**
 * thrown when a FreeDesktop.org compliant home directory location can be found
 */
struct NoUsefullHomeTrashEnvVar : public ExceptionBase {};

/**
 * @arg d (out) the directory FreeDesktop.org says should house the home directory
 */
void homeTrashDirectory(string& d);

/**
 * and unmanaged void homeTrashDirectory(string& d)
 * @return the home trash directory
 */
string* homeTrashDirectory();

/**
 * @arg homeBaseDir (out) the home trash can directory
 * @return a home trash can
 */
PhysicalTrashCan homeTrashCan(string& homeBaseDir);

/**
 * creates a hidden trash can
 * @arg base base directory of the trash can
 * @return the trash can
 */
inline PhysicalTrashCan nonHomeTrashCan(const string& base)
	{static TopDirPermission p; return PhysicalTrashCan(&p,&base);}

/**
 * @arg filename file desired to be put or restored in the can
 * @arg top (out) the top direcotry
 * @return trash can on top level directory of the device it is on
 */
PhysicalTrashCan deviceTopDirectoryTrashCan(const string& filename, string& topDir);

/**
 * Encapsulates the permissions and home directory for a trash can
 */
class EncapsulatingHomeCan : public PhysicalTrashCan {
public:
	EncapsulatingHomeCan() :
		PhysicalTrashCan(homeDirPermission_=new HomeDirPermission(),homeBaseDir_=homeTrashDirectory())
	{}
	~EncapsulatingHomeCan() {delete homeBaseDir_;delete homeDirPermission_;}
private:
	EncapsulatingHomeCan(const EncapsulatingHomeCan&); // trying to stop copying
	EncapsulatingHomeCan& operator = (const EncapsulatingHomeCan&); // trying to stop copying
	string* homeBaseDir_;
	DirectoryAttributes* homeDirPermission_;
};

/**
 * thrown when an invalid format for a trash item name is given
 */
struct InvalidTrashNameFormat : public ExceptionBase {
	InvalidTrashNameFormat(const string& n) : name(n) {}
	const string name;
};

/**
 * call back for directory listening
 */
struct DirectoryListener {
	virtual ~DirectoryListener() {}
	virtual void onDirectory(const string&)=0;
};

/**
 * a collection of trash directories, useful for listing contents of all
 * physical trash cans
 */
struct TrashDirectories {
	virtual ~TrashDirectories() {}
	virtual void get(DirectoryListener& dl) const =0;
	virtual void add(const string& dir)=0;
};

/**
 * for custom trash can configuration (useful with complicated NFS)
 */
struct CustomDirectorySet {
	string can;
	string target;
};

/**
 * for iterating CustomDirectorySet(s)
 */
struct CustomDirectorySetListener {
	virtual ~CustomDirectorySetListener() {}
	virtual void handle(const CustomDirectorySet&)=0;
};

/**
 * custom trash can location logic
 */
struct CustomDirectory {
	virtual ~CustomDirectory() {}
	virtual void add(const CustomDirectorySet& cds)=0;
	void add(const string& canbase, const string targetbase)
		{CustomDirectorySet cds;cds.can=canbase;cds.target=targetbase;add(cds);}
	/**
	@throw NoDirectoryForTarget if a custom location has not been defined
	*/
	virtual CustomDirectorySet get(const string& target) const =0;
	/**
	@throw NoDirectoryForTarget if value is not in list
	*/
	virtual void remove(const CustomDirectorySet&)=0;
	/**
	@throw NoDirectoryForTarget if list is unaccessible
	*/
	virtual void iterate(CustomDirectorySetListener&) const =0;
};

struct CanDirectoryListener {
	virtual ~CanDirectoryListener() {}
	/** the home directory */
	virtual void home(const string& directory)=0;
	/** all but the home */
	virtual void top(const string& directory)=0;
};

struct DummyCanDirectoryListener : public CanDirectoryListener {
	void home(const string& directory) {}
	void top(const string& directory) {}
};

/**
 * sorts out appended names
 */
void doTrashWork(
		CanDirectoryListener& l,
		const string& trashname,
		void (PhysicalTrashCan::*physical)(const string&),
		FileMover mover
);

/**
 * for gathering all known trash cans
 */
void allKnownTrashCans(TrashDirectories& listed, CustomDirectory& custom, CanDirectoryListener& listener);

struct CanDirectoryListenerExecuter {
	virtual ~CanDirectoryListenerExecuter() {}
	virtual void execute(CanDirectoryListener&)=0;
};

struct AllKnownTrashCans : public CanDirectoryListenerExecuter {
	AllKnownTrashCans(TrashDirectories& l,CustomDirectory& c) : listed(l), custom(c) {}
	void execute(CanDirectoryListener& l)
	{
		allKnownTrashCans(listed, custom, l);
	}
	TrashDirectories& listed;
	CustomDirectory& custom;
};

typedef vector<string> FileTrashDirectoryList;

/**
 * a file based implementation of TrashDirectories
 */
class FileTrashDirectories : public TrashDirectories {
public:
	FileTrashDirectories(const string& filename);
	~FileTrashDirectories();
	void get(DirectoryListener& dl) const;
	void add(const string& dir);
private:
	bool modified_;
	FileTrashDirectoryList list_;
	string filename_;
};

/**
 * a file based implementation of CustomDirectory
 */
class FileCustomDirectory : public CustomDirectory {
public:
	FileCustomDirectory(const string& filename) : filename_(filename) {}
	void add(const CustomDirectorySet& cds);
	CustomDirectorySet get(const string& target) const;
	void iterate(CustomDirectorySetListener&) const ;
	void remove(const CustomDirectorySet&);
private:
	string filename_;
};

/**
 * for insuring a directory is only iterated through once
 */
class OneTimeDirectoryEchoer {
public:
	/**
	 * @arg listener will be called back once for each unique directory
	 */
	OneTimeDirectoryEchoer(DirectoryListener& listener) {listener_.listener=&listener;}
	void handle(const CustomDirectory& cd) {cd.iterate(listener_);}
	void handle(const TrashDirectories& td) {td.get(listener_);}
	/**
	 * @arg d directory to mark as a seen directory, thus not being handled
	 */
	void markSeen(const string d)
		{if(listener_.seen.find(d) == listener_.seen.end()) listener_.seen.insert(d);}
private:
	struct : public CustomDirectorySetListener, public DirectoryListener {
		void handle(const CustomDirectorySet& cd) {handleDirectory(cd.can);}
		void onDirectory(const string& d) {handleDirectory(d);}
		void handleDirectory(const string& d)
		{
			if(seen.find(d)==seen.end()){
				seen.insert(d);
				listener->onDirectory(d);
			}
		}
		DirectoryListener* listener;
		set<string> seen;
	} listener_;
};

/**
 * works off a list of known trash cans and/or attempts to make more based of device ids
 */
class CanList : public TrashCan {
public:
	CanList(TrashDirectories& bases) : fallBackBases_(bases) {}
	const string add(const string& filename);
	void items(TrashItemListener& l) const;
	void unlink(const string& trashname);
	void shred(const string& trashname);
	void restore(const string& trashname);
	void cleanup();
private:
	TrashDirectories& fallBackBases_;
	string homeBaseDir_;
	string topDir_;
};

/**
 * tries custom, home & CanList trash cans in that order
 */
class CustomToHomeToList : public TrashCan {
public:
	CustomToHomeToList(CustomDirectory& cd, TrashDirectories& td) :
		usageListener_(&dummyListener_), customDirectory_(cd), trashDirectory_(td) {}
	/** throws NoDirectoryForTarget */
	const string add(const string& filename);
	void items(TrashItemListener& l) const;
	void unlink(const string& trashname)
		{doTrashWork(*usageListener_,trashname,&PhysicalTrashCan::unlink,&tryAnythingMove);}
	void shred(const string& trashname)
		{doTrashWork(*usageListener_,trashname,&PhysicalTrashCan::shred,&tryAnythingMove);}
	void restore(const string& trashname)
		{doTrashWork(*usageListener_,trashname,&PhysicalTrashCan::restore,&tryAnythingMove);}
	void cleanup();
	void canUsageListener(CanDirectoryListener* l) {usageListener_=l;}
private:
	DummyCanDirectoryListener dummyListener_;
	CanDirectoryListener* usageListener_;
	CustomDirectory& customDirectory_;
	TrashDirectories& trashDirectory_;
};

/**
 * useful for dealing with names that need to be appended for identifying which trash can to use
  */
class NameAppendedCan : public TrashCan {
public:
	NameAppendedCan(TrashCan& can, const string& directory) :
		can_(can), directory_(directory) {}
	const string add(const string& filename);
	void items(TrashItemListener& l) const;
	void unlink(const string& trashname)
		{doSomethingDirectoryBased(trashname,&candelegate::unlink);}
	void shred(const string& trashname)
	{doSomethingDirectoryBased(trashname,&candelegate::shred);}
	void restore(const string& trashname)
		{doSomethingDirectoryBased(trashname,&candelegate::restore);}
	void cleanup() {can_.cleanup();}
private:
	void doSomethingDirectoryBased(
		const string& name,
		candelegate::delegate call
	);
	TrashCan& can_;
	const string& directory_;
};

struct IsHomeTrash : public ExceptionBase {
	IsHomeTrash(const string& n) : name(n) {}
	const string name;
};

/** @throws IsHomeTrash if it is home directory trash
    @returns first=trashname second=directory */
const pair<string,string> trashCanSet(const string&);

inline InfoFile getItemFromTrashname(const string& trashname)
{
	string infofile;
	try {
		pair<string,string> p=trashCanSet(trashname);
		infofile+=p.second;
		infofile+="/.Trash/info";
	} catch (const IsHomeTrash& ht) {
		homeTrashDirectory(infofile);
		infofile+="/Trash/info";
	}
	return InfoFile(infofile,trashname);
}

inline string ensureHomeDirGone(const string& trashname)
{
	try {
		string homecan;
		pair<string,string> split=trashCanSet(trashname);
		homeTrashDirectory(homecan);
		if(split.second==homecan) return (split.first);
	} catch(const IsHomeTrash& e) {}
	return trashname;
}

}

#endif /* CANSET_H_ */
