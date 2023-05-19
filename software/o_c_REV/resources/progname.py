Import("env")

import subprocess

version = subprocess.check_output("tail -n1 'OC_version.h'|tr -d '\"'", shell=True).decode().strip()
git_rev = subprocess.check_output("git rev-parse --short HEAD", shell=True).decode().strip()
extras = ""

build_flags = env.ParseFlags(env['BUILD_FLAGS'])
defines = build_flags.get("CPPDEFINES")
if "FLIP_180" in defines:
    extras = "_flipped"
if "VOR" in defines:
    extras += "+VOR"

env.Replace(PROGNAME=f"o_C-phazerville-{version}{extras}-{git_rev}")
