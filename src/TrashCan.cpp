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
 * File:		TrashCan.cpp
 * Description:	implements a trash can based on a file directory base
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#include <cstdlib>

#include "TrashCan.h"

namespace trailer {

const char* TRASH_TRASH = "Trash";
const char* TRASH_FILES = "files";
const char* TRASH_INFO  = "info";
const char* TRASH_INFO_EXT=".trashinfo";
const char* SUBDIRS[] = {TRASH_FILES,TRASH_INFO,NULL};

const char* TRASHFILE_HEADER="[Trash Info]";
const char* TRASHFILE_PATH="Path=";
const char* TRASHFILE_DELETION="DeletionDate=";

typedef const string (PhysicalTrashCan::*DIRECTORY) () const;

bool readNamedLine(const string& line, string& result, const char* head)
{
	if(!result.empty()) return false;
	size_t len=strlen(head);
	if(line.size() < len) return false;
	if(strcmp(line.substr(0,len).c_str(),head)==0) {
		result=line.substr(len);
		return true;
	} else {
		return false;
	}
}

const string InfoFile::infoFileName() const
{
	string info=infodir_;
	info+="/";
	info+=trashname_;
	info+=TRASH_INFO_EXT;
	return info;
}

InfoFile::InfoFile(
	const string infodir,
	const string trashname,
	const string filename,
	const TimeStamp stamp) :
infodir_(infodir) , trashname_(trashname), filename_(filename), stamp_(stamp)
{
	string info=infoFileName();
	ofstream out(info.c_str());
	out << TRASHFILE_HEADER << endl;
	out << TRASHFILE_PATH << filename_ << endl;
	out << TRASHFILE_DELETION << stamp_  << endl;
	out.close();
	if(out.bad()) {
		throw CanNotCreateTrashInfo();
	}
}

InfoFile::InfoFile(const string infodir,const string trashname) :
infodir_(infodir), trashname_(trashname)
{
	string infofile=infoFileName();
	ifstream in(infofile.c_str());
	if(!in.good()) {
		throw TrashInfoReadError(TrashInfoReadError::BAD_FILE);
	}
	bool seenHeader=false;
	bool seenStamp=false;
	string line;
	while(getline(in,line)) {
		if(line.size()==0) continue;
		if(!seenHeader) {
			if(line.size() < strlen(TRASHFILE_HEADER)) {
				throw TrashInfoReadError(TrashInfoReadError::MISSING_HEADER);
			}
			string trimmed(line.substr(0,strlen(TRASHFILE_HEADER)));
			if(strcmp(trimmed.c_str(),TRASHFILE_HEADER)!=0) {
				throw TrashInfoReadError(TrashInfoReadError::MISSING_HEADER);
			}
			seenHeader=true;
			continue;
		} else if (readNamedLine(line,filename_,TRASHFILE_PATH)) {
			continue;
		} else if (readNamedLine(line,stamp_,TRASHFILE_DELETION)) {
			seenStamp=true;
			continue;
		}
		if(!seenHeader) {
			throw TrashInfoReadError(TrashInfoReadError::MISSING_HEADER);
		}
	}
	if(filename_.empty()) {
		throw TrashInfoReadError(TrashInfoReadError::MISSING_NAME);
	} else if (!seenStamp) {
		throw TrashInfoReadError(TrashInfoReadError::MISSING_DATE);
	}
}

void PhysicalTrashCan::ensureTrashDirectory()
{
	string filesdir(this->filesdir());
	list<string> neededDirs;
	// deduce directories that need to be created for files directory
	for(size_t i=filesdir.size();i>=0;i--) {
		string dir;
		if(filesdir.size()>0 && i==0 && filesdir.at(0)=='/') {
			break;
		} else if(i==filesdir.size()-1) {
			dir=filesdir;
		} else if(filesdir.size()!=i && filesdir.at(i)=='/') {
			dir=filesdir.substr(0,i);
		} else {
			continue;
		}
		{
			struct stat st;
			if(stat(dir.c_str(),&st)==0) {
				break;
			} else {
				neededDirs.push_front(dir);
			}
		}
	}
	{ // deduce a need for info directory
		struct stat st;
		string infodir(this->infodir());
		if(stat(infodir.c_str(),&st)!=0) {
			neededDirs.push_back(infodir);
		}
	}
	// generate needed directories
	for(list<string>::iterator i=neededDirs.begin();i!=neededDirs.end();i++) {
		if(mkdir(i->c_str(),00)==-1) {
			throw CantMakeDirectory(*i);
		}
		chmod(i->c_str(),directoryAttr_->permission());
		{
			struct stat st;
			int result;
			if((result=stat(i->c_str(),&st)) != 0 || !directoryAttr_->check(st.st_mode)) {
				ostringstream m;
				m << st.st_mode;
				string ms(m.str());
				throw TrashDirectoryModeError(*i,ms,result==0);
			}
		}
	}
}

PhysicalTrashCan::PhysicalTrashCan(const DirectoryAttributes* da, const string* baseDir)
	: directoryAttr_(da), baseDir_(baseDir), moverMethod_(&onlyRenamableMove),
	  beforeRestoringOnFile_(&toTrashCanMover), timestampCreator_(&getLocalSecondsTimestamp)
{
	ensureTrashDirectory();
}

const string PhysicalTrashCan::trashdir() const
{
	string dir(*baseDir_);
	if(dir.size()==0 || dir.at(dir.size()-1)!='/') dir += "/";
	if(directoryAttr_->hidden()) dir+=".";
	dir+=TRASH_TRASH;
	return dir;
}

const string PhysicalTrashCan::filesdir() const
{
	string filesdir(trashdir());
	filesdir+="/";
	filesdir+=TRASH_FILES;
	return filesdir;
}

const string PhysicalTrashCan::infodir() const
{
	string infodir(trashdir());
	infodir+="/";
	infodir+=TRASH_INFO;
	return infodir;
}

const string PhysicalTrashCan::infofile(const string& trashname) const
{
	string file;
	file+=infodir();
	file+=trashname;
	file+=TRASH_INFO_EXT;
	return file;
}

string getTimeStamp(const char* format)
{
	const int stampsize=19+1;
	char stamp[stampsize];
	time_t gmt;
	time(&gmt);
	tm* lt=localtime(&gmt);
	strftime(stamp,stampsize,format,lt);
	return string(stamp);
}

string getLocalSecondsTimestamp() { return getTimeStamp("%Y-%m-%dT%H:%M:%S"); }

const string ensureFullPathName(const string& filename)
{
	string fullname;
	if(filename.size()<1) {
		throw FileToTrashDoesNotExist(filename);
	} else if (filename.at(0)=='/') {
		fullname=filename;
	} else {
		string currentdir;
		{
			size_t size=0x10;
			char* c=NULL;
			while(true) {
				if(c!=NULL) delete [] c;
				size = size << 1;
				c = new char[size];
				char* r=getcwd(c,size);
				if(r!=NULL) {break;}
				if(errno != ERANGE) {
					throw CanNotGetCWD(errno);
				}
			};
			currentdir= c;
			delete [] c;
		}
		fullname+=currentdir;
		if(fullname.size()<1 || fullname.at(fullname.size()-1)!='/') fullname+="/";
		fullname+=filename;
	}
	return fullname;
}

const string topDeviceDirectory(const string& base)
{
	string last;
	{
		struct stat original;
		if(stat(base.c_str(),&original)!=0) { throw FileToTrashDoesNotExist(base); }
		string fullname=ensureFullPathName(base);
		size_t index=fullname.size();
		string current;
		while (index>0) {
			index--;
			current.clear();
			if(fullname.at(index)=='/' && last.size()==0) {
				last=fullname.substr(0,index);
				continue;
			} else if (fullname.at(index)=='/') {
				current=fullname.substr(0,index);
				struct stat st;
				if(stat(current.c_str(),&st)!=0) {continue;}
				if(st.st_dev!=original.st_dev) {break;}
				last=current;
			}
		}
	}
	return last;
}

const string PhysicalTrashCan::add(const string& filename)
{
	// get full path name
	const string fullname=ensureFullPathName(filename);
	// find something to call it in the trash can
	string toname;
	{
		int i=fullname.size();
		while(--i > 0 && fullname.at(i) != '/') {}
		int samenames;
		for(samenames=0;1;samenames++) {
			ostringstream namecheck;
			namecheck << filesdir() << '/' << fullname.substr(i+1);
			if(samenames!=0) namecheck << '.' << samenames;
			{
				struct stat s;
				const char* tocheck=namecheck.str().c_str();
				if(stat(tocheck,&s)!=0) break;
			}
		}
		{
			ostringstream name;
			name << fullname.substr(i+1);
			if(samenames!=0) name << '.' << samenames;
			toname = name.str().c_str();
		}
	}
	// move it to the trash can
	string trashfile=filesdir();
	trashfile+= "/";
	trashfile+= toname;
	moverMethod_(fullname,trashfile);
	// make info file
	try {
		TimeStamp ts=timestampCreator_();
		InfoFile ifile(infodir(),toname,fullname,ts);
	} catch (const CanNotCreateTrashInfo& e) {
		moverMethod_(trashfile,fullname);
		throw e;
	}
	return toname;
}


void onlyRenamableMove(const string& from, const string& to)
{
	// check that from exists
	struct stat original;
	if(stat(from.c_str(),&original)!=0) {
		throw FileToTrashDoesNotExist(from);
	}
	// attempt simple rename
	if(rename(from.c_str(),to.c_str())==0) {return;}
	{
		int error=errno;
		if(error!=EXDEV) {throw CanNotMoveToTrash(error);}
	}
	throw NonRenamableMove(from,to);
}

void copyThenDeleteToMove(const string& from, const string& to)
{
	chmod(from.c_str(),0777);
	ifstream src(from.c_str(),ios::binary);
	if(src.bad() | !src.is_open()) {
		throw CanNotMoveFile(from,to,CanNotMoveFile::BAD_SRC);
	}
	ofstream dest(to.c_str(),ios::binary);
	if(dest.bad() | !dest.is_open()) {
		throw CanNotMoveFile(from,to,CanNotMoveFile::BAD_DEST);
	}
	dest << src.rdbuf();
	src.close();
	if(src.bad()) {
		throw CanNotMoveFile(from,to,CanNotMoveFile::BAD_SRC);
	}
	dest.close();
	if(dest.bad()) {
		throw CanNotMoveFile(from,to,CanNotMoveFile::BAD_DEST);
	}
	if(remove(from.c_str())!=0) {
		remove(to.c_str());
		throw CantRemoveFile(from,CantRemoveFile::NOT_UNLINKING);
	}
}

void tryAnythingMove(const string& from, const string& to)
{
	try {
		onlyRenamableMove(from,to);
		return;
	} catch (const ExceptionBase& e) { /* pass */ }
	copyThenDeleteToMove(from,to);
}

void PhysicalTrashCan::items(TrashItemListener& til) const
{
	DIR *dp;
	if((dp = opendir(filesdir().c_str())) == NULL) {
		throw TrashInfoReadError(TrashInfoReadError::CANT_OPEN_DIR);
	}
	struct dirent *de;
	while((de=readdir(dp)) != NULL) {
		try {
			string file(de->d_name);
			if(file=="." || file=="..") {continue;}
			string dir(infodir());
			const InfoFile i(dir,file);
			til.handleTrashItem(i);
		} catch(const TrashInfoReadError& e) {
			// TODO: add some verbose behavior
		}
	}
	closedir(dp);
}

template <typename EX>
void removeLink(const string& link, void (*todo)(const string& l))
{
	struct stat st;
	if(stat(link.c_str(),&st)!=0) {
		throw EX(link,EX ::DOESNOT_EXIST);
	}
	if (S_ISREG (st.st_mode)) {
		todo(link);
	} else if(S_ISDIR (st.st_mode)) {
		DIR *dp;
		if((dp = opendir(link.c_str())) == NULL) {
			throw EX(link,EX ::CANT_OPEN_DIR);
		}
		struct dirent *de;
		while((de=readdir(dp)) != NULL) {
			string file(de->d_name);
			if(file=="." || file=="..") {continue;}
			string full(link);
			full+="/";
			full+=file;
			removeLink<EX>(full,todo);
		}
		closedir(dp);
		// TODO: shred directory information on shred, this just unlinks it
		todo(link);
	} else {
		throw EX(link,EX ::UNKOWN_TYPE);
	}
}

void unlinkFile(const string& unlinkable)
{
	{
		struct stat s;
		if(stat(unlinkable.c_str(),&s)!=0)
			throw CanNotUnlinkFile(unlinkable,CantRemoveFile::DOESNOT_EXIST);
	}
	chmod(unlinkable.c_str(),0777);
	if(unlink(unlinkable.c_str())!=0)
	if(rmdir(unlinkable.c_str())!=0)
		throw CanNotUnlinkFile(unlinkable,CantRemoveFile::NOT_UNLINKING);
}

void PhysicalTrashCan::unlink(const string& trashname)
{
	{
		string unlinkable=filesdir();
		unlinkable+="/";
		unlinkable+=trashname;
		removeLink<CanNotUnlinkFile>(unlinkable,&unlinkFile);
	}
	{
		InfoFile ifile(infodir(),trashname);
		string unlinkable=ifile.infoFileName();
		removeLink<CanNotUnlinkFile>(unlinkable,&unlinkFile);
	}
}

void shredFile(const string& shredable)
{
	chmod(shredable.c_str(),0777);
	long filesize;
	{
		ifstream file (shredable.c_str(),ifstream::binary);
		file.seekg(0,ifstream::end);
		filesize=file.tellg();
		file.seekg(0);
		if(!file.good()) {
			throw CanNotShredFile(shredable,CanNotShredFile::READ_SIZE);
		}
	}
	{
		ofstream file(shredable.c_str(),ofstream::binary);
		srand( time(NULL) );
		for(int i=0;i<filesize;i++) {
			char toWrite=rand();
			file.write(&toWrite,1);
		}
		if(!file.good()) {
			throw CanNotShredFile(shredable,CanNotShredFile::WRITE_GARBAGE);
		}
	}
}

void shredLink(const string& link)
{
	removeLink<CanNotShredFile>(link,&shredFile);
}

void PhysicalTrashCan::shred(const string& trashname)
{
	string shredable=filesdir();
	shredable+="/";
	shredable+=trashname;
	shredLink(shredable);
	unlink(trashname);
}

void PhysicalTrashCan::restore(const string& trashname)
{
	InfoFile info(infodir(),trashname);
	{
		string source(filesdir());
		source+="/";
		source+=trashname;
		{
			struct stat st;
			if(stat(info.filename().c_str(), &st)==0) {
				beforeRestoringOnFile_(this,info.filename());
			}
		}
		moverMethod_(source,info.filename());
	}
	{
		string unlinkable(info.infoFileName());
		unlinkFile(info.infoFileName());
	}	
}

void cleanDir(PhysicalTrashCan* can,DIRECTORY clean,DIRECTORY look)
{
	DIR* dp;
	if((dp = opendir((can->*clean)().c_str())) == NULL) {
		throw TrashInfoReadError(TrashInfoReadError::CANT_OPEN_DIR);
	}
	struct dirent *de;
	while((de=readdir(dp)) != NULL) {
		string file(de->d_name);
		if(file=="." || file=="..") {continue;}
		{
			struct stat st;
			string l((can->*look)());
			l+="/";
			l+=file;
			if(stat(l.c_str(),&st)==0) {continue;}
		}
		{
			string unlinkable((can->*clean)());
			unlinkable+="/";
			unlinkable+=file;
			unlinkFile(file);
		}	
	}
	closedir(dp);
}

void PhysicalTrashCan::cleanup()
{
	cleanDir(this,&PhysicalTrashCan::infodir,&PhysicalTrashCan::filesdir);
	cleanDir(this,&PhysicalTrashCan::filesdir,&PhysicalTrashCan::infodir);
}

namespace candelegate {

void unlink(TrashCan& tc,const string& n) {tc.unlink(n);}
void add(TrashCan& tc,const string& n) {tc.add(n);}
void shred(TrashCan& tc,const string& n) {tc.shred(n);}
void restore(TrashCan& tc,const string& n) {tc.restore(n);}
void cleanup(TrashCan& tc,const string& ignored) {tc.cleanup();}

string textForCommand(delegate d)
{
	if(&unlink==d) {
		return "delete";
	} else if(&shred==d) {
		return "shred";
	} else if(&restore==d) {
		return "restore";
	} else if(&cleanup==d) {
		return "cleanup";
	} else {
		return "unknown";
	}
}

string textForCommand(Command c)
{
	switch(c) {
	case UNLINK: return "unlink";
	case ADD: return "add";
	case SHRED: return "shred";
	case RESTORE: return "restore";
	case CLEANUP: return "cleanup";
	default: return "unknown";
	}
}
}

}

