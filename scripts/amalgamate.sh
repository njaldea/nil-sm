#!/usr/bin/env bash

if [[ $0 != $BASH_SOURCE ]]; then
    echo "do not source this script"
    return 1
fi

set -euo pipefail

INPUT=""
OUTPUT=""
INCLUDE_DIRS=()

HELP()
{
    echo "usage: $(basename "$BASH_SOURCE") -i INPUT [-o OUTPUT] [-I INCLUDE_DIR ...]"
    echo
    echo "Amalgamates INPUT header by recursively inlining every quote-form"
    echo "#include \"...\" (local project headers), resolved relative to the"
    echo "directory of the file containing it. Angle-form #include <...>"
    echo "(STL / third-party) is left alone: deduplicated and hoisted to the"
    echo "top of the output instead of being left inline."
    echo
    echo "options:"
    echo "  -i INPUT         header file to amalgamate (required)"
    echo "  -o OUTPUT        output file (default: stdout)"
    echo "  -I INCLUDE_DIR   extra directory to search if a quote-include is"
    echo "                   not found relative to its including file, may be"
    echo "                   repeated"
    echo "  -h               print this help"
    echo
    echo "example:"
    echo "  $(basename "$BASH_SOURCE") -i src/publish/nil/sm.hpp -o sm.hpp"
}

while getopts ":hi:o:I:" option; do
    case $option in
        h)
            HELP
            exit 0;;
        i)
            INPUT="$OPTARG";;
        o)
            OUTPUT="$OPTARG";;
        I)
            INCLUDE_DIRS+=("$OPTARG");;
        \?)
            echo "unknown option is provided: -$OPTARG"
            HELP
            exit 1;;
        :)
            echo "option -$OPTARG requires an argument"
            HELP
            exit 1;;
    esac
done
shift $((OPTIND - 1))

if [[ -z "$INPUT" ]]; then
    echo "error: -i INPUT is required"
    HELP
    exit 1
fi

declare -A SEEN
declare -A EXTERNAL_SEEN
EXTERNAL_INCLUDES=()

# resolve a quote-form include path relative to DIR_HINT (the directory of the file
# containing the #include), falling back to INCLUDE_DIRS. Prints the resolved absolute
# path on success.
resolve_include()
{
    local include_path="$1"
    local dir_hint="$2"

    if [[ -f "${dir_hint}/${include_path}" ]]; then
        realpath "${dir_hint}/${include_path}"
        return 0
    fi

    local dir
    for dir in "${INCLUDE_DIRS[@]}"; do
        if [[ -f "${dir}/${include_path}" ]]; then
            realpath "${dir}/${include_path}"
            return 0
        fi
    done

    return 1
}

process_file()
{
    local file="$1"
    local dir
    dir="$(dirname "$file")"

    local pragma_once_re='^[[:space:]]*#pragma[[:space:]]+once'
    local include_re='^[[:space:]]*#include[[:space:]]*(["<])([^">]+)[">]'

    local line
    while IFS='' read -r line || [[ -n "$line" ]]; do
        if [[ "$line" =~ $pragma_once_re ]]; then
            continue
        fi

        if [[ "$line" =~ $include_re ]]; then
            local delim="${BASH_REMATCH[1]}"
            local include_path="${BASH_REMATCH[2]}"

            if [[ "$delim" != '"' ]]; then
                if [[ -z "${EXTERNAL_SEEN[$include_path]+x}" ]]; then
                    EXTERNAL_SEEN["$include_path"]=1
                    EXTERNAL_INCLUDES+=("$line")
                fi
                continue
            fi

            local resolved
            if ! resolved="$(resolve_include "$include_path" "$dir")"; then
                echo "error: could not resolve local include '$include_path' from '$file'" >&2
                exit 1
            fi

            if [[ -n "${SEEN[$resolved]+x}" ]]; then
                continue
            fi
            SEEN["$resolved"]=1

            process_file "$resolved"
            continue
        fi

        echo "$line"
    done < "$file"
}

BODY_FILE="$(mktemp)"
trap 'rm -f "$BODY_FILE"' EXIT

process_file "$(realpath "$INPUT")" > "$BODY_FILE"

{
    for line in "${EXTERNAL_INCLUDES[@]}"; do
        printf '%s\n' "$line"
    done
    if [[ ${#EXTERNAL_INCLUDES[@]} -gt 0 ]]; then
        echo
    fi
    cat "$BODY_FILE"
} > "${OUTPUT:-/dev/stdout}"
