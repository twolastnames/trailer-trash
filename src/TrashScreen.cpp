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
 * File:		TrashScreen.cpp
 * Description:	the ncurses trash can
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 31 2010
 **********************************************************************************************/

#include "TrashScreen.h"

namespace trailer {

TrashScreen::TrashScreen(JobScheduler* scheduler, TrashCan* tc, const ColorDefinitions& cd) :
	Screen(scheduler, tc, cd), events_(&dummyEvents_), running_(0), focus_(0), active_(0)
{
	trash_.screen=this;
	listener_.screen=this;
	messageListener_.screen=this;
	trash_.size=0;
	pthread_mutex_init(&messageListener_.lock,NULL);
}

void TrashScreen::refreshEntries()
{
	if (trash_.size>static_cast<size_t>(active_)) {
		size_t oldActive=static_cast<size_t>(active_);
		TrashItemCopy copy(*trash_.order.at(active_));
		updateTrashContents();
		if(trash_.names.find(copy.trashname())!=trash_.names.end()) {
			active_ = trash_.names[copy.trashname()];
		} else if (oldActive>=0 && oldActive<trash_.size) {
			active_ = oldActive;
		} else if(active_>lastEntry()) {
			active_=lastEntry();
		} else {
			active_ = 0;
		}
	} else {
		updateTrashContents();
	}
	if(static_cast<size_t>(active_)>=trash_.size)
		active_=trash_.size -1;
	redraw();
}

void TrashScreen::execute()
{
	struct Running {
		Running(bool& r, pthread_mutex_t& m)
			: run(r), mut(m)
		{
			MutexLock l(mut);
			run=true;
		}
		~Running()
		{
			MutexLock l(mut);
			run=false;
		}
		bool &run;
		pthread_mutex_t& mut;
	};
	Running r(running_,lock());
	{
		getmaxyx(window(),maxy_,maxx_);
		MutexLock l(lock());
		this->updateTrashContents();
		active_=0;
		redraw();
	}
	while (true) {
		int c=getch();
		{
			MutexLock l(lock());
			bool clearMessages=true;
			int oldactive=active_;
			switch(c) {
			case KEY_DOWN:
				oldactive=active_++;
				break;
			case KEY_UP:
				oldactive=active_--;
				break;
			case KEY_NPAGE:
				oldactive=active_;
				active_+=maxDisplayed();
				break;
			case KEY_PPAGE:
				oldactive=active_;
				active_-=maxDisplayed();
				break;
			case ' ':
				if(selected_.find(active_)==selected_.end()) {
					selected_.insert(active_);
				} else {
					selected_.erase(active_);
				}
				break;
			case 27:  // escape
				{
					MutexLock l(messageListener_.lock);
					if(!messages_.empty())messages_.pop();
					clearMessages=false;
				}
				break;
			case KEY_F(FULL_SCREEN_F_COMMAND_Help):
				throw UserRequestedScreenChange(UserRequestedScreenChange::HELP);
				break;
			case KEY_F(FULL_SCREEN_F_COMMAND_Invert):
				invertSelections();
				break;
			case KEY_F(FULL_SCREEN_F_COMMAND_Restore):
				doAllSelected(&candelegate::restore);
				break;
			case KEY_F(FULL_SCREEN_F_COMMAND_Refresh):
				refreshEntries();
				break;
			case KEY_F(FULL_SCREEN_F_COMMAND_Delete):
				doAllSelected(&candelegate::unlink);
				break;
			case KEY_F(FULL_SCREEN_F_COMMAND_Shred):
				doAllSelected(&candelegate::shred);
				break;
			case KEY_F(FULL_SCREEN_F_COMMAND_Quit):
				throw UserRequestedScreenChange(UserRequestedScreenChange::EXIT);
				break;
			case KEY_RESIZE:
				clearMessages=false;
				redraw();
				break;
			default:
				clearMessages=false;
			}
			{
				MutexLock l(messageListener_.lock);
				while(clearMessages && !messages_.empty()) messages_.pop();
			}
			moveActive(oldactive);
			drawMenu();
			refresh();
		}
	}
}

void TrashScreen::invertSelections()
{
	for(int i=0;i<static_cast<int>(trash_.size);i++) {
		if(selected_.find(i)==selected_.end()) {
			selected_.insert(i);
		} else {
			selected_.erase(i);
		}
	}
	redraw();
}

void TrashScreen::addMessage(const string& m)
{
	{
		MutexLock l(messageListener_.lock);
		messages_.push(m);
	}
	{
		MutexLock l(lock());
		if(running_) updateInfo();
	}
	refresh();
}

void TrashScreen::doAllSelected(candelegate::delegate d)
{
	if(selected_.find(active_)==selected_.end()) selected_.insert(active_);
	set<int>::iterator i=selected_.begin();
	stack<int> todo;
	while(i!=selected_.end()) {
		todo.push(*i);
		i++;
	}
	while(!todo.empty()) {
		int selected=todo.top();
		if(trash_.size>static_cast<size_t>(selected)) {
			string trashname=trash_.order.at(selected)->trashname();
			Job job(d,trashname);
			events_->schedule(trashname);
			try {
				scheduler()->schedule(job);
			} catch (const JobAlreadyScheduled& e) {
				string message;
				message+= "Doing [";
				message+=  candelegate::textForCommand(e.job.action);
				message+= "] on [";
				message+= e.job.trash;
				message+= "] already scheduled";
				addMessage(message);
			}
		}
		todo.pop();
	}
}

void TrashScreen::moveActive(int oldactive)
{
	setPage();
	if(oldactive!=active_) ensureEntryUpdated(oldactive);
	ensureEntryUpdated(active_);
	updateInfo();
}

void TrashScreen::removeJob(const Job& j)
{
	MutexLock l(lock());
	if(trash_.names.find(j.trash)!=trash_.names.end()) {
		int index=static_cast<int>(trash_.names[j.trash]);
		removeIndex(index);
		ensureEntryUpdated(index);
		ensureEntryUpdated(index-1);
		redraw();
	}
}

void TrashScreen::setPage()
{
	int oldfocus=focus_;
	if(active_>lastEntry()) active_=lastEntry();
	if(active_<0) active_=0;
	int page=active_/maxDisplayed();
	focus_=maxDisplayed()* page;
	if(oldfocus!=focus_) redraw();
}

const int TrashScreen::lastShown() const
{
	return
			focus_+maxDisplayed()>lastEntry()?
			lastEntry():
			focus_+maxDisplayed();
}

void TrashScreen::ensureEntryUpdated(const int entry)
{
	if(trash_.items.empty()) active_=-1;
	if(static_cast<size_t>(active_)>=trash_.size) active_=trash_.size-1;
	int lastAllowed=focus_ + maxDisplayed();
	if(entry>=lastAllowed || entry< focus_) return;
	int row=entry-focus_+fileStart();
	string file(
			entry<static_cast<int>(trash_.size)?
			trash_.order.at(entry)->filename():
			string()
			);
	if(file.size()>static_cast<size_t>(maxx_)) {
		string adapted("...");
		adapted+=file.substr(file.size()-maxx_+strlen(adapted.c_str()));
		file=adapted;
	}
	if(static_cast<size_t>(maxx_)>file.size())
		file.append(static_cast<size_t>(maxx_)-file.size(),' ');
	move(row,0);
	{
		color_t c;
		if(scheduled_.find(entry)!=scheduled_.end()) {
			c=color().scheduled();
		} else if(selected_.find(entry)!=selected_.end()) {
			c=color().selected();
		} else if (entry==active_) {
			c=color().active();
		} else {
			c=color().inactive();
		}
		{
			ScopedColor _c(c);
			if(entry==active_) {
				ScopedAttribute bold(A_BOLD);
				printw(file.c_str());
			} else {
				printw(file.c_str());
			}
		}
	}
}

void TrashScreen::drawMenu()
{
	string menu;
	{
		bool scheduled=scheduled_.find(active_)==scheduled_.end();
		ostringstream m;
		m << "F" << FULL_SCREEN_F_COMMAND_Help << ":Help | ";
		if(trash_.size>0)
			m << "F" << FULL_SCREEN_F_COMMAND_Invert << ":Invert | ";
		if(scheduled && trash_.size>0)
			m << "F" << FULL_SCREEN_F_COMMAND_Restore << ":Restore | ";
		m << "F" << FULL_SCREEN_F_COMMAND_Refresh << ":Refresh | ";
		if(scheduled && trash_.size>0) {
			m << "F" << FULL_SCREEN_F_COMMAND_Delete << ":Delete | ";
			m << "F" << FULL_SCREEN_F_COMMAND_Shred << ":Shred | ";
		}
		m << "F" << FULL_SCREEN_F_COMMAND_Quit << ":Quit";
		menu= m.str();
	}
	if(menu.size()>static_cast<size_t>(maxx_)) menu.substr(0,maxx_);
	if(menu.size()<static_cast<size_t>(maxx_))
		menu.append(static_cast<size_t>(maxx_)-menu.size(),' ');
	{
		ScopedColor c(color().menu());
		printw(menu.c_str());
	}
}

void TrashScreen::redraw()
{
	getmaxyx(window(),maxy_,maxx_);
	werase(window());
	drawMenu();
	for(int i=focus_;i<maxDisplayed()+focus_;i++) {
		ensureEntryUpdated(i);
	}
	updateInfo();
	refresh();
}

void TrashScreen::updateInfo()
{
	string info;
	color_t col;
	{
		MutexLock l(messageListener_.lock);
		if(messages_.empty()) {
			if(static_cast<size_t>(active_)>=trash_.size && active_>=0) {
				active_=trash_.size-1;
			}
			if(trash_.size!=0){
				info+=trash_.order.at(active_)->time();
				info+="  ";
				info+=trash_.order.at(active_)->trashname();
			} else {
				info+="Nothing in Trash";
			}
			col=color().info();
		} else {
			info+=messages_.front();
			col=color().message();
		}
	}
	if(info.size()>static_cast<size_t>(maxx_)) info=info.substr(0,maxx_);
	info.append(static_cast<size_t>(maxx_)-info.size(),' ');
	move(maxy_-infoSize(),0);
	{
		ScopedColor c(col);
		printw(info.c_str());
	}
}

void decrementSetIndexes(const int i, set<int>& s)
{
	stack<int> remove;
	for(set<int>::iterator it=s.begin();it!=s.end();it++) {
		if(*it>=i) remove.push(*it);
	}
	while(!remove.empty()) {
		s.erase(remove.top());
		if(remove.top()!=i) {
			s.insert(remove.top()-1);
		}
		remove.pop();
	}
}

void TrashScreen::removeIndex(const int index)
{
	if(trash_.size<=static_cast<size_t>(index)) return;
	// delete all elements from this index
	TrashItemCopy copy(*trash_.order.at(index));
	{
		vector<TrashItemCopy*>::iterator toerase=trash_.order.begin();
		while(toerase!=trash_.order.end()) {
			if (copy==*(*toerase)) break;
			toerase++;
		}
		if(toerase!=trash_.order.end()) {
			trash_.order.erase(toerase);
		}
	}
	{
		list<TrashItemCopy>::iterator toerase=trash_.items.begin();
		while(toerase!=trash_.items.end()) {
			if (copy==*toerase) break;
			toerase++;
		}
		if(toerase!=trash_.items.end()) {
			trash_.items.erase(toerase);
		}
	}
	{
		string tn=copy.trashname();
		if(trash_.names.find(tn)!=trash_.names.end()) {
			trash_.names.erase(tn);
		}
	}
	trash_.size--;
	if(scheduled_.find(index)!=scheduled_.end()) scheduled_.erase(index);
	if(selected_.find(index)!=selected_.end()) selected_.erase(index);
	// decrement indexes about removed index
	for(map<string,int>::iterator it=trash_.names.begin();it!=trash_.names.end();it++) {
		if(it->second > index && it->second>0) it->second--;
	}
	decrementSetIndexes(index, scheduled_);
	decrementSetIndexes(index, selected_);
	setPage();
	drawMenu();
}

void TrashScreen::updateTrashContents()
{
	trash_.items.clear();
	trash_.size=0;
	trashcan().items(trash_);
	trash_.items.sort();
	handleNewItems();
}

void TrashScreen::handleNewItems()
{
	trash_.order.clear();
	trash_.names.clear();
	list<TrashItemCopy>::iterator it=trash_.items.begin();
	int index=0;
	while (it!=trash_.items.end()){
		const string& trashname=it->trashname();
		trash_.names.insert(make_pair( trashname, index++ ));
		trash_.order.push_back(&(*it));
		it++;
	}
}

bool doesTrashExist(const string& trashname)
{
	string infofile;
	string datafile;
	try {
		pair<string,string> p=trashCanSet(trashname);
		datafile+=p.second;
		datafile+="/.Trash/files/";
		datafile+=p.first;
		infofile+=p.second;
		infofile+="/.Trash/info/";
		infofile+=p.first;
	} catch (const IsHomeTrash& ht) {
		homeTrashDirectory(datafile);
		datafile+="/Trash/files/";
		datafile+=trashname;
		homeTrashDirectory(infofile);
		infofile+="/Trash/info/";
		infofile+=trashname;
		infofile+=".trashinfo";
	}
	{
		struct stat s;
		if(stat(infofile.c_str(),&s)!=0) return false;
	}
	{
		struct stat s;
		return stat(datafile.c_str(),&s)==0;
	}
}

struct Schedules {
	void operator () (const int& index)
	{
		ss.push_back(*(order->at(index)));
	}
	void operator () (const TrashItemCopy& tc)
	{
		si->insert((*names)[tc.trashname()]);
	}
	list<TrashItemCopy> ss;
	vector<TrashItemCopy*>* order;
	set<int>* si;
	map<string,int>* names;
} ;

void TrashScreen::add(const string& inname)
{
	string trashname=ensureHomeDirGone(inname);
	if(!doesTrashExist(trashname)) return;
	{
		MutexLock _l(lock());
		if(trash_.names.find(trashname)!=trash_.names.end()) return;
		string currentActive;
		if(active_>=0 && static_cast<size_t>(active_)>=trash_.size) {
			currentActive = trash_.order[active_]->trashname();
		}
		Schedules schedules;
		schedules.order=&trash_.order;
		schedules.si=&scheduled_;
		schedules.names=&trash_.names;
		for_each(scheduled_.begin(),scheduled_.end(),schedules);
		scheduled_.clear();
		{
			list<TrashItemCopy> found;
			TrashItemCopy item=getItemFromTrashname(trashname);
			found.push_back(item);
			list<TrashItemCopy> result(trash_.size+1,item);
			merge(
					trash_.items.begin(),trash_.items.end(),
					found.begin(),found.end(),
					result.begin()
			);
			trash_.size++;
			trash_.items=result;
			handleNewItems();
		}
		if(!currentActive.empty() && trash_.names.find(currentActive)!=trash_.names.end()) {
			active_= trash_.names[currentActive];
		}
		for_each(schedules.ss.begin(),schedules.ss.end(),schedules);
	}
	{
		MutexLock _l(lock());
		redraw();
	}
}

void TrashScreen::schedule(const string& trashname)
{
	MutexLock _l(lock());
	map<string,int>::iterator found=trash_.names.find(ensureHomeDirGone(trashname));
	if(found==trash_.names.end()) return;
	if(scheduled_.find(found->second)==scheduled_.end()) {
		scheduled_.insert(found->second);
		ensureEntryUpdated(found->second);
		refresh();
	}
}

void TrashScreen::unschedule(const string& trashname)
{
	MutexLock _l(lock());
	map<string,int>::iterator found=trash_.names.find(ensureHomeDirGone(trashname));
	if(found==trash_.names.end()) return;
	if(scheduled_.find(found->second)!=scheduled_.end()) {
		scheduled_.erase(found->second);
		ensureEntryUpdated(found->second);
		refresh();
	}
}

void TrashScreen::remove(const string& trashname)
{
	MutexLock _l(lock());
	map<string,int>::iterator found=trash_.names.find(ensureHomeDirGone(trashname));
	if(found==trash_.names.end()) return;
	removeIndex(found->second);
	ensureEntryUpdated(found->second);
	ensureEntryUpdated(found->second-1);
	ensureEntryUpdated(found->second+1);
	redraw();
}

/** intentionally ignored */
void TrashScreen::undefined(const char event,const string& trashname) {}

}
