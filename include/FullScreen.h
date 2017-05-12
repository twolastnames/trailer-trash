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

#ifndef FULLSCREEN_H_
#define FULLSCREEN_H_

#include <ostream>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <stack>
#include <queue>

#include <sys/stat.h>

#include "config.h"

#ifdef __CYGWIN__
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#endif

#include "Common.h"
#include "TrashCan.h"
#include "JobQueue.h"
#include "UserConfiguration.h"
#include "NcursesUtil.h"
#include "ScreenColors.h"
#include "TrashScreen.h"

using namespace std;

// for resize
// SIGWINCH signal, it pushes a KEY_RESIZE

namespace trailer {

/**
 * Screen for help
 */
class HelpScreen : public Screen {
public:
	HelpScreen(WINDOW* w, JobScheduler* scheduler, TrashCan* tc, const ColorDefinitions& cd);
	JobListener* listener() {return &listener_;}
protected:
	void execute();
	void redraw();
private:
	DummyJobListener listener_;
	int maxy_;
	int maxx_;
};

/**
 * for executing the full screen
 */
class FullScreenInterface {
public:
	FullScreenInterface(TrashCan* can, WINDOW* window, const ColorDefinitions& colors) :
		currentScreen_(TRASHCAN),
		can_(can),
		scheduler_(can_,&listener_,2),
		window_(window),
		trash_(&scheduler_,can,colors),
		help_(window,&scheduler_,can,colors),
		colors_(colors)
	{
		listener_.push_back(trash_.listener());
		messageListener_.push_back(trash_.messages());
	}
	void execute();
	TrashScreen& trashScreen() {return trash_;}
	JobScheduler& scheduler() {return scheduler_;}
	MessageListener& messages() {return messageListener_;}
private:
	struct : public JobListener, list<JobListener*> {
		void onEnterWait(const Job& j)
		{
			for(list<JobListener*>::iterator i=begin();i!=end();i++) (*i)->onEnterWait(j);
		}
		void onDoneWait(const Job& j)
		{
			for(list<JobListener*>::iterator i=begin();i!=end();i++) (*i)->onDoneWait(j);
		}
		void onStartAction(const Job& j)
		{
			for(list<JobListener*>::iterator i=begin();i!=end();i++) (*i)->onStartAction(j);
		}
		void onEndAction(const Job& j)
		{
			for(list<JobListener*>::iterator i=begin();i!=end();i++) (*i)->onEndAction(j);
		}
		void onCancel(const Job& j)
		{
			for(list<JobListener*>::iterator i=begin();i!=end();i++) (*i)->onCancel(j);
		}
	} listener_;
	struct : public MessageListener, list<MessageListener*> {
		void operator() (const string& message)
		{
			for(list<MessageListener*>::iterator i=begin();i!=end();i++) (*(*i))(message);
		}
	} messageListener_;
	enum SCREEN {TRASHCAN,FILESYSTEM,STATUS};
	SCREEN currentScreen_;
	TrashCan* can_;
	JobScheduler scheduler_;
	WINDOW *window_;
	TrashScreen trash_;
	HelpScreen help_;
	const ColorDefinitions& colors_;
};

}

#endif
