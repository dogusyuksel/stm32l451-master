use std::env;
use std::path::{Path, PathBuf};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let out_dir = env::var("OUT_DIR").unwrap();
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let external_lib_path = PathBuf::from(&(manifest_dir.clone())).join("../artifacts");

    let libcsp_path = external_lib_path.clone().join("bindings_libcsp.rs");
    println!("cargo:rerun-if-changed={}", libcsp_path.display());
    std::fs::copy(libcsp_path, Path::new(&out_dir).join("bindings_libcsp.rs")).unwrap();

    println!(
        "cargo:rustc-link-search=native={}",
        external_lib_path.display()
    );

    // link static files
    println!("cargo:rustc-link-lib=static=csp");
    println!("cargo:rustc-link-lib=static=socketcan");

    Ok(())
}
