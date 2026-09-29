#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Keep the information collected in this script synchronized with `runtime.py`.

# NOTE: this can run as a standalone script, called directly from Python
# (even though it's located inside a package).

__all__ = (
    "url_from_blender",
)


def url_from_blender() -> str:
    import re
    import struct
    import platform
    import subprocess
    import sys
    import urllib.parse
    from pathlib import Path

    print("Collecting system information...")

    query_params = {"type": "bug_report", "project": "blender"}

    query_params["os"] = "{:s} {:d} Bits".format(
        platform.platform(),
        struct.calcsize("P") * 8,
    )

    # There doesn't appear to be a easy way to collect GPU information in Python
    # if Blender isn't opening and we can't import the GPU module.
    # So just tell users to follow a written guide.
    query_params["gpu"] = (
        "Follow our guide to collect this information:\n"
        "https://developer.blender.org/docs/handbook/bug_reports/making_good_bug_reports/collect_system_information/"
    )

    os_type = platform.system()
    script_directory = Path(__file__).parent.resolve()
    if os_type == "Darwin":  # macOS appears as Darwin.
        blender_bin = script_directory.joinpath("../../../../../../MacOS/Blender")
    elif os_type == "Windows":
        blender_bin = script_directory.joinpath("../../../../../Blender.exe")
    else:  # Linux and other Unix systems.
        blender_bin = script_directory.joinpath("../../../../../blender")

    try:
        blender_output = subprocess.run(
            (blender_bin, "--version"),
            stdout=subprocess.PIPE,
            encoding="utf-8",
            errors="surrogateescape",
        )
    except Exception as ex:
        sys.stderr.write("{:s}\n".format(str(ex)))
        return ""

    text = blender_output.stdout

    unknown_string = "<unknown>"

    def re_group_or_unknown(m: re.Match[str] | None) -> str:
        if m is None:
            return unknown_string
        return m.group(1)

    # Gather Blender version information.
    values: dict[str, str] = {
        "version": re_group_or_unknown(re.search(r"^Blender (.*)", text, flags=re.MULTILINE)),
        "branch": re_group_or_unknown(re.search(r"^\s+build branch: (.*)", text, flags=re.MULTILINE)),
        "commit_date": re_group_or_unknown(re.search(r"^\s+build commit date: (.*)", text, flags=re.MULTILINE)),
        "commit_time": re_group_or_unknown(re.search(r"^\s+build commit time: (.*)", text, flags=re.MULTILINE)),
        "build_hash": re_group_or_unknown(re.search(r"^\s+build hash: (.*)", text, flags=re.MULTILINE)),
    }

    if not (set(values.values()) - {unknown_string}):
        # No valid Blender info could be found.
        print("Blender did not provide any build information. Blender may be corrupt or blocked from running.")
        print("Please try reinstalling Blender and double check your anti-virus isn't blocking it from running.")
        return ""

    query_params["broken_version"] = (
        "{version:s}, branch: {branch:s}, commit date: {commit_date:s} {commit_time:s}, hash `{build_hash:s}`".format(
            **values,
        )
    )

    return _glassmesh_issue_url(query_params)


def _glassmesh_issue_url(query_params):
    # GlassMesh: problems are reported to the GlassMesh project, not to Blender's bug tracker
    # (GlassMesh is an unofficial fork, its bugs must not end up in Blender's tracker).
    import urllib.parse
    body = (
        "**System Information**\n"
        "Operating system: {:s}\n"
        "Graphics card: {:s}\n"
        "\n"
        "**GlassMesh Version**\n"
        "Broken: {:s}\n"
        "\n"
        "**Short description of the problem**\n"
        "\n"
        "**Exact steps for others to reproduce the problem**\n"
    ).format(query_params.get("os", ""), query_params.get("gpu", ""), query_params.get("broken_version", ""))
    return "https://github.com/omranabdulaziz/GlassMesh/issues/new?" + urllib.parse.urlencode({"body": body})


def main() -> int:
    import webbrowser

    if not (url := url_from_blender()):
        return 1

    webbrowser.open(url)
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
