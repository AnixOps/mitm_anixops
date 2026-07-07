use std::process::Command;

fn main() {
    let libs = pkg_config_args(&["--libs-only-L", "--libs-only-l"]);
    for token in libs.split_whitespace() {
        if let Some(path) = token.strip_prefix("-L") {
            println!("cargo:rustc-link-search=native={}", path);
        } else if let Some(lib) = token.strip_prefix("-l") {
            println!("cargo:rustc-link-lib={}", lib);
        }
    }
}

fn pkg_config_args(args: &[&str]) -> String {
    let output = Command::new("pkg-config")
        .args(args)
        .arg("mitm_anixops")
        .output()
        .expect("pkg-config must be available for mitm-anixops");
    if !output.status.success() {
        panic!(
            "pkg-config failed for mitm_anixops: {}",
            String::from_utf8_lossy(&output.stderr)
        );
    }
    String::from_utf8(output.stdout).expect("pkg-config output must be utf-8")
}
