/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MoveTask.h"

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

NS_IMPL_ISUPPORTS_INHERITED0(MoveTask, FileSystemTaskBase)

MoveTask::MoveTask(FileSystemBase* aFileSystem,
                   const nsAString& aDirPath,
                   const nsAString& aSrcPath,
                   DOMFileImpl* aSrcFile,
                   const nsAString& aDestDirectory,
                   const nsAString& aDestName,
                   nsresult aErrorValue)
  : FileSystemTaskBase(aFileSystem)
  , mDirRealPath(aDirPath)
  , mSrcRealPath(aSrcPath)
  , mSrcFileImpl(aSrcFile)
  , mDestDirectory(aDestDirectory)
  , mDestName(aDestName)
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

MoveTask::MoveTask(FileSystemBase* aFileSystem,
                   const FileSystemMoveParams& aParam,
                   FileSystemRequestParent* aParent)
  : FileSystemTaskBase(aFileSystem, aParam, aParent)
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  mDirRealPath = aParam.directory();

  const FileSystemPathOrFileValue& src = aParam.src();
  if (src.type() == FileSystemPathOrFileValue::TnsString) {
    mSrcRealPath = src;
  } else {
    BlobParent* bp = static_cast<BlobParent*>(static_cast<PBlobParent*>(src));
    nsCOMPtr<nsIDOMBlob> blob = bp->GetBlob();
    MOZ_ASSERT(blob);
    mSrcFileImpl = static_cast<DOMFile*>(blob.get())->Impl();
  }

  mDestDirectory = aParam.destDirectory();
  mDestName = aParam.destName();
}

MoveTask::~MoveTask()
{
  MOZ_ASSERT(!mAbortableProgressPromise || NS_IsMainThread(),
             "mAbortableProgressPromise should be released on main thread!");
}

already_AddRefed<AbortableProgressPromise>
MoveTask::GetAbortableProgressPromise()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return nsRefPtr<AbortableProgressPromise>(mAbortableProgressPromise).forget();
}

FileSystemParams
MoveTask::GetRequestParams(const nsString& aFileSystem) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemMoveParams param;
  param.filesystem() = aFileSystem;
  if (mSrcFileImpl) {
    nsRefPtr<DOMFile> file = new DOMFile(mSrcFileImpl);
    BlobChild* actor
      = ContentChild::GetSingleton()->GetOrCreateActorForBlob(file);
    if (actor) {
      param.src() = actor;
    }
  } else {
    param.src() = mSrcRealPath;
  }
  param.destDirectory() = mDestDirectory;
  param.destName() = mDestName;
  return param;
}

FileSystemResponseValue
MoveTask::GetSuccessRequestResult() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return FileSystemBooleanResponse(true);
}

void
MoveTask::SetSuccessRequestResult(const FileSystemResponseValue& aValue)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  // Do nothing.
}

