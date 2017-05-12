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
 * File:		UserConfiguration.cpp
 * Description:	configuration decisions
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 17 2010
 **********************************************************************************************/

#include "UserConfiguration.h"

using namespace std;

namespace trailer {

string FileAndPathFinder::etc() const
{
	string f;
	f+="/etc/";
	f+=appname();
	f+="/";
	f+=file_;
	return f;
}

string FileAndPathFinder::home() const
{
	char* home=getenv("HOME");
	if(home==NULL) throw CantFind();
	string f;
	f+=home;
	f+="/.";
	f+=appname();
	f+="/";
	f+=file_;
	return f;
}

string FileAndPathFinder::best() const
{
	{
		struct stat s;
		string h=home();
		if(stat(h.c_str(),&s)==0) return h;
	}
	{
		struct stat s;
		string e=home();
		if(stat(e.c_str(),&s)==0) return e;
	}
	ensurehome();
	{
		struct stat s;
		string h=home();
		if(stat(h.c_str(),&s)==0) return h;
		else ofstream file(h.c_str());
	}
	{
		struct stat s;
		string h=home();
		if(stat(h.c_str(),&s)==0) return h;
	}
	throw CantFind();
}

void FileAndPathFinder::ensurehome() const
{
	char* home=getenv("HOME");
	if(home==NULL) throw CantFind();
	string f;
	f+=home;
	f+="/.";
	f+=appname();
	ensureDirectory(0700,f);
}

}
