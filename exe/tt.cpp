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
 * File:		tt.cpp
 * Description:	the simple executable
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#include "config.h"
#include "CanSet.h"
#include "UserConfiguration.h"
#include "CommandLine.h"
#include "ConsoleExecuter.h"

#ifdef HAVE_LIBNCURSES
#include "FullScreen.h"
#endif

using namespace trailer;
using namespace std;

#define EXE_NAME "tt"

const char* DEFAULT_COLOR_FILE="colors.xml";

void printHelp(ostream& o)
{
	o <<
	PACKAGE_STRING << endl <<
	"Usage:  " << EXE_NAME << " [options] [trashables]" << endl <<
	"Where trashables are files to be put in trash" << endl <<
#ifdef HAVE_LIBNCURSES
	"Without trashables the trash can display will open" << endl <<
#endif
	"== Options ==" << endl <<
	"-o force use of home trash can env var(XDG_DATA_HOME) or default" << endl <<
	"-c set the custom directory filename (each line= base:can)" << endl <<
	"-f set the trash can list filename, each line is the location of a known trash can" << endl <<
#ifdef HAVE_LIBNCURSES
	"-C color file, default colors use ''" << endl <<
#endif
	"-? displays this help message" << endl
#ifdef HAVE_LIBNCURSES
	<<
	"== Trash Can Screen Buttons ==" << endl <<
	"Up/Down Arrow: change selection" << endl <<
	"Page Up/Down: move a page" << endl <<
	"Space Bar: select/unselect" << endl <<
	"F" << FULL_SCREEN_F_COMMAND_Invert << ": invert selected entries" << endl <<
	"F" << FULL_SCREEN_F_COMMAND_Restore << ": restore selected files" << endl <<
	"F" << FULL_SCREEN_F_COMMAND_Refresh << ": refresh trash can" << endl <<
	"F" << FULL_SCREEN_F_COMMAND_Delete << ": delete selected files" << endl <<
	"F" << FULL_SCREEN_F_COMMAND_Shred << ": shred selected files" << endl <<
	"F" << FULL_SCREEN_F_COMMAND_Quit << ": quit" << endl <<
	"Escape: clear status message" << endl
#endif
	;
}

int main(int argc, const char** argv)
{
#ifdef HAVE_LIBNCURSES
	auto_ptr<ColorDefinitions> colorDefinitions;
	string colorsFile;
	{
		FileAndPathFinder fapf(DEFAULT_COLOR_FILE);
		colorsFile=fapf.best();
	}
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
	auto_ptr<TrashCan> trash;
	{
		int c;
		char** base=const_cast<char**>(argv);
#ifdef HAVE_LIBNCURSES
		while((c = getopt (argc, base,"c:f:o?C:")) != -1) {
#else
		while((c = getopt (argc, base,"c:f:o?")) != -1) {
#endif
			if('c'==c) {
				customHomeList.customFile=optarg;
			}else if ('f'==c) {
				customHomeList.fallbackFile=optarg;
#ifdef HAVE_LIBNCURSES
			}else if ('C'==c) {
				string colorsFile=optarg;
#endif
			} else if('o'==c) {
				EncapsulatingHomeCan* can=new EncapsulatingHomeCan();
				can->moveMethod(&tryAnythingMove);
				trash.reset(can);
			} else if('?'==c) {
				printHelp(cout);
				return 0;
			} else {
				cerr << "Unknown Option" << endl;
				printHelp(cerr);
				return 1;
			}
		}
	}
	if(trash.get()==0) trash.reset(customHomeList.trashcan());
#ifdef HAVE_LIBNCURSES
	if(optind>=argc) {
		try {
			console::FullScreenArgument arg;
			arg.messages.push_back("For command line help use -? switch at command line");
			arg.colorFile=colorsFile;
			arg.customDirectory=customHomeList.fileCustomDirectory.get();
			arg.trashDirectories=customHomeList.fileTrashDirectorys.get();
			console::fullScreenTrash(*trash.get(),&arg);
		} catch (const ExceptionBase& e) {return 2;}
		return 0;
	}
#endif
#ifndef DEBUG
	try {
#endif
		list<string> trashables;
		for (int i=optind; i< argc; i++ ) {
			trashables.push_back(argv[i]);
		}
		AllKnownTrashCans aktc(
				*customHomeList.fileTrashDirectorys.get(),
				*customHomeList.fileCustomDirectory.get());
		commandline::add(*trash.get(),aktc,trashables.begin(),trashables.end());
		return 0;
#ifndef DEBUG
	} catch (const remote::NetworkingError& e) {
		cerr << "Network Error (probably a busy network)" << endl;
	} catch (const CanNotMoveToTrash& e) {
		cerr << "Can not move to trash with error " << e.error << endl;
	} catch (const FileToTrashDoesNotExist& e) {
		cerr << "File to move to trash [" << e.filename << "] does not exist" << endl;
	} catch  (const CantMakeDirectory& e) {
		cerr << "Can't make directory [" << e.directory << "]" << endl;
	} catch (const CanNotCreateTrashInfo& e) {
		cerr << "CanNotCreateTrashInfo Error" << endl;
	} catch (const CanNotGetCWD& e) {
		cerr << "CanNotGetCWD " << e.error << endl;
	} catch (const CanNotMoveFile& e) {
		cerr << "CanNotMoveFile from [" << e.source << "] to [" << e.destination <<
				"] because of " << e.error << endl;
	} catch (const CantRemoveFile& e) {
		cerr << "can't remove [" << e.file << "] because of " << e.error << endl;
	} catch (const NoDirectoryForTarget& e) {
		cerr << "Can not find directory to put [" << e.target << "]" << endl;
	} catch (const InvalidTrashNameFormat& e) {
		cerr << "InvalidTrashNameFormat " << e.name << endl;
	} catch (const TrashInfoReadError& e) {
		cerr << "TrashInfoReadError " << e.error << endl;
	} catch (const TrashItemNotSet& e) {
		cerr << "TrashItemNotSet" << endl;
	} catch (const ExceptionBase& e) {
		cerr << "Undefined Error" << endl;
		return 2;
	}
#endif
	return 1;
}
