/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EnumerateTask_h
#define mozilla_dom_EnumerateTask_h

#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/dom/NativeAbortableHandler.h"
#include "mozilla/dom/AbortableProgressPromise.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {

class DOMFileImpl;
class AbortableProgressPromise;

class EnumerateTask MOZ_FINAL
  : public FileSystemTaskBase
  , public NativeAbortableHandler
{
  NS_DECL_ISUPPORTS_INHERITED
public:
  EnumerateTask(FileSystemBase* aFileSystem,
                const nsAString& aDirPath,
                const nsAString& aTargetPath,
                bool aRecursive);
  EnumerateTask(FileSystemBase* aFileSystem,
                const FileSystemEnumerateParams& aParam,
                FileSystemRequestParent* aParent);

  already_AddRefed<AbortableProgressPromise>
  GetAbortableProgressPromise();

  virtual void
  GetPermissionAccessType(nsCString& aAccess) const MOZ_OVERRIDE;

  virtual void
  AbortCallback() MOZ_OVERRIDE;

  virtual void
  NextCallback();
protected:
  virtual FileSystemParams
  GetRequestParams(const nsString& aFileSystem) const MOZ_OVERRIDE;

  virtual FileSystemResponseValue
  GetSuccessRequestResult() const MOZ_OVERRIDE;

  virtual void
  SetSuccessRequestResult(const FileSystemResponseValue& aValue) MOZ_OVERRIDE;

  virtual nsresult
  Work() MOZ_OVERRIDE;

  virtual void
  SetRequestProgress(const FileSystemResponseValue& aValue) MOZ_OVERRIDE;

  virtual FileSystemResponseValue
  GetRequestProgress() MOZ_OVERRIDE;

  virtual void
  HandlerCallback() MOZ_OVERRIDE;

  virtual void
  NotifyProgress() MOZ_OVERRIDE;

  nsresult EnumerateDirectory(nsCOMPtr<nsIFile> aSrcFile);
private:
  virtual
  ~EnumerateTask();

  nsRefPtr<AbortableProgressPromise> mAbortableProgressPromise;
  nsString mTargetRealPath;
  nsString mDirRealPath;
  bool mRecursive;
  bool mReturnValue;
  nsAutoTArray<nsCOMPtr<nsIFile>, 10> mQueue;
  nsAutoTArray<nsCOMPtr<nsIFile>, 10> mProgressQueue;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_EnumerateTask_h
