use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

fn build_libsocketcan() -> Result<bool, Box<dyn std::error::Error>> {
    let home_path = env::var("CARGO_MANIFEST_DIR")?;
    let script_path = PathBuf::from(home_path).join("libsocketcan_builder.sh");

    let status = Command::new("sh").arg("-c").arg(&script_path).status()?;

    if status.success() {
        println!("script executed successfully");
        return Ok(true);
    } else {
        eprintln!("libsocketcan.a cannot be built");
    }

    Ok(false)
}

fn build_libcsp_bindings(out_path: PathBuf) -> Result<(), Box<dyn std::error::Error>> {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR")?;
    let source_dir = PathBuf::from(&(manifest_dir.clone())).join("../../Dev/libcspv2");
    let workspace_path = env::var("WORKSPACE_PATH").unwrap_or_else(|_| "/workspace".to_string());

    println!("Source directory: {:?}", source_dir);
    println!("Output directory: {:?}", out_path);

    // build this first
    let _ = build_libsocketcan();

    // waf configure
    let status = Command::new("./waf")
        .current_dir(&(source_dir.clone()))
        .arg("configure")
        .arg("--enable-examples")
        .arg("--enable-can-socketcan")
        .arg("--with-max-bind-port")
        .arg("32")
        .arg("--with-driver-usart")
        .arg("linux")
        .arg("--enable-promisc")
        .arg("--enable-if-zmqhub")
        .arg("--enable-rtable")
        .status()
        .expect("Failed to execute waf configure");

    if !status.success() {
        panic!("waf configure failed");
    }

    // waf build
    let status = Command::new("./waf")
        .current_dir(&(source_dir.clone()))
        .arg("build")
        .status()
        .expect("Failed to execute waf build");

    if !status.success() {
        panic!("waf build failed");
    }

    // copy output files into artifact
    let libcsp_static_lib = source_dir.join("build/libcsp.a");
    std::fs::copy(libcsp_static_lib, Path::new("../artifacts/libcsp.a")).unwrap();

    println!("cargo:rerun-if-changed=libcsp_wrapper.h");

    let clang_args = vec![
        format!("-I{}/Dev/libcspv2/include/", workspace_path),
        format!("-I{}/Dev/libcspv2/build/include/", workspace_path),
        "-DCSP_BUFFER_SIZE=256".to_string(),
        "-DCSP_CONN_RXQUEUE_LEN=1".to_string(),
    ];

    let bindings_libcsp = bindgen::Builder::default()
        .header("libcsp_wrapper.h")
        .clang_args(clang_args)
        .generate()
        .expect("couldn't GENERATE the bindings");

    let output_file = PathBuf::from(manifest_dir).join("../artifacts/bindings_libcsp.rs");
    Ok(bindings_libcsp.write_to_file(output_file)?)
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let out_path = PathBuf::from(std::env::var("OUT_DIR").unwrap());

    build_libcsp_bindings(out_path)
}
