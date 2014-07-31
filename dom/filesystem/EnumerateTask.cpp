/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EnumerateTask.h"

#include "DOMError.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/unused.h"
#include "nsDOMFile.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "nsStringGlue.h"
#include "FileSystemNotifyBase.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED0(EnumerateTask, FileSystemTaskBase)

EnumerateTask::EnumerateTask(FileSystemBase* aFileSystem,
                             const nsAString& aDirPath,
                             const nsAString& aTargetPath,
                             bool aRecursive)
  : FileSystemTaskBase(aFileSystem)
  , mTargetRealPath(aTargetPath)
  , mDirRealPath(aDirPath)
  , mRecursive(aRecursive)
  , mReturnValue(false)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (!aFileSystem) {
    return;
  }
  nsCOMPtr<nsIGlobalObject> globalObject =
    do_QueryInterface(aFileSystem->GetWindow());
  if (!globalObject) {
    return;
  }
  mAbortableProgressPromise = new AbortableProgressPromise(globalObject, this);
}
  
EnumerateTask::EnumerateTask(FileSystemBase* aFileSystem,
                             const FileSystemEnumerateParams& aParam,
                             FileSystemRequestParent* aParent)
  : FileSystemTaskBase(aFileSystem, aParam, aParent)
  , mRecursive(false)
  , mReturnValue(false)
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  
  mDirRealPath = aParam.directory();
  mRecursive = aParam.recursive();

  const FileSystemPathOrFileValue& target = aParam.target();

  if (target.type() == FileSystemPathOrFileValue::TnsString) {
    mTargetRealPath = target;
    return;
  }
}

EnumerateTask::~EnumerateTask()
{
  MOZ_ASSERT(!mAbortableProgressPromise || NS_IsMainThread(),
             "mAbortableProgressPromise should be released on main thread!");
}

already_AddRefed<AbortableProgressPromise>
EnumerateTask::GetAbortableProgressPromise()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return nsRefPtr<AbortableProgressPromise>(mAbortableProgressPromise).forget();
}

FileSystemParams
EnumerateTask::GetRequestParams(const nsString& aFileSystem) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemEnumerateParams param;
  param.filesystem() = aFileSystem;
  param.directory() = mDirRealPath;
  param.recursive() = mRecursive;
  param.target() = mTargetRealPath;
  return param;
}

FileSystemResponseValue
EnumerateTask::GetSuccessRequestResult() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return FileSystemBooleanResponse(true);
}

void
EnumerateTask::SetSuccessRequestResult(const FileSystemResponseValue& aValue)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  // Do nothing.
}

nsresult
EnumerateTask::Work()
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");
  if (mFileSystem->IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  if (!FileSystemUtils::IsDescendantPath(mDirRealPath, mTargetRealPath)) {
    return NS_ERROR_DOM_FILESYSTEM_NO_MODIFICATION_ALLOWED_ERR;
  }

  // Create nsIFile object from the source path.
  nsCOMPtr<nsIFile> srcFile = mFileSystem->GetLocalFile(mTargetRealPath);
  if (!srcFile) {
    return NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
  }

  // Check if the source entry exits.
  bool exists = false;
  nsresult rv = srcFile->Exists(&exists);
  if (!exists) {
    return NS_ERROR_DOM_FILE_NOT_FOUND_ERR;
  }

  // Check the source entry type.
  bool isDirectory = false;
  rv = srcFile->IsDirectory(&isDirectory);
  if (!isDirectory) {
    // Neither directory or file.
    return NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;
  }

  rv = EnumerateDirectory(srcFile);
  MonitorAutoLock monitor(mMonitor);
  while (mRecursive && !mQueue.IsEmpty()) {
    monitor.Wait();
    nsCOMPtr<nsIFile>& file = mQueue[0];
    rv = EnumerateDirectory(file);
    mQueue.RemoveElementAt(0);
  }
  return rv;
}

nsresult
EnumerateTask::EnumerateDirectory(nsCOMPtr<nsIFile> aSrcFile)
{
  nsresult rv;
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = aSrcFile->GetDirectoryEntries(getter_AddRefs(enumerator));
  if (NS_FAILED(rv))
    return rv;

  bool more;
  while (NS_SUCCEEDED((rv = enumerator->HasMoreElements(&more))) && more && !mAbort) {
    nsCOMPtr<nsISupports> next;
    if (NS_FAILED((rv = enumerator->GetNext(getter_AddRefs(next))))) {
      break;
    }
    nsCOMPtr<nsIFile> subFile = do_QueryInterface(next);
    if (!subFile) {
      continue;
    }
    bool isSubDirectory;
    subFile->IsDirectory(&isSubDirectory);
    if (!isSubDirectory) {
      nsString srcSubPath;
      rv = subFile->GetPath(srcSubPath);
      if (NS_FAILED(rv)) {
        break;
      }
      nsString srcSubRealPath;
      srcSubRealPath = Substring(srcSubPath,
        srcSubPath.RFind(mTargetRealPath));
      mProgressQueue.AppendElement(subFile);
    } else {
      mProgressQueue.AppendElement(subFile);
      if (mRecursive)
        mProgressQueue.AppendElement(subFile);
    }
  }
  if (mAbort) {
    rv = NS_ERROR_ABORT;
  }
  nsRefPtr<FileSystemNotifyBase> notify = new FileSystemNotifyBase(this);
  NS_DispatchToMainThread(notify);

  return rv;
}

void
EnumerateTask::AbortCallback() {
  if (FileSystemUtils::IsParentProcess()) {
    mAbort = true;
    return;
  }
  SendAbort(); 
}

void
EnumerateTask::NextCallback() {
  if (FileSystemUtils::IsParentProcess()) {
    MonitorAutoLock monitor(mMonitor);
    monitor.Notify();
    return;
  }
  SendNextEnumerate(); 
}

void
EnumerateTask::HandlerCallback()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (mFileSystem->IsShutdown()) {
    mAbortableProgressPromise = nullptr;
    return;
  }

  if (HasError()) {
    nsRefPtr<DOMError> domError = new DOMError(mFileSystem->GetWindow(),
      mErrorValue);
    mAbortableProgressPromise->MaybeRejectBrokenly(domError);
    mAbortableProgressPromise = nullptr;
    return;
  }

  
  mAbortableProgressPromise->MaybeResolve(JS::UndefinedHandleValue);
  mAbortableProgressPromise = nullptr;
}

void
EnumerateTask::SetRequestProgress(const FileSystemResponseValue& aValue)
{
/*  MutexAutoLock lock(mMutex);
  FileSystemDirectoryResponse r = aValue;
  nsString realPath = r.realPath();
  mProgressFiles.AppendElement(realPath);*/
}

FileSystemResponseValue
EnumerateTask::GetRequestProgress()
{
  FileSystemDirectoryResponse r;
  /* MutexAutoLock lock(mMutex);
  r.realPath() = mProgressFiles[0];
  mProgressFiles.RemoveElementAt(0);*/
  return r;
}

void EnumerateTask::NotifyProgress()
{
}

void
EnumerateTask::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral("write");
}

} // namespace dom
} // namespace mozilla
