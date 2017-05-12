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
 * File:		trailer.cpp
 * Description:	the full feature executable
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#include <iostream>
#include <vector>

#include "config.h"
#include "CanSet.h"
#include "CommandLine.h"
#include "UserConfiguration.h"

#ifdef HAVE_LIBPTHREAD
#ifdef HAVE_LIBNCURSES
#include "FullScreen.h"
#include "ConsoleExecuter.h"
#endif
#endif

using namespace std;
using namespace trailer;

#define EXE_NAME "trailer"

#define COMMAND_ADD "put"
#define COMMAND_RESTORE "restore"
#define COMMAND_LIST "list"
#define COMMAND_UNLINK "unlink"
#define COMMAND_SHRED "shred"
#define COMMAND_CLEANUP "cleanup"
#define COMMAND_FULLSCREEN "full"
#define COMMAND_HELP "help"

#ifdef HAVE_LIBNCURSES
const char* DEFAULT_COLOR_FILE="colors.xml";
#endif

const char* MAIN_HELP=
	"Usage:  " EXE_NAME " command [options] ...\n"
	"If the intent is not to script/alias/admin " PACKAGE_STRING "\n"
	"look into using the executable 'tt'\n"
	"== Options ==\n"
	"-o force use of home trash can env var(XDG_DATA_HOME) or default\n"
	"-c set the custom directory filename (each line= base:can)\n"
	"-f set the trash can list filename, each line is the location of a known trash can\n"
#ifdef HAVE_LIBNCURSES
	"-C color file, default colors use '', otherwise a default will be generated\n"
#endif
	"== Commands ==\n"
	COMMAND_ADD ":  " EXE_NAME " " COMMAND_ADD  " filename ...\n"
	"  adds file(s) to a trash can\n"
	"  where filename and following arguments will be moved to the trash can\n"
	COMMAND_LIST ":  " EXE_NAME " " COMMAND_LIST "\n"
	"  lists the contents of the trash can\n"
	COMMAND_UNLINK ": " EXE_NAME " " COMMAND_UNLINK " code ...\n"
	"  turns a trash can item into free file space\n"
	"	 where code(s) is the content code returned by a \"list\" command\n"
	COMMAND_SHRED ":  " EXE_NAME " " COMMAND_SHRED " code ...\n"
	"  turns a trash can item into free file space composed of random data\n"
	"	 where code(s) is the content code returned by a \"list\" command\n"
	COMMAND_CLEANUP ":  " EXE_NAME " " COMMAND_CLEANUP "\n"
	"  turns orphaned/corrupted data in the trash can into free file space\n"
#ifdef HAVE_LIBNCURSES
	COMMAND_FULLSCREEN ":  " EXE_NAME " " COMMAND_FULLSCREEN "\n"
	"  enters full screen mode\n"
#endif
	COMMAND_HELP ":  " EXE_NAME " " COMMAND_HELP "\n"
	"  show this message and exit (ignores options)\n"
;

