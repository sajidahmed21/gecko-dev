/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/plugins/PluginModuleChild.h"
#include "ContentChild.h"
#include "CrashReporterChild.h"
#include "nsXULAppAPI.h"

using mozilla::plugins::PluginModuleChild;

namespace mozilla {
namespace dom {

/*static*/
PCrashReporterChild*
CrashReporterChild::GetCrashReporter()
{
  return nullptr;
}

} // namespace dom
} // namespace mozilla