nsresult
MoveTask::Work()
{
  MOZ_ASSERT(FileSystemUtils::IsParentProcess(),
             "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");

  if (mFileSystem->IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  // Get the DOM path if a DOMFile is passed as the target.
  if (mSrcFileImpl) {
    if (!mFileSystem->GetRealPath(mSrcFileImpl, mSrcRealPath)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
    if (!FileSystemUtils::IsDescendantPath(mDirRealPath, mSrcRealPath)) {
      return NS_ERROR_DOM_FILESYSTEM_NO_MODIFICATION_ALLOWED_ERR;
    }
  }

  // Create nsIFile object from the source path.
  nsCOMPtr<nsIFile> srcFile = mFileSystem->GetLocalFile(mSrcRealPath);
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
  bool isFile = false;
  rv = srcFile->IsFile(&isFile);
  if (!isDirectory && !isFile) {
    // Neither directory or file.
    return NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;
  }

  if (isFile && !mFileSystem->IsSafeFile(srcFile)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // If the destination name is not passed, use the source name.
  if (mDestName.IsEmpty()) {
    mDestName = Substring(mSrcRealPath,
      mSrcRealPath.RFindChar(FileSystemUtils::kSeparatorChar) + 1);
  }

  // Get the destination path.
  nsString destRealPath;
  destRealPath = mDestDirectory +
                 NS_LITERAL_STRING(FILESYSTEM_DOM_PATH_SEPARATOR) +
                 mDestName;

  // Create nsIFile object from the destination path.
  nsCOMPtr<nsIFile> destFile = mFileSystem->GetLocalFile(destRealPath);
  if (!destFile) {
    return NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
  }

  // Check if destination exists.
  rv = destFile->Exists(&exists);
  if (exists) {
    return NS_ERROR_DOM_FILESYSTEM_PATH_EXISTS_ERR;
  }
  // Move the entry.
  nsCOMPtr<nsIFile> destParent;
  rv = destFile->GetParent(getter_AddRefs(destParent));
  nsString destName;
  rv = destFile->GetLeafName(destName);
  if (isFile) {
    MutexAutoLock lock(mMutex);
    mProgressFiles.AppendElement(mSrcRealPath);
    nsRefPtr<FileSystemNotifyBase> notify = new FileSystemNotifyBase(this);
    NS_DispatchToMainThread(notify);

    rv = srcFile->MoveTo(destParent, destName);
  } else if (isDirectory) {
    rv = srcFile->RenameTo(destParent, destName);
    if (NS_ERROR_FILE_ACCESS_DENIED != rv) {
      MutexAutoLock lock(mMutex);
      mProgressFiles.AppendElement(mSrcRealPath);
      nsRefPtr<FileSystemNotifyBase> notify = new FileSystemNotifyBase(this);
      NS_DispatchToMainThread(notify);
      return rv;
    }
    rv = MoveDirectory(srcFile, destRealPath);
  }
  return rv;
}

nsresult
MoveTask::MoveDirectory(nsCOMPtr<nsIFile> aSrcFile, const nsAString& destRealPath)
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
    nsString destName;
    subFile->GetLeafName(destName);
    if (!isSubDirectory) {
      nsCOMPtr<nsIFile> destFile = mFileSystem->GetLocalFile(destRealPath);
      if (!destFile) {
        return NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
      }

      nsString srcSubPath;
      rv = subFile->GetPath(srcSubPath);
      if (NS_FAILED(rv)) {
        break;
      }
      nsString srcSubRealPath;
      srcSubRealPath = Substring(srcSubPath,
        srcSubPath.RFind(mSrcRealPath));
      MutexAutoLock lock(mMutex);
      mProgressFiles.AppendElement(srcSubRealPath);
      nsRefPtr<FileSystemNotifyBase> notify = new FileSystemNotifyBase(this);
      NS_DispatchToMainThread(notify);

      rv = subFile->MoveTo(destFile, destName);
      if (NS_FAILED(rv)) {
        break;
      }
    } else {
      nsString destSubRealPath;
      destSubRealPath = destRealPath +
                 NS_LITERAL_STRING(FILESYSTEM_DOM_PATH_SEPARATOR) +
                 destName;
      rv = MoveDirectory(subFile, destSubRealPath);
      if (NS_FAILED(rv)) {
        break;
      }
    }
  }
  if (mAbort) {
    rv = NS_ERROR_ABORT;
  }
  return rv;
}

void
MoveTask::AbortCallback() {
  if (FileSystemUtils::IsParentProcess()) {
    mAbort = true;
    return;
  }
  SendAbort(); 
}

void
MoveTask::HandlerCallback()
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
MoveTask::SetRequestProgress(const FileSystemResponseValue& aValue)
{
  MutexAutoLock lock(mMutex);
  FileSystemDirectoryResponse r = aValue;
  nsString realPath = r.realPath();
  mProgressFiles.AppendElement(realPath);
}

FileSystemResponseValue
MoveTask::GetRequestProgress()
{
  MutexAutoLock lock(mMutex);
  FileSystemDirectoryResponse r;
  r.realPath() = mProgressFiles[0];
  mProgressFiles.RemoveElementAt(0);
  return r;
}

void
MoveTask::NotifyProgress()
{
  if (mProgressFiles.IsEmpty())
    return;
  MutexAutoLock lock(mMutex);
  nsString realPath = mProgressFiles[0];
  mProgressFiles.RemoveElementAt(0);
  AutoSafeJSContext cx;
  JSString* strValue = JS_NewUCStringCopyZ(cx, realPath.get());
  JS::Rooted<JS::Value> valValue(cx, STRING_TO_JSVAL(strValue));
  Optional<JS::Handle<JS::Value>> aJSValue;
  aJSValue.Value() = valValue;
  mAbortableProgressPromise->NotifyProgress(aJSValue);
}

void
MoveTask::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral("write");
}

} // namespace dom
} // namespace mozilla