int main(int argc, const char** argv) {
#ifdef HAVE_LIBNCURSES
	console::FullScreenArgument reportArgument;
	FileAndPathFinder dcf(DEFAULT_COLOR_FILE);
	reportArgument.colorFile=dcf.best();
#endif
	struct : public TrashCanFactory {
		void ensureConfig()
		{
			if(customFile.size()<1) {
				FileAndPathFinder fapf("customCanMappings");
				customFile=fapf.best();
			}
			if(fallbackFile.size()<1) {
				FileAndPathFinder fapf("canList");
				fallbackFile=fapf.best();
			}
		}
		TrashCan* trashcan()
		{
			ensureConfig();
			fileCustomDirectory.reset(new FileCustomDirectory (customFile));
			fileTrashDirectorys.reset(new FileTrashDirectories (fallbackFile));
			return new CustomToHomeToList
				(*fileCustomDirectory.get(),*fileTrashDirectorys.get());
		}
		auto_ptr<FileCustomDirectory> fileCustomDirectory;
		auto_ptr<FileTrashDirectories> fileTrashDirectorys;
		string customFile;
		string fallbackFile;
	} customHomeList;
#ifndef DEBUG
	try {
#endif
		if(argc<2) {
			cerr << MAIN_HELP;
			return 1;
		}

		const string command(argv[1]);
		auto_ptr<TrashCan> trash;
		string trashCanLocation;
		{
			int c;
			char** base=const_cast<char**>(argv);
			base++;
#ifdef HAVE_LIBNCURSES
			while((c = getopt (argc-1, base,"c:f:oC:")) != -1) {
#else
			while((c = getopt (argc-1, base,"c:f:o")) != -1) {
#endif
				switch (c) {
				case 'c':
					customHomeList.customFile=optarg;
					break;
				case 'f':
					customHomeList.fallbackFile=optarg;
					break;
#ifdef HAVE_LIBNCURSES
				case 'C':
					reportArgument.colorFile=optarg;
					break;
#endif
				case 'o':
					EncapsulatingHomeCan* can=new EncapsulatingHomeCan();
					can->moveMethod(&tryAnythingMove);
					trash.reset(can);
					break;
				}
			}
		}
		if(trash.get()==0) trash.reset(customHomeList.trashcan());
		list<string> lineArgs;
		for (int i=optind + 1; i< argc; i++ ) {
			lineArgs.push_back(argv[i]);
		}
		reportArgument.customDirectory=customHomeList.fileCustomDirectory.get();
		reportArgument.trashDirectories=customHomeList.fileTrashDirectorys.get();
		AllKnownTrashCans aktc(
				*customHomeList.fileTrashDirectorys.get(),
				*customHomeList.fileCustomDirectory.get());
		if(command==COMMAND_ADD) {
			commandline::add(*trash.get(),aktc,lineArgs.begin(),lineArgs.end());
		} else if(command==COMMAND_LIST) {
			commandline::listTrashContents(*trash.get());
		} else if(command==COMMAND_UNLINK) {
			commandline::removeTrashItem(*trash.get(),aktc,&candelegate::unlink,lineArgs.begin(),lineArgs.end());
		} else if(command==COMMAND_RESTORE) {
			commandline::removeTrashItem(*trash.get(),aktc,&candelegate::restore,lineArgs.begin(),lineArgs.end());
		} else if(command==COMMAND_SHRED) {
			commandline::removeTrashItem(*trash.get(),aktc,&candelegate::shred,lineArgs.begin(),lineArgs.end());
		} else if(command==COMMAND_CLEANUP) {
			trash.get()->cleanup();
#ifdef HAVE_LIBNCURSES
		} else if(command==COMMAND_FULLSCREEN) {
			console::fullScreenTrash(*trash.get(),&reportArgument);
#endif
		} else if (command==COMMAND_HELP) {
			cout << MAIN_HELP << endl;
			return 0;
		} else {
			cerr << "Command [" << command << "] not recognized\n";
			cerr << MAIN_HELP;
			return 1;
		}
#ifndef DEBUG
	} catch (const CanNotMoveToTrash& e) {
		cerr << "Can not move to trash with error " << e.error << endl;
		return 1;
	} catch (const remote::NetworkingError& e) {
		cerr << "Network Error (probably a busy network)" << endl;
		return 1;
	} catch (const FileToTrashDoesNotExist& e) {
		cerr << "File to move to trash [" << e.filename << "] does not exist" << endl;
		return 1;
	} catch  (const CantMakeDirectory& e) {
		cerr << "Can't make directory [" << e.directory << "]" << endl;
		return 1;
	} catch (const CanNotCreateTrashInfo& e) {
		cerr << "CanNotCreateTrashInfo Error" << endl;
	} catch (const CanNotGetCWD& e) {
		cerr << "CanNotGetCWD " << e.error << endl;
	} catch (const CanNotMoveFile& e) {
		cerr << "CanNotMoveFile from [" << e.source << "] to [" << e.destination <<
				"] because of " << e.error << endl;
	} catch (const CantRemoveFile& e) {
		cerr << "can't remove [" << e.file << "] because of " << e.error << endl;
	} catch (const InvalidTrashNameFormat& e) {
		cerr << "InvalidTrashNameFormat " << e.name << endl;
	} catch (const TrashInfoReadError& e) {
		cerr << "TrashInfoReadError " << e.error << endl;
	} catch (const NoDirectoryForTarget& e) {
		cerr << "Can not find directory to put [" << e.target << "]" << endl;
	} catch (const TrashItemNotSet& e) {
		cerr << "TrashItemNotSet" << endl;
	} catch (const ExceptionBase& e) {
		cerr << "Undefined Error" << endl;
		return 2;
	}
#endif
	return 0;
}

