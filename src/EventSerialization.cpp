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
 * File:		EventSerialization.cpp
 * Description:	trash can event marshaling
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Aug 3 2010
 **********************************************************************************************/

#include "EventSerialization.h"

namespace trailer {
namespace serialization {

size_t CanEventWriter::output(char* raw,const size_t rawmax)
{
	#ifdef DEBUG
		string sent;
	#endif
	const size_t avail=availible();
	const size_t entryCount = rawmax < avail ? rawmax : avail;
	const char* last=raw + entryCount;
	while(raw != last) {
		next(*raw);
		#ifdef DEBUG
			sent += *raw;
		#endif
		raw++;
	}
	count_-=entryCount;
	DEBUGLOG("CanEventWriter::output: count(" << count_ << ") entryCount(" << entryCount << ") sent(" << sent << ")")
	return entryCount;
}

void CanEventWriter::undefined(const char event,const string& trashname)
{
	DEBUGLOG("CanEventWriter::undefined: event(" << event << ") trashname(" << trashname << ")")
	MutexLock l(mutex_);
	outgoing_.push_back(event);
	for(size_t i=0;i<trashname.size();i++) {
		outgoing_.push_back(trashname[i]);
	}
	outgoing_.push_back(caneventcommands::end());
	#ifdef DEBUG
		int beforeCount=count_;
	#endif
	count_+=trashname.size() + sizeof(event) + sizeof(caneventcommands::end());
	DEBUGLOG("CanEventWriter::undefined: countchange(" << (count_-beforeCount) << ") trashname(" << trashname << ")")
}

void CanEventReader::operator () (const char* data, const int len)
{
	#ifdef DEBUG
		string forlogging;
	#endif
	for(int i=0;i<len;i++) {
		#ifdef DEBUG
			forlogging += data[i];
		#endif
		(*this)(data[i]);
	}
	#ifdef DEBUG
		DEBUGLOG("CanEventReader::operator (): len: " << len << ":" << forlogging)
	#endif
}

}

}
