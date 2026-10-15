#!/usr/bin/env bash
# Run from a normal terminal with GitHub access and a writable .git directory.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

repository="${1:-xi-void/es2clone}"
if [[ ! "$repository" =~ ^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$ ]]; then
    printf 'Expected a repository in owner/name form.\n' >&2
    exit 1
fi

gh auth status --hostname github.com
gh api user --jq '"Authenticated as " + .login'

if project_root="$(git rev-parse --show-toplevel 2>/dev/null)"; then
    if [[ "$project_root" != "$PWD" ]]; then
        printf 'This directory belongs to a parent repository; stopping.\n' >&2
        exit 1
    fi
else
    git init -b main
fi

branch="$(git symbolic-ref --quiet --short HEAD)"
remote_url="$(git remote get-url origin 2>/dev/null || true)"
if [[ -n "$remote_url" && "$remote_url" != "https://github.com/$repository.git" && "$remote_url" != "https://github.com/$repository" && "$remote_url" != "git@github.com:$repository.git" ]]; then
    printf 'Existing origin points elsewhere: %s\n' "$remote_url" >&2
    exit 1
fi
if ! git diff --cached --quiet; then
    printf 'There are already staged changes. Commit or unstage them before publishing.\n' >&2
    exit 1
fi

git add -- .gitignore CMakeLists.txt README.md run.sh package.sh publish-github.sh src tests assets docs
if ! git diff --cached --quiet; then
    git commit -m "Save playable native Linux Earthsiege prototype"
fi

if ! gh repo view "$repository" --json nameWithOwner >/dev/null 2>&1; then
    gh repo create "$repository" --public \
        --description "Native Linux mech combat inspired by Earthsiege 2, built with C++, SDL2, and OpenGL."
fi
if [[ -z "$remote_url" ]]; then
    git remote add origin "https://github.com/$repository.git"
fi
git -c credential.helper= -c 'credential.helper=!gh auth git-credential' \
    push --set-upstream origin "$branch"
gh repo view "$repository" --json url --jq .url
