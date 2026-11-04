#!/usr/bin/env python3
"""Turn TamaPoke.ino into the C++ file PlatformIO compiles for the board.

Uses PlatformIO's own .ino converter (it adds the function prototypes an .ino
leaves out), so the renderer compiles the same translation unit as the
firmware. Run it with PlatformIO's Python: render.sh does.

    ino2cpp.py <TamaPoke.ino> <out.ino.cpp>
"""

import os
import shutil
import subprocess
import sys

from platformio.builder.tools.pioino import InoToCPPConverter


class _Node:
    def __init__(self, path):
        self._path = path

    def get_path(self):
        return self._path


class _Env:
    """The two SCons calls the converter makes: run the C++ preprocessor."""

    def VerboseAction(self, cmd, _label):  # noqa: N802 (SCons API name)
        return cmd

    def Execute(self, cmd):  # noqa: N802
        subprocess.check_call(cmd.replace("$CXX", os.environ.get("CXX", "g++")), shell=True)


def main():
    src, out = sys.argv[1], sys.argv[2]
    if not out.endswith(".ino.cpp"):
        sys.exit("output must be <name>.ino.cpp")
    work = out[: -len(".cpp")]  # the converter writes <ino>.cpp next to the .ino it reads
    shutil.copyfile(src, work)
    InoToCPPConverter(_Env()).convert([_Node(work)])
    # compiler messages should point at the real sketch, not the copy
    with open(out, encoding="utf-8") as f:
        text = f.read()
    with open(out, "w", encoding="utf-8") as f:
        f.write(text.replace('"%s"' % work, '"%s"' % os.path.abspath(src)))
    os.remove(work)


if __name__ == "__main__":
    main()
