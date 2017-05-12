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
 * File:		TrashCan.h
 * Description:	implements a trash can based on a file directory base
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#ifndef TRASHCAN_H_
#define TRASHCAN_H_

#include <ctime>
#include <cstdlib>

#include <list>
#include <string>
#include <fstream>
#include <cstring>
#include <sstream>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "Common.h"

namespace trailer {

typedef mode_t Permission;

/**
* @return name as a full path name... may be the same
*/
const string ensureFullPathName(const string& name);

/**
 * @base a file system location
 * @return the root directory of base, according to the device it is on
 * @throw FileToTrashDoesNotExist if base does not exist
 */
const string topDeviceDirectory(const string& base);

/**
 * thrown when there is corrupt information in the trash info on the disk
 */
struct TrashInfoReadError : public ExceptionBase {
	enum ERROR {NO_ERROR,MISSING_HEADER,MISSING_DATE,MISSING_NAME,BAD_FILE,CANT_OPEN_DIR};
	TrashInfoReadError(const ERROR& e) : error(e) {}
	const ERROR error;
};

/**
 * how a trash can should be appended to directory
 */
struct DirectoryAttributes {
	virtual ~DirectoryAttributes () {}
	virtual const Permission& permission() const =0;
	virtual const bool check(Permission& p) const =0;
	virtual const bool hidden() const =0;
};

/**
 * how a trash can should act when it is the home can
 */
class HomeDirPermission : public DirectoryAttributes {
public:
	HomeDirPermission() : permission_(0700) {}
	const Permission& permission() const {return permission_;}
	const bool check(Permission& p) const {return S_IRUSR & p && S_IWUSR & p && S_IXUSR & p;}
	const bool hidden() const {return false;}
private:
	const Permission permission_;
};

/**
 * how a trash can should act when it is not the home can
 */
class TopDirPermission : public DirectoryAttributes {
public:
	TopDirPermission() : permission_(0777) {}
	const Permission& permission() const {return permission_;}
	const bool check(Permission& p) const {return (S_IRWXU|S_IRWXG|S_IRWXO) & p;}
	const bool hidden() const {return true;}
private:
  const	Permission permission_;
};

/**
 * thrown when file permissions do not allow for creating/modifying a trash can
 */
struct TrashDirectoryModeError  : public ExceptionBase {
	TrashDirectoryModeError
		(const string& dirname, const string& mode,const bool& exists) :
		directory(dirname), mode(mode), exists(exists) {}
	const string directory;
	const string mode;
	const bool exists;
};

typedef string TimeStamp;

/**
 * information about something in the trash
 */
struct TrashItem {
	virtual ~TrashItem() {}
	virtual const TimeStamp& time() const=0;
	virtual const string& filename() const=0;
	virtual const string& trashname() const=0;
};

/**
 * to store a trash item in the RAM
 */
class TrashItemCopy : public TrashItem {
public:
	TrashItemCopy(const TrashItem& ti) :
		time_(ti.time()), filename_(ti.filename()), trashname_(ti.trashname()) {}
	TrashItemCopy(const TimeStamp time, const string filename, const string trashname) :
		time_(time), filename_(filename), trashname_(trashname) {}
	const TimeStamp& time() const {return time_;}
	const string& filename() const {return filename_;}
	const string& trashname() const {return trashname_;}
	const bool operator < (const TrashItemCopy& a) const
	{
		if(a.filename()!=filename()) return a.filename() > filename();
		if(a.time()!=time()) return a.time() > time();
		return a.trashname() > trashname();
	}
	const bool operator == (const TrashItemCopy& a) const
	{
		return a.trashname()==trashname() &&
				a.filename()==filename() && a.time()==time();
	}
private:
	TimeStamp time_;
	string filename_;
	string trashname_;
};

struct TrashItemCopyFunctor {
	const bool operator () (const TrashItemCopy* const & a,const TrashItemCopy* const & b) const
	{
		return *a < *b;
	}
};

/**
 * thrown when a TrashItem field is not available or legible
 */
struct TrashItemNotSet : public ExceptionBase {};

/**
 *
 */
struct ErrorItem : public TrashItem {
	const TimeStamp& time() const { throw TrashItemNotSet(); }
	const string& filename() const { throw TrashItemNotSet(); }
	const string& trashname() const { throw TrashItemNotSet(); }
};

/**
 * for iterating TrashItem(s)
 */
struct TrashItemListener {
	virtual ~TrashItemListener() {}
	virtual void handleTrashItem(const TrashItem&)=0;
};

/**
any method in this could throw a TrashInfoReadError object
*/
struct TrashCan {
	virtual ~TrashCan() {}
	/**
	@param l to listen to iteration of trash can contents
	*/
	virtual void items(TrashItemListener& l) const=0;
	/**
	@param filename file to move to the trash can
	@return name of the added trash (trashname)
	*/
	virtual const string add(const string& filename)=0;
	/**
	@param trashname content to remove as named by items iteration
	*/
	virtual void unlink(const string& trashname)=0;
	/**
	@param trashname content to shred as named by items iteration
	*/
	virtual void shred(const string& trashname)=0;
	/**
	@param trashname content to restore as named by items iteration
	*/
	virtual void restore(const string& trashname)=0;
	/**
	purges orphan info and trashed files
	*/
	virtual void cleanup()=0;
};

/**
 * The file slated to go to the trash does not exist
 */
struct FileToTrashDoesNotExist  : public ExceptionBase {
	FileToTrashDoesNotExist(const string& file) : filename(file) {}
	const string filename;
};

/**
 * for some reason can't determine the process's working directory
 */
struct CanNotGetCWD : public ExceptionBase {
	CanNotGetCWD(const int& error) : error(error) {}
	/**
	 * system error code describing the reason
	 */
	const int error;
};

typedef void (*FileMover) (const string& from, const string& to);

/**
 * the particular file move can not be done with a rename
 */
struct NonRenamableMove : public ExceptionBase {
	NonRenamableMove(const string& fromfile,const string& tofile) :
		from(fromfile), to(tofile) {}
	const string from;
	const string to;
};

/**
 * Catch all for unsupported errors for problems with physically
 * moving a file to the trash
 */
struct CanNotMoveToTrash : public ExceptionBase {
	CanNotMoveToTrash(const int& error) : error(error) {}
	CanNotMoveToTrash() : error(0) {}
	/**
	 * system error code for what went wrong
	 */
	const int error;
};

/**
 * thrown when a directory can not be found to coincide with a target
 */
struct NoDirectoryForTarget : public ExceptionBase {
	NoDirectoryForTarget(const string& t)
		: target(t) {}
	const string target;
};

/**
 * when the trash information can not be written to disc
 */
struct CanNotCreateTrashInfo : public ExceptionBase {};

/**
 * when a file can not be removed
 */
struct CantRemoveFile : public ExceptionBase {
	enum ERROR {NO_ERROR,READ_SIZE,WRITE_GARBAGE,DOESNOT_EXIST,CANT_OPEN_DIR,NOT_UNLINKING,UNKOWN_TYPE};
	CantRemoveFile(const string& file, const ERROR e) : file(file), error(e) {}
	const string file;
	const ERROR error;
	static const string errorString(const ERROR& e) {
		string reason;
		switch(e) {
		case NO_ERROR:
			reason="No Error";
			break;
		case READ_SIZE:
			reason="Problem Detecting Size of File";
			break;
		case WRITE_GARBAGE:
			reason="Problem Writing to File";
			break;
		case DOESNOT_EXIST:
			reason="File Doesn't Exist";
			break;
		case NOT_UNLINKING:
			reason="Can't Unlink File";
			break;
		default:
			reason="Internal Error";
		};
		return reason;
	}
};

/**
 * when a file can not be moved
 */
struct CanNotMoveFile : public ExceptionBase {
	enum ERROR {NO_ERROR,BAD_SRC,BAD_DEST,SRC_DEL_ERR,UNKOWN_TYPE};
	CanNotMoveFile(const string& src,const string& dest, const ERROR e) :
		source(src), destination(dest), error(e) {}
	const string source;
	const string destination;
	const ERROR error;
};

/**
 * if a file can not be shreded
 */
struct CanNotShredFile : public CantRemoveFile {
	CanNotShredFile(const string& file, const ERROR e) : CantRemoveFile(file,e) {}
};

/**
 * if a file can not be unlinked from the file system
 */
struct CanNotUnlinkFile : public CantRemoveFile {
	CanNotUnlinkFile(const string& file, const ERROR e) : CantRemoveFile(file,e) {}
};

/**
 * moves a file if it can be moved through a rename
 * @arg from location to move file from
 * @arg to location to move file to
 * @throws FileToTrashDoesNotExist if the source file doesn't exist
 * @throws NonRenamableLocation if the files are not in renamable locations
 * 		(like not on same file system)
 * @throws CanNotMoveToTrash any other reason it can't occur
 */
void onlyRenamableMove(const string& from, const string& to);

/**
 * moves a file by copying it and deleting the exisitng file
 * @arg from location to move file from
 * @arg to location to move file to
 * @throws CanNotMoveFile reason in error code
 * @thorws CantRemoveFile if it failed because it couldn't delete the existing file
 */
void copyThenDeleteToMove(const string& from, const string& to);

/**
 * tries all methods it knows of to move a file
 * @arg from location to move file from
 * @arg to location to move file to
 * @throws CanNotMoveFile
 */
void tryAnythingMove(const string& from, const string& to);

/**
 * defined for prototype of time stamp format creator
 */
typedef string (*TimestampCreator) ();

/**
 * gets ISO time stamp to seconds precision
 */
string getLocalSecondsTimestamp();

/**
 * prototype for what to do before restoring on top of an existing file
 */
typedef void (*BeforeRestoringOnFile) (TrashCan* tc, const string filename);

/**
 * for BeforeRestoringExistingFile
 */
inline void toTrashCanMover(TrashCan* tc, const string filename) {tc->add(filename);}

/**
 * The real thing... all virtual trash cans likely will use this after making
 * it's custom decisions
 */
class PhysicalTrashCan : public TrashCan {
public:
	/**
	@throws CantMakeBaseTrashDirectory if it can't create a non-existing trash directory
	@throws TrashDirectoryModeError if there are permission issues using the trash can
	*/
	PhysicalTrashCan(const DirectoryAttributes* attr,const string* baseDir);
	virtual ~PhysicalTrashCan() {}
	void moveMethod(FileMover m) {moverMethod_=m;}
	void timestampCreator(TimestampCreator c) {timestampCreator_=c;}
	void beforeRestoringOnFile(BeforeRestoringOnFile b)
		{beforeRestoringOnFile_=b;}
	/**
	@throws CantMakeBaseTrashDirectory if it can't create a non-existing trash directory
	@throws TrashDirectoryModeError if there are permission issues using the trash can
	@throws CanNotCreateTrashInfo if it cannot read the trash
	*/
	const string add(const string& filename);
	/**
	@throws TrashDirectoryModeError if there are permission issues using the trash can
	*/
	void items(TrashItemListener&) const;
	/**
	@throws TrashDirectoryModeError if there are permission issues using the trash can
	*/
	void unlink(const string& trashname);
	/**
	@throws TrashDirectoryModeError if there are permission issues using the trash can
	*/
	void shred(const string& trashname);
	/**
	@throws TrashDirectoryModeError if there are permission issues using the trash can
	*/
	void restore(const string& trashname);
	/**
	@throws TrashDirectoryModeError if there are permission issues using the trash can
	*/
	void cleanup();
private:
	const string filesdir() const;
	const string infodir() const;
	const string trashdir() const;
	const string infofile(const string& trashname) const;
	void ensureTrashDirectory();
	const DirectoryAttributes* directoryAttr_;
	const string* baseDir_;
	FileMover moverMethod_;
	BeforeRestoringOnFile beforeRestoringOnFile_;
	TimestampCreator timestampCreator_;
};

namespace candelegate {
	enum Command {UNLINK,ADD,SHRED,RESTORE,CLEANUP};
	typedef void (*delegate)(TrashCan& tc,const string&);
	void unlink(TrashCan& tc,const string& n);
	void add(TrashCan& tc,const string& n);
	void shred(TrashCan& tc,const string& n);
	void restore(TrashCan& tc,const string& n);
	void cleanup(TrashCan& tc,const string& ignored);
	string textForCommand(delegate d);
	string textForCommand(Command d);
}

struct TrashCanFactory {
	virtual ~TrashCanFactory() {}
	virtual TrashCan* trashcan()=0;
};

class InfoFile : public TrashItem {
public:
	/**
	@throw TrashInfoReadError
	*/
	InfoFile(const string infodir,const string trashname);
	InfoFile(
		const string infodir,
		const string trashname,
		const string filename,
		const TimeStamp t);
	const TimeStamp& time() const {return stamp_;}
	const string& filename() const {return filename_;}
	const string& trashname() const {return trashname_;}
	const string infoFileName() const;
private:
	const string infodir_;
	const string trashname_;
	string filename_;
	TimeStamp stamp_;
};

}

#endif /* TRASHCAN_H_ */

