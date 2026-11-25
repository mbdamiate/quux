#!/bin/sh
# Installs the versioned git hooks of tools/git-hooks (D-53) for this clone.
set -eu
repository_root=$(cd "$(dirname "$0")/.." && pwd)
git -C "$repository_root" config core.hooksPath tools/git-hooks
chmod +x "$repository_root"/tools/git-hooks/*
echo "git hooks installed: core.hooksPath = tools/git-hooks"
