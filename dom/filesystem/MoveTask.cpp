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
#include "nsIDOMFile.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED0(MoveTask, FileSystemTaskBase)

MoveTask::MoveTask(FileSystemBase* aFileSystem,
                   const nsAString& aDirPath,
                   const nsAString& aSrcPath,
                   nsIDOMFile* aSrcFile,
                   const nsAString& aDestDirectory,
                   const nsAString& aDestName,
                   nsresult aErrorValue)
  : FileSystemTaskBase(aFileSystem)
  , mDirRealPath(aDirPath)
  , mSrcRealPath(aSrcPath)
  , mSrcFile(aSrcFile)
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
  mAbortablePromise = new AbortablePromise(globalObject, this);
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
    mSrcFile = do_QueryInterface(blob);
    MOZ_ASSERT(blob, "mSrcFile should not be null.");
  }

  mDestDirectory = aParam.destDirectory();
  mDestName = aParam.destName();
}

MoveTask::~MoveTask()
{
  MOZ_ASSERT(!mAbortablePromise || NS_IsMainThread(),
             "mAbortablePromise should be released on main thread!");
}

already_AddRefed<AbortablePromise>
MoveTask::GetAbortablePromise()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return nsRefPtr<AbortablePromise>(mAbortablePromise).forget();
}

FileSystemParams
MoveTask::GetRequestParams(const nsString& aFileSystem) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemMoveParams param;
  param.filesystem() = aFileSystem;
  if (mSrcFile) {
    BlobChild* actor
      = ContentChild::GetSingleton()->GetOrCreateActorForBlob(mSrcFile);
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
  if (mSrcFile) {
    if (!mFileSystem->GetRealPath(mSrcFile, mSrcRealPath)) {
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
  rv = srcFile->RenameTo(destParent, destName);
  if (NS_ERROR_FILE_ACCESS_DENIED != rv) {
    return rv;
  }
  if (isFile) {
    rv = srcFile->MoveTo(destParent, destName);
  } else if (isDirectory) {
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
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&more)) && more && !bAbort) {
    nsCOMPtr<nsISupports> next;
    if (NS_FAILED(enumerator->GetNext(getter_AddRefs(next)))) {
      break;
    }
    nsCOMPtr<nsIFile> subfile = do_QueryInterface(next);
    if (!subfile) {
      continue;
    }
    bool isSubDirectory;
    subfile->IsDirectory(&isSubDirectory);
    nsString destName;
    subfile->GetLeafName(destName);
    if (!isSubDirectory) {
      nsCOMPtr<nsIFile> destFile = mFileSystem->GetLocalFile(destRealPath);
      if (!destFile) {
        return NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
      }
      rv = subfile->MoveTo(destFile, destName);
      if (NS_FAILED(rv)) {
        break;
      }
    } else {
      nsString destSubRealPath;
      destSubRealPath = destRealPath +
                 NS_LITERAL_STRING(FILESYSTEM_DOM_PATH_SEPARATOR) +
                 destName;
      rv = MoveDirectory(subfile, destSubRealPath);
      if (NS_FAILED(rv)) {
        break;
      }
    }
  }
  if (bAbort) {
    rv = NS_ERROR_ABORT;
  }
  return rv;
}

void
MoveTask::AbortCallback() {
  if (FileSystemUtils::IsParentProcess()) {
    bAbort = true;
    return;
  }
  SendAbortMove(); 
}

void
MoveTask::HandlerCallback()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (mFileSystem->IsShutdown()) {
    mAbortablePromise = nullptr;
    return;
  }

  if (HasError()) {
    nsRefPtr<DOMError> domError = new DOMError(mFileSystem->GetWindow(),
      mErrorValue);
    mAbortablePromise->MaybeRejectBrokenly(domError);
    mAbortablePromise = nullptr;
    return;
  }

  
  mAbortablePromise->MaybeResolve(JS::UndefinedHandleValue);
  mAbortablePromise = nullptr;
}

void
MoveTask::GetPermissionAccessType(nsCString& aAccess) const
{
  aAccess.AssignLiteral("write");
}

} // namespace dom
} // namespace mozilla
