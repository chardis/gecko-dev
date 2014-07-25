/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/FileSystemRequestParent.h"

#include "CreateDirectoryTask.h"
#include "CreateFileTask.h"
#include "GetFileOrDirectoryTask.h"
#include "RemoveTask.h"
#include "MoveTask.h"
#include "EnumerateTask.h"
#include "mozilla/AppProcessChecker.h"
#include "mozilla/dom/FileSystemBase.h"

namespace mozilla {
namespace dom {

FileSystemRequestParent::FileSystemRequestParent()
{
}

FileSystemRequestParent::~FileSystemRequestParent()
{
}

#define FILESYSTEM_REQUEST_PARENT_DISPATCH_ENTRY(name)                         \
    case FileSystemParams::TFileSystem##name##Params: {                        \
      const FileSystem##name##Params& p = aParams;                             \
      mFileSystem = FileSystemBase::FromString(p.filesystem());                \
      mTask = new name##Task(mFileSystem, p, this);                            \
      break;                                                                   \
    }

bool
FileSystemRequestParent::Dispatch(ContentParent* aParent,
                                  const FileSystemParams& aParams)
{
  MOZ_ASSERT(aParent, "aParent should not be null.");
  switch (aParams.type()) {
    FILESYSTEM_REQUEST_PARENT_DISPATCH_ENTRY(CreateDirectory)
    FILESYSTEM_REQUEST_PARENT_DISPATCH_ENTRY(CreateFile)
    FILESYSTEM_REQUEST_PARENT_DISPATCH_ENTRY(GetFileOrDirectory)
    FILESYSTEM_REQUEST_PARENT_DISPATCH_ENTRY(Remove)
    FILESYSTEM_REQUEST_PARENT_DISPATCH_ENTRY(Move)
    FILESYSTEM_REQUEST_PARENT_DISPATCH_ENTRY(Enumerate)

    default: {
      NS_RUNTIMEABORT("not reached");
      break;
    }
  }

  if (NS_WARN_IF(!mTask || !mFileSystem)) {
    // Should never reach here.
    return false;
  }

  if (!mFileSystem->IsTesting()) {
    // Check the content process permission.

    nsCString access;
    mTask->GetPermissionAccessType(access);

    nsAutoCString permissionName;
    permissionName = mFileSystem->GetPermission();
    permissionName.Append('-');
    permissionName.Append(access);

    if (!AssertAppProcessPermission(aParent, permissionName.get())) {
      return false;
    }
  }

  mTask->Start();
  return true;
}

void
FileSystemRequestParent::ActorDestroy(ActorDestroyReason why)
{
  if (!mFileSystem) {
    return;
  }
  mTask->Abort();
  mFileSystem->Shutdown();
  mFileSystem = nullptr;
}

bool
FileSystemRequestParent::RecvAbortMove()
{
  mTask->Abort();
  return true;
}

bool
FileSystemRequestParent::RecvAbortEnumerate()
{
  mTask->Abort();
  return true;
}

bool
FileSystemRequestParent::RecvNextEnumerate()
{
  mTask->Next();
  return true;
}

} // namespace dom
} // namespace mozilla
