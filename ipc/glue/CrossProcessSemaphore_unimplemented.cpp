/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossProcessSemaphore.h"

#include "nsDebug.h"

namespace mozilla {

CrossProcessSemaphore::CrossProcessSemaphore(const char*, uint32_t)
{
  MOZ_CRASH("Cross-process semaphores not allowed on this platform.");
}

CrossProcessSemaphore::CrossProcessSemaphore(CrossProcessSemaphoreHandle)
{
  MOZ_CRASH("Cross-process semaphores not allowed on this platform.");
}

CrossProcessSemaphore::~CrossProcessSemaphore()
{
  MOZ_CRASH("Cross-process semaphores not allowed on this platform - woah! We should've aborted by now!");
}

bool
CrossProcessSemaphore::Wait(Maybe<TimeDuration> aWaitTime)
{
  MOZ_CRASH("Cross-process semaphores not allowed on this platform - woah! We should've aborted by now!");
  return false;
}

void
CrossProcessSemaphore::Signal()
{
  MOZ_CRASH("Cross-process semaphores not allowed on this platform - woah! We should've aborted by now!");
}

CrossProcessSemaphoreHandle
CrossProcessSemaphore::ShareToProcess(base::ProcessId aTargetPid)
{
  MOZ_CRASH("Cross-process semaphores not allowed on this platform - woah! We should've aborted by now!");
  return 0;
}

}
