/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AbortablePromise.h"

#include "AbortableCallback.h"
#include "mozilla/dom/AbortablePromiseBinding.h"
#include "mozilla/dom/NativeAbortableHandler.h"
#include "mozilla/ErrorResult.h"
#include "PromiseCallback.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS0(NativeAbortableHandler)

NS_IMPL_ADDREF_INHERITED(AbortablePromise, Promise)
NS_IMPL_RELEASE_INHERITED(AbortablePromise, Promise)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AbortablePromise)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(Promise)
NS_IMPL_CYCLE_COLLECTION_INHERITED(AbortablePromise, Promise, mAbortableCallback)

AbortablePromise::AbortablePromise(nsIGlobalObject* aGlobal,
                                   NativeAbortableHandler* aAbortable)
  : Promise(aGlobal)
{
  MOZ_ASSERT(aAbortable);
  mAbortableCallback = new NativeAbortableCallback(aAbortable);
}

AbortablePromise::AbortablePromise(nsIGlobalObject* aGlobal)
  : Promise(aGlobal)
{
}

AbortablePromise::~AbortablePromise()
{
}

JSObject*
AbortablePromise::WrapObject(JSContext* aCx)
{
  return AbortablePromiseBinding::Wrap(aCx, this);
}

/* static */ already_AddRefed<AbortablePromise>
AbortablePromise::Constructor(const GlobalObject& aGlobal, PromiseInit& aInit,
            AbortCallback& aAbortCallback, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();

  nsCOMPtr<nsIGlobalObject> global;
  global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<AbortablePromise> promise = new AbortablePromise(global);

  JS::Rooted<JSObject*> resolveFunc(cx,
                                    CreateFunction(cx, aGlobal.Get(), promise,
                                    PromiseCallback::Resolve));
  if (!resolveFunc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  JS::Rooted<JSObject*> rejectFunc(cx,
                                   CreateFunction(cx, aGlobal.Get(), promise,
                                   PromiseCallback::Reject));
  if (!rejectFunc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  aInit.Call(resolveFunc, rejectFunc, aRv, CallbackObject::eRethrowExceptions);
  aRv.WouldReportJSException();

  if (aRv.IsJSException()) {
    JS::Rooted<JS::Value> value(cx);
    aRv.StealJSException(cx, &value);

    // we want the same behavior as this JS implementation:
    // function Promise(arg) { try { arg(a, b); } catch (e) { this.reject(e); }}
    if (!JS_WrapValue(cx, &value)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    promise->MaybeRejectInternal(cx, value);
  }

  promise->mAbortableCallback = new WrapperAbortableCallback(&aAbortCallback);
  return promise.forget();
}

void
AbortablePromise::Abort()
{
  if (mState != Pending || !mAbortableCallback) {
    return;
  }
  this->MaybeReject(NS_ERROR_ABORT);
  mAbortableCallback->Call();
}

} // namespace dom
} // namespace mozilla
