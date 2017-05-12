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
 * File:		ScreenColors.h
 * Description:	manipulation of ncurses colors
 * Project:		Trailer Trash
 * Author:		Alan Glenn
 * Date:		Jul 31 2010
 **********************************************************************************************/

#ifndef SCREENCOLORS_H_
#define SCREENCOLORS_H_

#include <iostream>
#include <stack>
#include <sstream>

#include "config.h"

#ifdef __CYGWIN__
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#include "XmlUtil.h"
#endif

#include "Common.h"
#include "UserConfiguration.h"

using namespace std;

namespace trailer {

typedef short color_t;

namespace localcolor {
	const color_t active=6;
	const color_t inactive=1;
	const color_t selected=2;
	const color_t scheduled=3;
	const color_t menu=4;
	const color_t info=5;
	const color_t message=7;
}

/** colors for displaying **/
struct ColorDefinitions {
	virtual ~ColorDefinitions() {}
	/** color of where the pseudo-cursor is */
	virtual const color_t active() const=0;
	/** color of entry with nothing special */
	virtual const color_t inactive() const=0;
	/** color of a selected entry */
	virtual const color_t selected() const=0;
	/** color of entry that is scheduled to have something done with it */
	virtual const color_t scheduled() const=0;
	/** color of menu */
	virtual const color_t menu() const=0;
	/** color of status bar */
	virtual const color_t info() const=0;
	/** color of status bar */
	virtual const color_t message() const=0;
};

struct MappedColors : public ColorDefinitions {
	virtual ~MappedColors() {}
	const color_t active() const {return localcolor::active;}
	const color_t inactive() const {return localcolor::inactive;}
	const color_t selected() const {return localcolor::selected;}
	const color_t scheduled() const {return localcolor::scheduled;}
	const color_t menu() const {return localcolor::menu;}
	const color_t info() const {return localcolor::info;}
	const color_t message() const {return localcolor::message;}
};

struct DefaultColors : public MappedColors {
	DefaultColors();
};

#ifdef HAVE_LIBXML2

namespace colorfiletags {

struct InvalidColorData : public ExceptionBase {
	enum REASON {
		MISSING_TAG,NO_WELL_FORM,FREE_TEXT, NO_INIT_TAG,MULTIPLE_STARTS,
		INVALID_TAG,INVALID_ATTRIBUTE,INVALID_COLOR,MISSING_COLOR,
		MISSING_SHADE, INVALID_SHADE
	};
	InvalidColorData(const REASON r) : reason(r) {}
	const REASON reason;
};

struct KnownColor {
	KnownColor(const string& n, const short c) : name(n), color(c) {}
	const string name;
	const short color;
};

inline const char* foreground() {return "front";}
inline const char* background() {return "back";}
inline const char* start() {return "customColors";}
inline const char* colorsTag() {return "colors";}
inline const char* shadesTag() {return "shades";}

struct Color {
	virtual ~Color() {}
	virtual void color(const xmlChar* color, const xmlChar* shade)=0;
	virtual void type (const xmlChar* t)=0;
};

struct ShadeData : public Color {
	ShadeData() :  red_(-1), blue_(-1), green_(-1), index_(-1) {}
	void color(const xmlChar* color, const xmlChar* shade);
	void type(const xmlChar* t);
	const int red() const {return red_;}
	const int blue() const {return blue_;}
	const int green() const {return green_;}
	const short index() const {return index_;}
private:
	int red_;
	int blue_;
	int green_;
	short index_;
};

class ColorData : public Color {
public:
	ColorData() : part_(-1), foreground_(-1), background_(-1) {}
	void color(const xmlChar* tag, const xmlChar* color);
	void type(const xmlChar* t);
	const color_t part() const {return part_;}
	const short foreground() const {return foreground_;}
	const short background() const {return background_;}
private:
	color_t part_;
	short foreground_;
	short background_;
};

struct ColorSetter {
	virtual ~ColorSetter() {}
	virtual Color& next()=0;
};

/**
 * COLORDATA is of type Color
 */
template <typename COLORDATA>
class Initializer : public ColorSetter {
public:
	virtual ~Initializer() {}
	virtual void initialize()=0;
	Color& next()
	{
		colors_.push(COLORDATA());
		return colors_.top();
	}
protected:
	stack<COLORDATA> colors_;
};

struct ColorInitializer : public Initializer<ColorData> {
	ColorInitializer() : Initializer<ColorData>() {}
	void initialize();
};

struct ShadeInitializer : public Initializer<ShadeData> {
	ShadeInitializer() : Initializer<ShadeData>() {}
	void initialize();
};

struct ColorTag {
	ColorTag(const char* n, const color_t i) : name(n), index(i) {}
	const char* name;
	const color_t index;
};

inline const char* blue() {return "blue";}
inline const char* red() {return "red";}
inline const char* green() {return "green";}

inline ColorTag active() {return ColorTag("active",localcolor::active);}
inline ColorTag inactive() {return ColorTag("inactive",localcolor::inactive);}
inline ColorTag selected() {return ColorTag("selected",localcolor::selected);}
inline ColorTag scheduled() {return ColorTag("scheduled",localcolor::scheduled);}
inline ColorTag menu() {return ColorTag("menu",localcolor::menu);}
inline ColorTag info() {return ColorTag("info",localcolor::info);}
inline ColorTag message() {return ColorTag("message",localcolor::message);}

}

struct ColorFile : public MappedColors {
	ColorFile(const string& filename);
};

}

#endif


#endif /* SCREENCOLORS_H_ */
