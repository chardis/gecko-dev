/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbortableCallback.h"

#include "mozilla/dom/NativeAbortableHandler.h"

namespace mozilla {
namespace dom {

// AbortableCallback

NS_IMPL_CYCLE_COLLECTING_ADDREF(AbortableCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AbortableCallback)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbortableCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION_0(AbortableCallback)

AbortableCallback::AbortableCallback()
{
}

AbortableCallback::~AbortableCallback()
{
}

// WrapperAbortableCallback

NS_IMPL_ADDREF_INHERITED(WrapperAbortableCallback, AbortableCallback)
NS_IMPL_RELEASE_INHERITED(WrapperAbortableCallback, AbortableCallback)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WrapperAbortableCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(AbortableCallback)
NS_IMPL_CYCLE_COLLECTION_INHERITED(WrapperAbortableCallback, AbortableCallback,
                                   mCallback)

WrapperAbortableCallback::WrapperAbortableCallback(AbortCallback* aCallback)
  : mCallback(aCallback)
{
}

WrapperAbortableCallback::~WrapperAbortableCallback()
{
}

void
WrapperAbortableCallback::Call()
{
  ErrorResult rv;
  mCallback->Call(rv);
}

// NativeAbortableCallback

NS_IMPL_ADDREF_INHERITED(NativeAbortableCallback, AbortableCallback)
NS_IMPL_RELEASE_INHERITED(NativeAbortableCallback, AbortableCallback)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(NativeAbortableCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(AbortableCallback)
NS_IMPL_CYCLE_COLLECTION_INHERITED(NativeAbortableCallback, AbortableCallback,
                                   mHandler)

NativeAbortableCallback::NativeAbortableCallback(NativeAbortableHandler* aHandler)
  : mHandler(aHandler)
{
  MOZ_ASSERT(aHandler);
}

NativeAbortableCallback::~NativeAbortableCallback()
{
}

void
NativeAbortableCallback::Call()
{
  mHandler->AbortCallback();
}

} // namespace dom
} // namespace mozilla
