/************************************************************************
 ************************************************************************
    FAUST compiler
	Copyright (C) 2003-2014 GRAME, Centre National de Creation Musicale
    ---------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ************************************************************************
 ************************************************************************/

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>

#include "compatibility.hh"
#include "asmjs_dsp_aux.hh"
#include "libfaust.h"

static inline std::string flatten(const std::string& src)
{
    std::stringstream dst;
    size_t size = src.size();
    for (size_t i = 0; i < src.size(); i++) {
        switch (src[i]) {
            case '\n':
            case '\t':
            case '\r':
                break;
            case ' ':
                if (!(i + 1 < size && src[i + 1] == ' ')) {
                    dst << src[i];
                }
                break;
            default:
                dst << src[i];
                break;
        }
    }
    return dst.str();
}

EXPORT const char* createAsmCDSPFactoryFromString(const char* name_app, const char* dsp_content, const char* name_class, char* error_msg)
{
    int argc1 = 7;
 	const char* argv1[32];
    
    argv1[0] = "faust";
	argv1[1] = "-lang";
	argv1[2] = "ajs";
    argv1[3] = "-cn";
    argv1[4] = name_class;
    argv1[5] = "-o";
    argv1[6] = "asmjs";
    
    string str;
    try {
        str = compile_faust_asmjs(argc1, argv1, name_app, dsp_content, error_msg);
        str = flatten(str);
    } catch (...) {
        strncpy(error_msg, "libfaust.js fatal error...", 256);
        str = "";
    }
    char* cstr = (char*)malloc(str.length() + 1);
    strcpy(cstr, str.c_str());
    return cstr;
}
