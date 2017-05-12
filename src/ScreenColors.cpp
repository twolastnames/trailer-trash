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
 * File:		ScreenColors.cpp
 * Description:	manipulation of ncurses colors
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 31 2010
 **********************************************************************************************/

#include "ScreenColors.h"

namespace trailer {

DefaultColors::DefaultColors()
{
	if(has_colors()) {
		init_pair(localcolor::active,COLOR_YELLOW,COLOR_BLACK);
		init_pair(localcolor::inactive,COLOR_YELLOW,COLOR_BLUE);
		init_pair(localcolor::selected,COLOR_BLUE,COLOR_GREEN);
		init_pair(localcolor::scheduled,COLOR_RED,COLOR_YELLOW);
		init_pair(localcolor::menu,COLOR_WHITE,COLOR_BLACK);
		init_pair(localcolor::info,COLOR_CYAN,COLOR_BLACK);
		init_pair(localcolor::message,COLOR_WHITE,COLOR_RED);
		start_color();
	}
}

#ifdef HAVE_LIBXML2

namespace colorfiletags {

const KnownColor colors[] = {
	KnownColor("black",COLOR_BLACK),
	KnownColor("red",COLOR_RED),
	KnownColor("green",COLOR_GREEN),
	KnownColor("yellow",COLOR_YELLOW),
	KnownColor("blue",COLOR_BLUE),
	KnownColor("magenta",COLOR_MAGENTA),
	KnownColor("cyan",COLOR_CYAN),
	KnownColor("white",COLOR_WHITE)
};

const ColorTag tags[] = {
	ColorTag("active",localcolor::active),
	ColorTag("inactive",localcolor::inactive),
	ColorTag("selected",localcolor::selected),
	ColorTag("scheduled",localcolor::scheduled),
	ColorTag("menu",localcolor::menu),
	ColorTag("info",localcolor::info),
	ColorTag("message",localcolor::message)
};

void ColorData::color(const xmlChar* tag, const xmlChar* color)
{
	short found;
	for(size_t i=0;i<sizeof(colors)/sizeof(KnownColor);i++) {
		xmlutil::XmlCharContainer cont(colors[i].name.c_str());
		if(xmlStrcmp(cont.data,color)==0) {
			found=colors[i].color;
			goto HASCOLOR;
		}
	}
	throw InvalidColorData(InvalidColorData::INVALID_ATTRIBUTE);
	HASCOLOR:
	{
		xmlutil::XmlCharContainer cont(colorfiletags::foreground());
		if(xmlStrcmp(cont.data,tag)==0) {
			foreground_=found;
			return;
		}
	}
	{
		xmlutil::XmlCharContainer cont(colorfiletags::background());
		if(xmlStrcmp(cont.data,tag)==0) {
			background_=found;
			return;
		}
	}
	throw InvalidColorData(InvalidColorData::INVALID_COLOR);
}

void ColorData::type(const xmlChar* t)
{
	for(size_t i=0;i<sizeof(tags)/sizeof(ColorTag);i++) {
		xmlutil::XmlCharContainer cont(tags[i].name);
		if(xmlStrcmp(cont.data,t)==0) {
			part_=tags[i].index;
			return;
		}
	}
	throw InvalidColorData(InvalidColorData::INVALID_TAG);
}

void ColorInitializer::initialize()
{
	if(has_colors()) {
		while (!colors_.empty()) {
			ColorData& cd=colors_.top();
			if     (cd.foreground() <0) throw InvalidColorData(InvalidColorData::MISSING_COLOR);
			else if(cd.background()  <0) throw InvalidColorData(InvalidColorData::MISSING_COLOR);
			else if(cd.part()<0) throw InvalidColorData(InvalidColorData::MISSING_TAG);
			init_pair(cd.part(),cd.foreground(),cd.background());
			colors_.pop();
		}
	}
}

void ShadeData::color(const xmlChar* color, const xmlChar* shade)
{
	if(!xmlutil::isNumber(shade)) {
		throw InvalidColorData(InvalidColorData::INVALID_SHADE);
	}
	int num=xmlutil::asInt(shade);
	if(num>1000) throw InvalidColorData(InvalidColorData::INVALID_SHADE);
	xmlutil::XmlCharContainer b(colorfiletags::blue());
	xmlutil::XmlCharContainer r(colorfiletags::red());
	xmlutil::XmlCharContainer g(colorfiletags::green());
	if(xmlStrcmp(color,b.data)==0) blue_=num;
	else if(xmlStrcmp(color,r.data)==0) red_=num;
	else if(xmlStrcmp(color,g.data)==0) green_=num;
	else {
		throw InvalidColorData(InvalidColorData::INVALID_COLOR);
	}
}

void ShadeData::type(const xmlChar* t)
{
	using namespace colorfiletags;
	for(size_t i=0;i<sizeof(colors)/sizeof(KnownColor);i++) {
		xmlutil::XmlCharContainer d(colors[i].name.c_str());
		if(xmlStrcmp(t,d.data)==0) index_=colors[i].color;
	}
	if(index_==-1) throw InvalidColorData(InvalidColorData::INVALID_TAG);
}

void ShadeInitializer::initialize()
{
	if(can_change_color()) {
		while (!colors_.empty()) {
			ShadeData& cd=colors_.top();
			if     (cd.blue() <0) throw InvalidColorData(InvalidColorData::MISSING_SHADE);
			else if(cd.red()  <0) throw InvalidColorData(InvalidColorData::MISSING_SHADE);
			else if(cd.green()<0) throw InvalidColorData(InvalidColorData::MISSING_SHADE);
			else if(cd.index()<0) throw InvalidColorData(InvalidColorData::MISSING_TAG);
			init_color(cd.index(),cd.red(),cd.green(),cd.blue());
			colors_.pop();
		}
	}
}

}

namespace usercolorfile {

void makeDefaultColorFile(const string& filename)
{
	using namespace colorfiletags;
	ofstream out(filename.c_str());
	out << "<?xml version=\"1.0\"?>" << endl << endl;
	out << "<" << colorfiletags::start() << ">" << endl;
	out << "<" << colorfiletags::colorsTag() << ">" << endl;
	out << "<!-- valid colors are black, red, green, yellow, blue, magenta, cyan & white -->" << endl;
	out << "  <" << tags[0].name << " front='yellow' back='black'/>" << endl;
	out << "  <" << tags[1].name << " front='yellow' back='blue'/>" << endl;
	out << "  <" << tags[2].name << " front='blue' back='green'/>" << endl;
	out << "  <" << tags[3].name << " front='red' back='yellow'/>" << endl;
	out << "  <" << tags[4].name << " front='white' back='black'/>" << endl;
	out << "  <" << tags[5].name << " front='cyan' back='black'/>" << endl;
	out << "  <" << tags[6].name << " front='white' back='red'/>" << endl;
	out << "</" << colorfiletags::colorsTag() << ">" << endl;
	out << "<!-- most terminals do not support the shades tag and is untested outside of debugging --> " << endl;
	out << "<" << colorfiletags::shadesTag() << ">" << endl;
	out << "  <" << colors[0].name << " red='0' green='0' blue='0'/>" << endl;
	out << "  <" << colors[1].name << " red='255' green='0' blue='0'/>" << endl;
	out << "  <" << colors[2].name << " red='0' green='255' blue='0'/>" << endl;
	out << "  <" << colors[3].name << " red='255' green='255' blue='0'/>" << endl;
	out << "  <" << colors[4].name << " red='0' green='0' blue='255'/>" << endl;
	out << "  <" << colors[5].name << " red='255' green='0' blue='255'/>" << endl;
	out << "  <" << colors[6].name << " red='0' green='255' blue='255'/>" << endl;
	out << "  <" << colors[7].name << " red='255' green='255' blue='255'/>" << endl;
	out << "</" << colorfiletags::shadesTag() << ">" << endl;
	out << "</" << colorfiletags::start() << ">" << endl;
	out.flush();
	out.close();
}

void nullifySaxHandler(xmlSAXHandler& handler)
{
	handler.internalSubset=NULL;
	handler.isStandalone=NULL;
	handler.hasInternalSubset=NULL;
	handler.hasExternalSubset=NULL;
	handler.resolveEntity=NULL;
	handler.getEntity=NULL;
	handler.entityDecl=NULL;
	handler.notationDecl=NULL;
	handler.attributeDecl=NULL;
	handler.elementDecl=NULL;
	handler.unparsedEntityDecl=NULL;
	handler.setDocumentLocator=NULL;
	handler.startDocument=NULL;
	handler.endDocument=NULL;
	handler.startElement=NULL;
	handler.endElement=NULL;
	handler.reference=NULL;
	handler.characters=NULL;
	handler.ignorableWhitespace=NULL;
	handler.processingInstruction=NULL;
	handler.comment=NULL;
	handler.warning=NULL;
	handler.error=NULL;
	handler.fatalError=NULL;
}

struct ColorFileReport {
	ColorFileReport() : error(NULL) {}
	colorfiletags::ColorSetter* setter;
	colorfiletags::ColorInitializer colors;
	colorfiletags::ShadeInitializer shades;
	xmlSAXHandler handler;
	colorfiletags::InvalidColorData* error;
	void setError(colorfiletags::InvalidColorData* e) {if(error!=NULL) delete error;error=e;}
};

void error(void *user_data, const char *msg, ...)
{
	using namespace colorfiletags;
	ColorFileReport* c=reinterpret_cast<ColorFileReport*>(user_data);
	c->setError(
			new InvalidColorData(InvalidColorData::NO_WELL_FORM)
	);
}

void fatalError(void *user_data, const char *msg, ...)
{
	using namespace colorfiletags;
	ColorFileReport* c=reinterpret_cast<ColorFileReport*>(user_data);
	c->setError(
			new InvalidColorData(InvalidColorData::NO_WELL_FORM)
	);
}

void characters(	void * 	user_data,
 	const xmlChar * 	ch,
 	int  	len)
{
	using namespace colorfiletags;
	ColorFileReport* c=reinterpret_cast<ColorFileReport*>(user_data);
	string text;
	bool spaces=true;
	for(int i=0;i<len;i++) {
		if(*ch > 32 && *ch < 127) {
			spaces=false;
		}
		text+=*ch;
	}
	if(!spaces) c->setError(new InvalidColorData(InvalidColorData::FREE_TEXT));
}

void innerElement(	void * 	user_data,
 	const xmlChar * 	name,
 	const xmlChar ** 	attrs)
{
	using namespace colorfiletags;
	ColorFileReport* c=reinterpret_cast<ColorFileReport*>(user_data);
	Color& color=c->setter->next();
	color.type(name);
	xmlChar ** a=const_cast<xmlChar**>(attrs);
	while(a!=NULL) {
		xmlChar** n=a++;
		if((*n)==NULL) break;
		xmlChar** v=a++;
		if((*v)==NULL) break;
		try {
			color.color(*n,*v);
		} catch (const InvalidColorData& e) {
			c->setError(new InvalidColorData(e.reason));
		}
	}
}

void outerElement(void* user_data,const xmlChar* name,const xmlChar** attrs);

void rootBackElement(	void * 	user_data,
 	const xmlChar * 	name)
{
	ColorFileReport* c=reinterpret_cast<ColorFileReport*>(user_data);
	{
		xmlutil::XmlCharContainer o(colorfiletags::colorsTag());
		xmlutil::XmlCharContainer s(colorfiletags::shadesTag());
		if(xmlStrcmp(o.data,name)==0 || xmlStrcmp(s.data,name)==0) {
			c->handler.startElement=&outerElement;
			c->handler.endElement=NULL;
		}
	}
}

#define SET_TAG(tag,set) \
{ \
xmlutil::XmlCharContainer tagName( tag ()); \
if(xmlStrcmp(tagName.data,name)==0) { \
	c->setter=&c-> set; \
	c->handler.endElement=&rootBackElement; \
	c->handler.startElement=&innerElement; \
	return; \
} \
}

void outerElement(	void * 	user_data,
 	const xmlChar * 	name,
 	const xmlChar ** 	attrs)
{
	using namespace colorfiletags;
	ColorFileReport* c=reinterpret_cast<ColorFileReport*>(user_data);
	SET_TAG(colorfiletags::colorsTag, colors)
	SET_TAG(colorfiletags::shadesTag, shades)
	{
		xmlutil::XmlCharContainer tagName( colorfiletags::start());
		if(xmlStrcmp(tagName.data,name)==0) {
			return;
		}
	}
	c->setError(new InvalidColorData(InvalidColorData::INVALID_TAG));
}
#undef SET_TAG

}

ColorFile::ColorFile(const string& filename)
{
	using namespace usercolorfile;
	{
		ifstream in(filename.c_str());
		string firstline;
		std::getline(in,firstline);
		if(firstline.empty())
			usercolorfile::makeDefaultColorFile(filename.c_str());
	}
	ColorFileReport report;
	nullifySaxHandler(report.handler);
	report.handler.startElement=&outerElement;
	report.handler.characters=&characters;
	report.handler.error=&error;
	report.handler.fatalError=&fatalError;
	xmlSAXUserParseFile(&report.handler, &report, filename.c_str());
	if(has_colors()) start_color();
	if(report.error!=NULL) throw *report.error;
	report.colors.initialize();
	report.shades.initialize();
}

}

#endif

