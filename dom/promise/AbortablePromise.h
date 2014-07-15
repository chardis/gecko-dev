/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AbortablePromise_h__
#define mozilla_dom_AbortablePromise_h__

#include "mozilla/dom/Promise.h"

struct JSContext;

namespace mozilla {
namespace dom {

class AbortCallback;
class NativeAbortableHandler;
class AbortableCallback;

class AbortablePromise
  : public Promise
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AbortablePromise, Promise)

public:
  // Constructor used to create native AbortablePromise with C++.
  AbortablePromise(nsIGlobalObject* aGlobal,
                   NativeAbortableHandler* aAbortable);

protected:
  // Constructor used to create AbortablePromise for JavaScript. It should be
  // called by the static AbortablePromise::Constructor.
  AbortablePromise(nsIGlobalObject* aGlobal);

  virtual
  ~AbortablePromise();

public:
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<AbortablePromise>
  Constructor(const GlobalObject& aGlobal, PromiseInit& aInit,
              AbortCallback& aAbortCallback, ErrorResult& aRv);

  void
  Abort();

protected:
  // The callback function to abort the promise.
  nsRefPtr<AbortableCallback> mAbortableCallback;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AbortablePromise_h__
