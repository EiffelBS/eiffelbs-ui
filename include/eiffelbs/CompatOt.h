// CompatOt.h
// eiffelbs-ui - TRANSITIONAL compatibility shim for codebases that still
// spell the design system `ot::` (the old OpenTimbre port namespace).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Provides a plain namespace alias so existing call sites keep compiling
// against this library:
//
//     #include <eiffelbs/CompatOt.h>
//     ot::bgDark();          // -> ebs::bgDark()
//
// Notes:
// - Application-local classes declared in the old namespace (e.g.
//   ot::OTLookAndFeel) must be mapped during migration, typically with a
//   local `using OTLookAndFeel = ebs::LookAndFeel;`.
// - Do NOT include this header in a TU that still defines its own real
//   `namespace ot { ... }` (OTTheme.h): an alias and a definition cannot
//   coexist. It is meant for the cutover commit, after the local copies
//   are deleted.
// - This shim is scheduled to disappear once all consumers are migrated;
//   new code must use `ebs::` directly.

#pragma once

#include "eiffelbs/Theme.h"

namespace ot = ::ebs;
