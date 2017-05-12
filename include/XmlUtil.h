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
 * File:		XmlUtil.h
 * Description:	generic xml routines
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 29 2010
 **********************************************************************************************/

#ifndef XMLUTIL_H_
#define XMLUTIL_H_

#include <libxml/xmlstring.h>

#include "Common.h"

namespace trailer {
namespace xmlutil {

struct XmlCharContainer {
	XmlCharContainer(const char* c) : data(xmlCharStrdup(c)) {}
	XmlCharContainer(const char* c, int l) : data(xmlCharStrndup(c,l)) {}
	~XmlCharContainer() {delete []data;}
	const xmlChar* data;
	XmlCharContainer(const XmlCharContainer& c);
	XmlCharContainer& operator = (const XmlCharContainer& c);
};

inline bool isNumber(const xmlChar* d)
{
	if(*d==0) return false;
	while(*d!=0) {
		if(*d<48) return false;
		if(*d>57) return false;
		d++;
	}
	return true;
}

inline int asInt(const xmlChar* d)
{
	int num=0;
	while(*d!=0) {
		num *= 10;
		num += *d - 48;
		d++;
	}
	return num;
}

}
}

#endif /* XMLUTIL_H_ */
