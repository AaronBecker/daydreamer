extern crate board;
extern crate rand;

use std::env;
use std::fs::File;
use std::io::Write;
use std::path::Path;
use std::process::Command;

mod gen_bitboards;
use gen_bitboards::write_bitboards;

fn main() {
    let out_dir = env::var("OUT_DIR").unwrap();
    write_version(File::create(Path::new(&out_dir).join("version.rs")).unwrap());
    write_bitboards(File::create(Path::new(&out_dir).join("generated_bitboards.rs")).unwrap());
}

fn write_version(mut f: File) {
    // Call git to get the best available version descriptor.
    let git_desc = Command::new("git")
        .arg("describe")
        .arg("--abbrev=7")
        .arg("--always")
        .arg("--dirty")
        .arg("--tags")
        .output()
        .expect("failed to get git version");
    let git_rev = Command::new("git")
        .arg("rev-parse")
        .arg("--short")
        .arg("HEAD")
        .output()
        .expect("failed to get git revision");

    let version = String::from_utf8(git_desc.stdout).expect("couldn't parse git describe");
    let revision = String::from_utf8(git_rev.stdout).expect("couldn't parse git rev-parse");
    let tag = if !version.contains(revision.trim()) {
        format!("{}-{}", version.trim(), revision.trim())
    } else {
        format!("{}", version.trim())
    };
    f.write_all(tag.as_bytes()).unwrap();
}
