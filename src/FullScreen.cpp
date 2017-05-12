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
 * File:		FullScreen.cpp
 * Description:	the ncurses implementation
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#include <sstream>
#include <list>
#include <set>

#include "FullScreen.h"

using namespace std;

namespace trailer {

void inactiveRefresher(WINDOW* w) {}
void activeRefresher(WINDOW* w) {wrefresh(w);}

void FullScreenInterface::execute()
{
	keypad(window_, TRUE);
	// starting with trash screen
	UserRequestedScreenChange::SCREEN screen=UserRequestedScreenChange::TRASH;
	while (true) {
		try {
			switch(screen) {
			case UserRequestedScreenChange::TRASH :
				trash_.window(window_);
				trash_.go();
				break;
			case UserRequestedScreenChange::HELP :
				help_.window(window_);
				help_.go();
				break;
			case UserRequestedScreenChange::EXIT :
				scheduler_.close();
				return;
			};
		} catch (const UserRequestedScreenChange& e) {
			screen=e.screen;
		}
	}
}

HelpScreen::HelpScreen(WINDOW* w, JobScheduler* scheduler, TrashCan* tc, const ColorDefinitions& cd) :
		Screen(w,scheduler,tc,cd)
{

}

void HelpScreen::execute()
{
	{
		getmaxyx(window(),maxy_,maxx_);
		redraw();
	}
	while (true) {
		int c=getch();
		{
			switch(c) {
			case KEY_RESIZE:
				redraw();
				break;
			case KEY_F(FULL_SCREEN_F_COMMAND_ToTrash):
				throw UserRequestedScreenChange(UserRequestedScreenChange::TRASH);
			case KEY_F(FULL_SCREEN_F_COMMAND_Quit):
				throw UserRequestedScreenChange(UserRequestedScreenChange::EXIT);
			}
		}
	}
}

#define WriteNextLine(writeline) \
if(line++<maxy_) { \
	ostringstream out; \
	out << writeline; \
	string l(out.str()); \
	setStringToSize(l,maxx_); \
	ScopedColor c(color().inactive()); \
	printw(l.c_str());		\
}

void HelpScreen::redraw()
{
	getmaxyx(window(),maxy_,maxx_);
	int line=0;
	werase(window());
	if(line++<maxy_) {
		ostringstream out;
		out << "F" << FULL_SCREEN_F_COMMAND_ToTrash << ":To Trash | ";
		out << "F" << FULL_SCREEN_F_COMMAND_Quit << ":Quit" << endl;
		{
			string l(out.str());
			setStringToSize(l,maxx_);
			ScopedColor c(color().menu());
			printw(out.str().c_str());
		}
	}
	if(line++<maxy_) {
		string l;
		setStringToSize(l,maxx_);
		ScopedColor c(color().inactive());
		printw(l.c_str());
	}
	if(line++<maxy_) {
		ScopedAttribute bold(A_BOLD);
		int packLen=static_cast<int>(strlen(PACKAGE_STRING));
		string l;
		if((maxx_-packLen)/2 + packLen > maxx_) {
			setStringToSize(l,(maxx_-packLen)/2);
		}
		l+=PACKAGE_STRING;
		setStringToSize(l,maxx_);
		wprintw(window(),l.c_str());
	}
	if(line++<maxy_) {
		string l;
		setStringToSize(l,maxx_);
		ScopedColor c(color().inactive());
		printw(l.c_str());
	}
	WriteNextLine("Up/Down Arrow: change selection");
	WriteNextLine("Page Up/Down: move a page");
	WriteNextLine("Space Bar: select/unselect");
	WriteNextLine("Escape: clear status message");
	WriteNextLine("F" << FULL_SCREEN_F_COMMAND_Invert << ": invert selected entries");
	WriteNextLine("F" << FULL_SCREEN_F_COMMAND_Restore << ": restore selected files");
	WriteNextLine("F" << FULL_SCREEN_F_COMMAND_Refresh << ": refresh trash can");
	WriteNextLine("F" << FULL_SCREEN_F_COMMAND_Delete << ": delete selected files");
	WriteNextLine("F" << FULL_SCREEN_F_COMMAND_Shred << ": shred selected files");
	WriteNextLine("F" << FULL_SCREEN_F_COMMAND_Quit << ": quit");
	while(line++<maxy_) {
		string l;
		setStringToSize(l,maxx_);
		ScopedColor c(color().inactive());
		printw(l.c_str());
	}
	wrefresh(window());
}
#undef WriteNextLine

}
