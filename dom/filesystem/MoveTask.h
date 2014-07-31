/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MoveTask_h
#define mozilla_dom_MoveTask_h

#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/dom/NativeAbortableHandler.h"
#include "mozilla/dom/AbortableProgressPromise.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {

class DOMFileImpl;
class AbortableProgressPromise;

class MoveTask MOZ_FINAL
  : public FileSystemTaskBase
  , public NativeAbortableHandler
{
  NS_DECL_ISUPPORTS_INHERITED
public:
  MoveTask(FileSystemBase* aFileSystem,
           const nsAString& aDirPath,
           const nsAString& aSrcPath,
           DOMFileImpl* aSrcFile,
           const nsAString& aDestDirectory,
           const nsAString& aDestName,
           nsresult aErrorValue);
  MoveTask(FileSystemBase* aFileSystem,
           const FileSystemMoveParams& aParam,
           FileSystemRequestParent* aParent);

  already_AddRefed<AbortableProgressPromise>
  GetAbortableProgressPromise();

  virtual void
  GetPermissionAccessType(nsCString& aAccess) const MOZ_OVERRIDE;

  virtual void
  AbortCallback() MOZ_OVERRIDE;

protected:
  virtual FileSystemParams
  GetRequestParams(const nsString& aFileSystem) const MOZ_OVERRIDE;

  virtual FileSystemResponseValue
  GetSuccessRequestResult() const MOZ_OVERRIDE;

  virtual void
  SetSuccessRequestResult(const FileSystemResponseValue& aValue) MOZ_OVERRIDE;

  virtual void
  SetRequestProgress(const FileSystemResponseValue& aValue) MOZ_OVERRIDE;

  virtual FileSystemResponseValue
  GetRequestProgress() MOZ_OVERRIDE;

  virtual nsresult
  Work() MOZ_OVERRIDE;

  virtual void
  HandlerCallback() MOZ_OVERRIDE;

  void
  NotifyProgress() MOZ_OVERRIDE;

  nsresult MoveDirectory(nsCOMPtr<nsIFile> aSrcFile, const nsAString& destRealPath);
private:
  virtual
  ~MoveTask();

  nsRefPtr<AbortableProgressPromise> mAbortableProgressPromise;
  nsString mDirRealPath;
  nsString mSrcRealPath;
  // This cannot be a DOMFile because this object will be used on a different
  // thread and DOMFile is not thread-safe. Let's use the DOMFileImpl instead.
  nsRefPtr<DOMFileImpl> mSrcFileImpl;
  nsString mDestDirectory;
  nsString mDestName;
  nsTArray<nsString> mProgressFiles;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MoveTask_h
