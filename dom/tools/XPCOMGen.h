/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _XPCOMGen_h__
#define _XPCOMGen_h__

#include <string.h>
#include "FileGen.h"

class ofstream;
class IdlSpecification;
class IdlInterface;

class XPCOMGen : public FileGen {
public:
     XPCOMGen();
     ~XPCOMGen();

     virtual void     Generate(char *aFileName, char *aOutputDirName, 
                               IdlSpecification &aSpec, int aIsGlobal);

protected:
     void     GenerateIfdef(IdlSpecification &aSpec);
     void     GenerateIncludes(IdlSpecification &aSpec);
     void     GenerateForwardDecls(IdlSpecification &aSpec);
     void     GenerateGuid(IdlInterface &aInterface);
     void     GenerateClassDecl(IdlInterface &aInterface);
     void     GenerateEnums(IdlInterface &aInterface);
     void     GenerateMethods(IdlInterface &aInterface);
     void     GenerateEndClassDecl();
     void     GenerateEpilog(IdlSpecification &aSpec);

     int mIsGlobal;
};

#endif // _XPCOMGen_h__ 
