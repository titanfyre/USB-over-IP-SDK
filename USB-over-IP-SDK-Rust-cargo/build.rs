use std::{env, fs, path::PathBuf};
fn main() {
    let dir = env::var_os("USB_OVER_IP_SDK_SDK_LIB_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap()).join("native")
        });
    println!("cargo:rustc-link-search=native={}", dir.display());
    println!("cargo:rustc-link-lib=dylib=USBoverIPSDKJsonSDK");
    println!("cargo:rerun-if-env-changed=USB_OVER_IP_SDK_SDK_LIB_DIR");
    println!(
        "cargo:rerun-if-changed={}",
        dir.join("USBoverIPSDKJsonSDK.dll").display()
    );
    let out = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let profile = out.ancestors().nth(3).unwrap();
    for target in [profile.to_path_buf(), profile.join("deps")] {
        fs::create_dir_all(&target).unwrap();
        fs::copy(
            dir.join("USBoverIPSDKJsonSDK.dll"),
            target.join("USBoverIPSDKJsonSDK.dll"),
        )
        .expect("Download the complete release or build USBoverIPSDKJsonSDK first");
    }
}
