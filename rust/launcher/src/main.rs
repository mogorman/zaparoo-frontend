// SPDX-License-Identifier: LicenseRef-PolyForm-Noncommercial-1.0.0
// SPDX-FileCopyrightText: 2026 Callan Barrett
//
// Phase 1: Rust entry point. The full Qt application runs in the C++ glue
// (src/cpp/init.cpp). Phase 3 will progressively move each piece into Rust:
// models → ZaparooClient → Config → Logger → MiSterRuntime → QML app setup.

use std::ffi::c_int;
use std::os::raw::c_char;

extern "C" {
    fn zaparoo_run_launcher(argc: c_int, argv: *mut *mut c_char) -> c_int;
}

fn main() {
    // Collect argv as CStrings and build the raw argv pointer array Qt expects.
    let args: Vec<std::ffi::CString> = std::env::args()
        .map(|a| std::ffi::CString::new(a).expect("arg contains null byte"))
        .collect();
    let mut argv: Vec<*mut c_char> = args
        .iter()
        .map(|a| a.as_ptr().cast_mut())
        .collect();
    argv.push(std::ptr::null_mut()); // POSIX-style null terminator

    let exit_code = unsafe { zaparoo_run_launcher(args.len() as c_int, argv.as_mut_ptr()) };
    std::process::exit(exit_code);
}
