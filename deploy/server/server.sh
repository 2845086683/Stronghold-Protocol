#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if ! command -v python3 >/dev/null 2>&1; then
    printf '%s\n' 'Python 3 is needed for the deployment controller. Install python3 from your signed distro repository; see README.md.' >&2
    exit 1
fi
exec python3 "$script_dir/compose.py" "$@"
