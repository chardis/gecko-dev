/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_domerror_h__
#define mozilla_dom_domerror_h__

#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsPIDOMWindow.h"

#define DOMERROR_IID \
{ 0x220cb63f, 0xa37d, 0x4ba4, \
 { 0x8e, 0x31, 0xfc, 0xde, 0xec, 0x48, 0xe1, 0x66 } }

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

class DOMError : public nsISupports,
                 public nsWrapperCache
{
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsString mName;
  nsString mMessage;

protected:
  virtual ~DOMError();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMError)

  NS_DECLARE_STATIC_IID_ACCESSOR(DOMERROR_IID)

  // aWindow can be null if this DOMError is not associated with a particular
  // window.

  DOMError(nsPIDOMWindow* aWindow);

  DOMError(nsPIDOMWindow* aWindow, nsresult aValue);

  DOMError(nsPIDOMWindow* aWindow, const nsAString& aName);

  DOMError(nsPIDOMWindow* aWindow, const nsAString& aName,
           const nsAString& aMessage);

  nsPIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<DOMError>
  Constructor(const GlobalObject& global, const nsAString& name,
              const nsAString& message, ErrorResult& aRv);

  void GetName(nsString& aRetval) const
  {
    aRetval = mName;
  }

  void GetMessage(nsString& aRetval) const
  {
    aRetval = mMessage;
  }

  void Init(const nsAString& aName, const nsAString& aMessage)
  {
    mName = aName;
    mMessage = aMessage;
  }
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMError, DOMERROR_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_domerror_h__
