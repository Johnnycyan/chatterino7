// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// Force-included into the dav1d build on Windows (see build.yml).
//
// dav1d 1.5.4's include/compat/msvc/stdatomic.h uses __has_include_next
// unguarded, which newer MSVC (14.5x) rejects with C7780. Fixed upstream in
// dav1d commit "compat: Fix building with newer versions of MSVC"
// (2026-09-09); remove this once Conan Center ships a release containing it.
#ifndef __has_include_next
#    define __has_include_next(x) 0
#endif
