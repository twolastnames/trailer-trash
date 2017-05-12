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
 * File:		NcursesUtil.h
 * Description:	ncurses utility
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 31 2010
 **********************************************************************************************/

#ifndef NCURSESUTIL_H_
#define NCURSESUTIL_H_

#include "config.h"

#ifdef __CYGWIN__
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif

#include "TrashCan.h"
#include "JobQueue.h"
#include "ScreenColors.h"

#define FULL_SCREEN_F_COMMAND_ToTrash 2

#define FULL_SCREEN_F_COMMAND_Help    1
#define FULL_SCREEN_F_COMMAND_Invert  3
#define FULL_SCREEN_F_COMMAND_Restore 4
#define FULL_SCREEN_F_COMMAND_Refresh 5
#define FULL_SCREEN_F_COMMAND_Delete  7
#define FULL_SCREEN_F_COMMAND_Shred   8
#define FULL_SCREEN_F_COMMAND_Quit   12

namespace trailer {

typedef short color_t;
typedef int attrib_t;

struct UserRequestedScreenChange : public ExceptionBase {
	enum SCREEN {EXIT,HELP,TRASH};
	UserRequestedScreenChange(const SCREEN s) : screen(s) {}
	/** Intentionally not implemented */
	UserRequestedScreenChange& operator = (const UserRequestedScreenChange& c);
	const SCREEN screen;
};

/**
 * holds an ncurses attribute for the duration of scope
 */
class ScopedAttribute {
public:
	ScopedAttribute(const attrib_t a) : attrib_(a) {attron(attrib_);}
	~ScopedAttribute() {attroff(attrib_);}
private:
	ScopedAttribute(const ScopedAttribute&);
	ScopedAttribute& operator = (const ScopedAttribute&);
	const attrib_t attrib_;
};

inline void setStringToSize(string& s, int size)
{
	int diff=static_cast<size_t>(size)-s.size();
	if(diff>0) s.append(diff,' ');
	else s=s.substr(0,size);
}

/** to hold an ncurses color for the duration of scope */
class ScopedColor {
public:
	ScopedColor(const color_t pair) : pair_(pair) {if(has_colors()) attron(COLOR_PAIR(pair_));}
	~ScopedColor() {if(has_colors()) attroff(pair_);}
private:
	ScopedColor(const ScopedColor&);
	ScopedColor& operator = (const ScopedColor&);
	const color_t pair_;
};

/** types for refreshing a window */
typedef void (*Refresher)(WINDOW*);

/** does nothing */
void inactiveRefresher(WINDOW* w);

/** invokes a refresh the ncurses window */
void activeRefresher(WINDOW* w);

/**
 * a screen for full screen mode
 */
class Screen {
public:
	Screen(JobScheduler* scheduler, TrashCan* tc,const ColorDefinitions& color) :
		window_(stdscr), lastWindow_(NULL), scheduler_(scheduler),
		trashcan_(tc), color_(&color), refresher_(&inactiveRefresher)
		{pthread_mutex_init(&lock_,NULL);}
	Screen(WINDOW* window, JobScheduler* scheduler, TrashCan* tc,const ColorDefinitions& color) :
		window_(window), lastWindow_(NULL), scheduler_(scheduler), trashcan_(tc),
		color_(&color), refresher_(&inactiveRefresher)
		{pthread_mutex_init(&lock_,NULL);}
	virtual ~Screen() {pthread_mutex_destroy(&lock_);}
	/**
	 * will gather input and update screen accordingly
	 * @throws UserRequestedScreenChange when a user requests a screen change
	 */
	void go()
	{
		{
			MutexLock l(lock());
			refresher_=&activeRefresher;
		}
		execute();
		{
			MutexLock l(lock());
			refresher_=&inactiveRefresher;
		}
	}
	/**
	 * @param w window to use
	 */
	void window(WINDOW* w) {window_=w;}
	/**
	 * accesses where this where this opject is listening
	 */
	virtual JobListener* listener()=0;
	/**
	 * to set color definitions
	 */
	void color(const ColorDefinitions& color) {color_=&color;}
protected:
	/**
	 * the mutex for this screen
	 */
	pthread_mutex_t& lock() {return lock_;}
	/**
	 * where to implement for reacting to user input
	 */
	virtual void execute()=0;
	/**
	 * use for refreshes
	 */
	void refresh() {refresher_(window());}
	/**
	 * window for refreshing/info and such
	 */
	WINDOW* window()
	{
		if(window_!=lastWindow_) {
			lastWindow_=window_;
			redraw();
		}
		return window_;
	}
	/**
	 * will redraw the screen from scratch
	 */
	virtual void redraw()=0;
	/**
	 * @return the trash can being used
	 */
	TrashCan& trashcan() {return *trashcan_;}
	/**
	 * @return the scheduler being used
	 */
	JobScheduler* scheduler() {return scheduler_;}
	/**
	 * to access colors to use
	 */
	const ColorDefinitions& color() {return *color_;}
private:
	WINDOW* window_;
	WINDOW* lastWindow_;
	JobScheduler* scheduler_;
	TrashCan* trashcan_;
	const ColorDefinitions* color_;
	Refresher refresher_;
	pthread_mutex_t lock_;
};

inline void shutdownNcurses()
{
	echo();
	endwin();
}

class NcursesSetup {
public:
	NcursesSetup()
	{
		initscr();
		cbreak();
		noecho();
	}
	~NcursesSetup() { shutdownNcurses(); }
private:
	NcursesSetup(const NcursesSetup&);
	NcursesSetup& operator = (const NcursesSetup&);
};

}

#endif /* NCURSESUTIL_H_ */
