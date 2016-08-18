use std::env;
use std::fs::File;
use std::io::Write;
use std::path::Path;
use std::process::Command;

fn main() {
    let out_dir = env::var("OUT_DIR").unwrap();
    let out_path = Path::new(&out_dir).join("version.rs");
    let mut f = File::create(&out_path).unwrap();
    // Call git to get the best available version descriptor.
    let git_output = Command::new("git").arg("describe")
                                        .arg("--dirty")
                                        .arg("--always")
                                        .arg("--tags")
                                        .output()
                                        .expect("failed to get git version");
    let version = String::from_utf8(git_output.stdout).expect("couldn't parse git output");
    f.write_all(version.trim().as_bytes()).unwrap();
}

