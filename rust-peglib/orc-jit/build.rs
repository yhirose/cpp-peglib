use std::process::Command;

fn main() {
    let llvm_config = std::env::var("LLVM_CONFIG")
        .unwrap_or_else(|_| {
            if let Ok(out) = Command::new("brew").args(["--prefix", "llvm"]).output() {
                if out.status.success() {
                    let prefix = String::from_utf8(out.stdout).unwrap().trim().to_string();
                    let cfg = format!("{prefix}/bin/llvm-config");
                    if std::path::Path::new(&cfg).exists() {
                        return cfg;
                    }
                }
            }
            "llvm-config".to_string()
        });

    let run = |args: &[&str]| -> String {
        let out = Command::new(&llvm_config).args(args).output()
            .unwrap_or_else(|e| panic!("failed to run {llvm_config} {}: {e}", args.join(" ")));
        String::from_utf8(out.stdout).unwrap().trim().to_string()
    };

    let ldflags = run(&["--ldflags"]);
    for flag in ldflags.split_whitespace() {
        if let Some(dir) = flag.strip_prefix("-L") {
            println!("cargo:rustc-link-search=native={dir}");
        }
    }

    // Link shared LLVM library
    let libs = run(&["--link-shared", "--libs"]);
    for flag in libs.split_whitespace() {
        if let Some(lib) = flag.strip_prefix("-l") {
            println!("cargo:rustc-link-lib=dylib={lib}");
        }
    }

    let syslibs = run(&["--system-libs"]);
    for flag in syslibs.split_whitespace() {
        if let Some(lib) = flag.strip_prefix("-l") {
            println!("cargo:rustc-link-lib=dylib={lib}");
        }
    }

    #[cfg(target_os = "macos")]
    println!("cargo:rustc-link-lib=dylib=c++");
    #[cfg(target_os = "linux")]
    println!("cargo:rustc-link-lib=dylib=stdc++");
}
