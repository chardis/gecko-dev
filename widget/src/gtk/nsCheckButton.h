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

#ifndef nsCheckButton_h__
#define nsCheckButton_h__

#include "nsWidget.h"
#include "nsICheckButton.h"

/**
 * Native Motif Checkbox wrapper
 */

class nsCheckButton : public nsWidget,
                      public nsICheckButton
{

public:
                            nsCheckButton();
    virtual                 ~nsCheckButton();

    // nsISupports
    NS_IMETHOD_(nsrefcnt) AddRef();
    NS_IMETHOD_(nsrefcnt) Release();
    NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

    // nsICheckButton part
    NS_IMETHOD SetLabel(const nsString &aText);
    NS_IMETHOD GetLabel(nsString &aBuffer);
    NS_IMETHOD SetState(const PRBool aState);
    NS_IMETHOD GetState(PRBool& aState);

  virtual PRBool OnMove(PRInt32 aX, PRInt32 aY);
  virtual PRBool OnPaint(nsPaintEvent &aEvent);
  virtual PRBool OnResize(nsSizeEvent &aEvent);

  // These are needed to Override the auto check behavior
  void Armed();
  void DisArmed();

protected:
  NS_IMETHOD nsCheckButton::CreateNative(GtkWidget *parentWindow);
  GtkWidget *mLabel;

private:
  PRBool mInitialState;
  PRBool mNewValue;
  PRBool mValueWasSet;
  PRBool mIsArmed;

};

#endif // nsCheckButton_h__
