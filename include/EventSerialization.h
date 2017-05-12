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
 * File:		EventSerialization.h
 * Description:	trash can event marshaling
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Aug 3 2010
 **********************************************************************************************/

#ifndef EVENTSERIALIZATION_H_
#define EVENTSERIALIZATION_H_

#include <stdlib.h>
#include <string.h>

#include <memory>
#include <algorithm>
#include <fstream>
#include <sstream>

#include <sys/types.h>

#include "Common.h"
#include "TrashEvent.h"

using namespace trailer::trashevent;

namespace trailer {
namespace serialization {

typedef unsigned short checksum_t;

namespace caneventcommands {
	inline const char add()        {return 'a';}
	inline const char schedule()   {return 's';}
	inline const char unschedule() {return 'u';}
	inline const char remove()     {return 'r';}
	inline const char end()        {return 0x17;}
};

inline char commandForMember(ListenerMember m)
{
	if(m==&trashevent::EventListener::add) {
		return caneventcommands::add();
	} else 	if(m==&trashevent::EventListener::schedule) {
		return caneventcommands::schedule();
	} else 	if(m==&trashevent::EventListener::unschedule) {
		return caneventcommands::unschedule();
	} else 	if(m==&trashevent::EventListener::remove) {
		return caneventcommands::remove();
	} else {
		return '?';
	}
}

/**
 * parses events associated with the trash can
 */
class CanEventReader {
public:
	CanEventReader(EventListener* ev) : event_(caneventcommands::end()), listener_(ev) {}
	void operator () (const char* data) {while(*data!='\0') {(*this)(*data);data++;} }
	void operator () (const char* data, const int len);
	void operator () (const char c);
private:
	string trashname_;
	char event_;
	EventListener* listener_;
};

inline void CanEventReader::operator () (const char c)
{
	if(c=='\0') return;
	if(event_==caneventcommands::end()) {
		event_=c;
	} else if (c==caneventcommands::end()) {
		if(event_==caneventcommands::add()) {
			listener_->add(trashname_);
		} else if(event_==caneventcommands::schedule()) {
			listener_->schedule(trashname_);
		} else if(event_==caneventcommands::remove()) {
			listener_->remove(trashname_);
		} else {
			listener_->undefined(event_,trashname_);
		}
		event_=caneventcommands::end();
		trashname_.clear();
	} else {
		trashname_+=c;
	}
}

/**
 * serializes events associated with the trash can
 * workflow:
 * 1)	call add, schedule, unschedule or remove (call as many of these as needed now)
 * 2)   get bytes to send by calling available
 * 3)   give the output buffer
 */
class CanEventWriter : public EventListener {
public:
	CanEventWriter() : count_(0) {pthread_mutex_init(&mutex_,NULL);}
	~CanEventWriter() {pthread_mutex_destroy(&mutex_);}
	/**
	 * @returns 0 if there is nothing more that is currently parsed
	 */
	size_t output(char* raw,const size_t rawmax);
	/**
	 * @returns available bytes
	 */
	size_t availible() {return count_;}
	void add(const string& trashname) {undefined(caneventcommands::add(),trashname);}
	void schedule(const string& trashname) {undefined(caneventcommands::schedule(),trashname);}
	void unschedule(const string& trashname) {undefined(caneventcommands::unschedule(),trashname);}
	void remove(const string& trashname) {undefined(caneventcommands::remove(),trashname);}
	/**
	 * This is only an error if you make it one.  If the event and trashname are valid than it is good.
	 */
	void undefined(const char event,const string& trashname);
private:
	CanEventWriter(const CanEventWriter&);
	CanEventWriter& operator = (const CanEventWriter&);
	void next(char& out);
	list<char> outgoing_;
	pthread_mutex_t mutex_;
	size_t count_;
};

inline void CanEventWriter::next(char& out)
{
	out=outgoing_.front();
	outgoing_.pop_front();
}

}

}

#endif /* EVENTSERIALIZATION_H_ */
