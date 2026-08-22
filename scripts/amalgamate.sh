#!/usr/bin/env bash

if [[ $0 != $BASH_SOURCE ]]; then
    echo "do not source this script"
    return 1
fi

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$BASH_SOURCE")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [[ $# -ne 0 ]]; then
    echo "error: this script does not accept arguments"
    echo "usage: $(basename "$BASH_SOURCE")"
    exit 1
fi

run_amalgamate()
{
    local input="$1"
    local output="$2"
    local -a HEADER_INCLUDES=()
    local -a SKIP_FILES=()

    case "$input" in
        */sm/id.hpp)
            HEADER_INCLUDES=("../sm.hpp")
            SKIP_FILES=("$REPO_ROOT/src/publish/nil/sm/structs.hpp")
            ;;
        */sm/uml.hpp)
            HEADER_INCLUDES=("../sm.hpp" "id.hpp")
            SKIP_FILES=(
                "$REPO_ROOT/src/publish/nil/sm/state.hpp"
                "$REPO_ROOT/src/publish/nil/sm/detail.hpp"
                "$REPO_ROOT/src/publish/nil/sm/structs.hpp"
                "$REPO_ROOT/src/publish/nil/sm/id.hpp"
            )
            ;;
    esac

    declare -A SEEN=()
    declare -A EXTERNAL_SEEN=()
    local -a EXTERNAL_INCLUDES=()

# Simplified STL detection: one-word include with no extension (e.g. <vector>, <string>). 
    is_stl_like()
    {
        local include_path="$1"
        [[ "$include_path" =~ ^[[:alnum:]_]+$ ]]
    }

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
                        EXTERNAL_INCLUDES+=("$include_path")
                    fi
                    continue
                fi

                local resolved
                if ! resolved="$(resolve_include "$include_path" "$dir")"; then
                    echo "error: could not resolve local include '$include_path' from '$file'" >&2
                    exit 1
                fi

                local skip_file
                for skip_file in "${SKIP_FILES[@]}"; do
                    if [[ "$resolved" == "$(realpath "$skip_file")" ]]; then
                        continue 2
                    fi
                done

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

    local body_file
    body_file="$(mktemp)"

    process_file "$(realpath "$input")" > "$body_file"

    local trimmed_body_file
    trimmed_body_file="$(mktemp)"
    awk '/[^[:space:]]/ { started = 1 } started { print }' "$body_file" > "$trimmed_body_file"
    mv "$trimmed_body_file" "$body_file"

    local normalized_body_file
    normalized_body_file="$(mktemp)"
    awk '
        /^[[:space:]]*$/ {
            if (!blank) {
                print
            }
            blank = 1
            next
        }
        {
            blank = 0
            print
        }
    ' "$body_file" > "$normalized_body_file"
    mv "$normalized_body_file" "$body_file"

    local -a THIRD_PARTY_INCLUDES=()
    local -a STL_INCLUDES=()
    local include_path
    for include_path in "${EXTERNAL_INCLUDES[@]}"; do
        if is_stl_like "$include_path"; then
            STL_INCLUDES+=("$include_path")
        else
            THIRD_PARTY_INCLUDES+=("$include_path")
        fi
    done

    local -a SORTED_THIRD_PARTY=()
    if [[ ${#THIRD_PARTY_INCLUDES[@]} -gt 0 ]]; then
        mapfile -t SORTED_THIRD_PARTY < <(printf '%s\n' "${THIRD_PARTY_INCLUDES[@]}" | sort)
    fi

    local -a SORTED_STL=()
    if [[ ${#STL_INCLUDES[@]} -gt 0 ]]; then
        mapfile -t SORTED_STL < <(printf '%s\n' "${STL_INCLUDES[@]}" | sort)
    fi

    {
        echo "#pragma once"
        echo

        for include_path in "${HEADER_INCLUDES[@]}"; do
            printf '#include "%s"\n' "$include_path"
        done
        if [[ ${#HEADER_INCLUDES[@]} -gt 0 ]]; then
            echo
        fi

        for include_path in "${SORTED_THIRD_PARTY[@]}"; do
            printf '#include <%s>\n' "$include_path"
        done
        if [[ ${#SORTED_THIRD_PARTY[@]} -gt 0 ]]; then
            echo
        fi

        for include_path in "${SORTED_STL[@]}"; do
            printf '#include <%s>\n' "$include_path"
        done
        if [[ ${#SORTED_STL[@]} -gt 0 ]]; then
            echo
        fi

        cat "$body_file"
    } > "${output:-/dev/stdout}"

    rm -f "$body_file"
}

GEN_DIR="$SCRIPT_DIR/gen"
GEN_SM_DIR="$GEN_DIR/sm"

mkdir -p "$GEN_SM_DIR"

run_amalgamate "$REPO_ROOT/src/publish/nil/sm.hpp" "$GEN_DIR/sm.hpp"
run_amalgamate "$REPO_ROOT/src/publish/nil/sm/id.hpp" "$GEN_SM_DIR/id.hpp"
run_amalgamate "$REPO_ROOT/src/publish/nil/sm/uml.hpp" "$GEN_SM_DIR/uml.hpp"

echo "wrote: $GEN_DIR/sm.hpp"
echo "wrote: $GEN_SM_DIR/id.hpp"
echo "wrote: $GEN_SM_DIR/uml.hpp"
