// SPDX-License-Identifier: LicenseRef-PolyForm-Noncommercial-1.0.0
// SPDX-FileCopyrightText: 2026 Callan Barrett

fn main() {
    // These env vars are set by cmake/ZaparooRust.cmake via corrosion_set_env_vars.
    // When building outside CMake (e.g. plain `cargo build`), they must be set
    // manually or the build will fail with a clear message.
    let qt_include = env("ZAPAROO_QT_INCLUDE");
    let core_src = env("ZAPAROO_CORE_SRC");
    let third_party = env("ZAPAROO_THIRD_PARTY");
    let app_src = env("ZAPAROO_APP_SRC");

    let mut build = cc::Build::new();
    build
        .cpp(true)
        .std("c++17")
        // Qt6 root include dir (e.g. /usr/include/qt6).
        .include(&qt_include)
        // Per-module subdirs so bare `#include <QFoo>` resolves without module prefix.
        // Must mirror every Qt module whose headers are transitively included by
        // src/core headers (ZaparooClient.h pulls in QAbstractSocket → QtNetwork).
        .include(format!("{qt_include}/QtCore"))
        .include(format!("{qt_include}/QtGui"))
        .include(format!("{qt_include}/QtNetwork"))
        .include(format!("{qt_include}/QtQml"))
        .include(format!("{qt_include}/QtQuick"))
        .include(format!("{qt_include}/QtQuickControls2"))
        .include(format!("{qt_include}/QtWebSockets"))
        // Zaparoo C++ source dirs.
        .include(&core_src)
        .include(&app_src)
        .include(&third_party)
        .file("src/cpp/init.cpp");

    // Propagate compile-time defines that match the CMake build's defines.
    if std::env::var("ZAPAROO_MISTER").is_ok() {
        build.define("ZAPAROO_MISTER", None);
    }
    if std::env::var("ZAPAROO_DEV_BUILD").is_ok() {
        build.define("ZAPAROO_DEV_BUILD", None);
    }

    build.compile("zaparoo_init");

    // Re-run build.rs if init.cpp or any of the env vars change.
    println!("cargo:rerun-if-changed=src/cpp/init.cpp");
    println!("cargo:rerun-if-env-changed=ZAPAROO_QT_INCLUDE");
    println!("cargo:rerun-if-env-changed=ZAPAROO_CORE_SRC");
    println!("cargo:rerun-if-env-changed=ZAPAROO_THIRD_PARTY");
    println!("cargo:rerun-if-env-changed=ZAPAROO_APP_SRC");
    println!("cargo:rerun-if-env-changed=ZAPAROO_MISTER");
    println!("cargo:rerun-if-env-changed=ZAPAROO_DEV_BUILD");
}

fn env(key: &str) -> String {
    std::env::var(key).unwrap_or_else(|_| {
        panic!(
            "build.rs: env var {key} not set.\n\
             Build via CMake with -DZAPAROO_BUILD_RUST=ON, or set it manually."
        )
    })
}
