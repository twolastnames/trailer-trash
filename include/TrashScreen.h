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
 * File:		TrashScreen.h
 * Description:	the ncurses trash can
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 31 2010
 **********************************************************************************************/

#ifndef TRASHSCREEN_H_
#define TRASHSCREEN_H_

#include <map>

#include "NcursesUtil.h"
#include "Common.h"
#include "TrashCan.h"
#include "CanSet.h"
#include "JobQueue.h"

namespace trailer {
/**
 * a trash can display
 */
class TrashScreen : public Screen, public trashevent::EventListener {
public:
	TrashScreen(JobScheduler* scheduler, TrashCan* tc, const ColorDefinitions& cd);
	~TrashScreen() {pthread_mutex_destroy(&messageListener_.lock);}
	JobListener* listener() {return &listener_;}
	MessageListener* messages() {return &messageListener_;}
	void add(const string& trashname);
	void schedule(const string& trashname);
	void unschedule(const string& trashname);
	void remove(const string& trashname);
	void undefined(const char event,const string& trashname);
	void eventListener(trashevent::EventListener& e) {events_=&e;}
protected:
	/**
	 * redraws whole screen
	 */
	void redraw();
	void execute();
private:
	trashevent::DummyEventListener dummyEvents_;
	trashevent::EventListener* events_;
	bool running_;
	TrashScreen(const TrashScreen&);
	TrashScreen& operator = (const TrashScreen&);
	void updateTrashContents();
	struct : public MessageListener {
		void operator() (const string& message)
		{
			screen->addMessage(message);
		}
		TrashScreen *screen;
		pthread_mutex_t lock;
	} messageListener_;
	struct : public TrashItemListener {
		void handleTrashItem(const TrashItem& ti)
		{
			items.push_back(TrashItemCopy(ti));
			size++;
		}
		// the order they are displayed
		vector<TrashItemCopy*> order;
		// the physical items
		list<TrashItemCopy> items;
		// indexes the items
		map<string,int> names;
		TrashScreen *screen;
		size_t size;
	} trash_;
	struct : public JobListener	{
		void onEnterWait(const Job& j)
		{
			if(screen->trash_.names.find(j.trash)!=screen->trash_.names.end()) {
				int index=static_cast<int>(screen->trash_.names[j.trash]);
				if(screen->scheduled_.find(index)==screen->scheduled_.end()) {
					screen->scheduled_.insert(index);
				}
				if(screen->selected_.find(index)!=screen->selected_.end()) {
					screen->selected_.erase(index);
				}
				screen->ensureEntryUpdated(index);
			}
			screen->refresh();
		}
		void onDoneWait(const Job& j)
		{
		}
		void onStartAction(const Job& j)
		{
		}
		void onEndAction(const Job& j)
		{
			if(
					j.action==&candelegate::restore ||
					j.action==&candelegate::unlink ||
					j.action==&candelegate::shred) {
				screen->events_->remove(j.trash);
			}
			screen->removeJob(j);
		}
		void onCancel(const Job& j)
		{
			if(
					j.action==&candelegate::restore ||
					j.action==&candelegate::unlink ||
					j.action==&candelegate::shred) {
				screen->events_->remove(j.trash);
			}
			screen->removeJob(j);
		}
		TrashScreen *screen;
	} listener_;
	/** selected values */
	set<int> selected_;
	/** scheduled values */
	set<int> scheduled_;
	/** first entry shown */
	int focus_;
	/** where user is pointing */
	int active_;

	int maxy_;
	int maxx_;
	void removeJob(const Job& j);
	void refreshEntries();
	void moveActive(int oldactive);
	void doAllSelected(candelegate::delegate d);
	void invertSelections();
	void updateInfo();
	const int lastEntry() const {return static_cast<const int>(trash_.size-1);}
	void setPage();
	const int lastShown() const;
	const int maxDisplayed() const {return fileEnd()-fileStart();}
	void ensureEntryUpdated(const int entry);
	const int fileStart() const {return menuSize();}
	const int fileEnd() const {return maxy_-infoSize();}
	const int menuSize() const {return 1;}
	const int infoSize() const {return 1;}
	void drawMenu();
	void removeIndex(int index);
	void addMessage(const string& m);
	void handleNewItems();
	queue<string> messages_;
};
}

#endif /* TRASHSCREEN_H_ */
