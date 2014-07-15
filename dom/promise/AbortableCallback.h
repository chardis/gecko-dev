/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AbortableCallback_h
#define mozilla_dom_AbortableCallback_h

#include "mozilla/dom/AbortablePromiseBinding.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

class NativeAbortableHandler;

// This is the base class for any AbortableCallback.
class AbortableCallback : public nsISupports
{
protected:
  virtual
  ~AbortableCallback();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(AbortableCallback)

  AbortableCallback();

  virtual void
  Call() = 0;
};

// This is a wrapper for the JS abort Callback.
class WrapperAbortableCallback MOZ_FINAL : public AbortableCallback
{
protected:
  virtual
  ~WrapperAbortableCallback();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WrapperAbortableCallback,
                                           AbortableCallback)

  WrapperAbortableCallback(AbortCallback* aCallback);

  void
  Call() MOZ_OVERRIDE;

private:
  nsRefPtr<AbortCallback> mCallback;
};

// NativeAbortableCallback wraps a NativeAbortableHandler.
class NativeAbortableCallback MOZ_FINAL : public AbortableCallback
{
protected:
  virtual
  ~NativeAbortableCallback();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(NativeAbortableCallback,
                                           AbortableCallback)

  NativeAbortableCallback(NativeAbortableHandler* aHandler);

  void
  Call() MOZ_OVERRIDE;

private:
  nsRefPtr<NativeAbortableHandler> mHandler;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AbortableCallback_h
