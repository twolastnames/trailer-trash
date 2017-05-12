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
 * File:		CommandLine.cpp
 * Description:	a command line implementation
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#include "CommandLine.h"

namespace trailer {
namespace commandline {

struct Column {
	Column() : maxsize(0) {}
	size_t maxsize;
	list<string> values;
	void insert(const string& v)
	{
		if(v.size()>maxsize) maxsize=v.size();
		values.push_back(v);
	}
};

void listTrashContents(TrashCan& tc)
{
	enum KEY {TRASHNAME, TIME, FILENAME};
	struct : public TrashItemListener {
		void insert(KEY k, const string& s)
		{
			while(cache.size()<=static_cast<size_t>(k)) cache.push_back(Column());
			cache[k].insert(s);
		}
		void handleTrashItem(const TrashItem& ti)
		{
			insert(TRASHNAME,ti.trashname());
			insert(TIME,ti.time());
			insert(FILENAME,ti.filename());
		}
		vector<Column> cache;
	} l;
	tc.items(l);
	{
		char* columnenv=getenv("COLUMNS");
		if(columnenv==NULL) columnenv=const_cast<char*>("80");
		int columnCount=atoi(columnenv);
		if(columnCount<5) columnCount=80;
	}
	if(l.cache.size()<1) return;
	const int BETWEEN=2;
	{
		list<string>::iterator tsit=l.cache[TRASHNAME].values.begin();
		list<string>::iterator tmit=l.cache[TIME].values.begin();
		list<string>::iterator flit=l.cache[FILENAME].values.begin();
		while(
				tsit != l.cache[TRASHNAME].values.end() &&
				tmit != l.cache[TIME].values.end() &&
				flit != l.cache[FILENAME].values.end() ) {
			cout << (*tsit);
			for(size_t i=tsit->size();i<l.cache[TRASHNAME].maxsize+BETWEEN;i++) cout << ' ';
			cout << (*tmit);
			for(size_t i=tmit->size();i<l.cache[TIME].maxsize+BETWEEN;i++) cout << ' ';
			cout << (*flit);
			for(size_t i=flit->size();i<l.cache[FILENAME].maxsize;i++) cout << ' ';
			cout << endl;
			tsit++;
			tmit++;
			flit++;
		}
	}
}

}
}
