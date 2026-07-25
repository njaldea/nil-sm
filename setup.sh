#!/usr/bin/env bash
set -euo pipefail

if [[ $0 != $BASH_SOURCE ]]; then
    echo "do not source this script"
    return 1
fi

if [ $# -ne 1 ]; then
    echo "Usage: $0 <new-project-name>"
    exit 1
fi

NEW_NAME="$1"
OLD_NAME="nil-template"

if [ "$NEW_NAME" = "$OLD_NAME" ]; then
    echo "New name is the same as the old name. Nothing to do."
    exit 0
fi

# Rename in CMakeLists.txt
sed -i "s/^    ${OLD_NAME} CXX$/    ${NEW_NAME} CXX/" CMakeLists.txt

# Rename in vcpkg.json
sed -i "s/\"name\": \"${OLD_NAME}\"/\"name\": \"${NEW_NAME}\"/" vcpkg.json

echo "Renamed '${OLD_NAME}' to '${NEW_NAME}' in CMakeLists.txt and vcpkg.json"

# Update nil-vcpkg-ports baseline
REPO="njaldea/nil-vcpkg-ports"
REPO_URL="https://github.com/${REPO}"

NEW_BASELINE=$(curl -fsSL "https://api.github.com/repos/${REPO}/commits/HEAD" | grep -m1 '"sha"' | cut -d'"' -f4)

if [ -z "$NEW_BASELINE" ]; then
    echo "Error: failed to fetch latest commit from ${REPO_URL}" >&2
    exit 1
fi

jq --arg url "$REPO_URL" --arg baseline "$NEW_BASELINE" \
    '(.registries[] | select(.repository == $url) | .baseline) = $baseline' \
    vcpkg-configuration.json > vcpkg-configuration.tmp.json \
    && mv vcpkg-configuration.tmp.json vcpkg-configuration.json

echo "Updated '${REPO_URL}' baseline to '${NEW_BASELINE}'"

rm -- "$0"
