#!/usr/bin/env bash
#
# Install git hooks for ABDMS2000 development.
#
# This sets core.hooksPath to .githooks so git uses the hooks in this repo.
#
# Usage:  ./githooks-install.sh   (or:  bash .githooks/install.sh)
#

cd "$(dirname "$0")/.." || exit 1

git config core.hooksPath .githooks
echo "✅ Git hooks installed (.githooks/)"
echo ""
echo "To uninstall:  git config --unset core.hooksPath"
