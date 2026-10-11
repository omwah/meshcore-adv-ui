# PlatformIO pre-build hook: generate fonts, and (for device builds) prepare
# the patched MeshCore tree. Runs inside PlatformIO's Python.
import os
import subprocess
import sys

Import("env")  # noqa: F821  (provided by PlatformIO)

root = env.subst("$PROJECT_DIR")  # noqa: F821
subprocess.check_call([sys.executable, "-I", os.path.join(root, "scripts", "gen_fonts.py")])
if env.subst("$PIOPLATFORM") != "native":  # noqa: F821
    subprocess.check_call([os.path.join(root, "scripts", "prepare-deps.sh")])
