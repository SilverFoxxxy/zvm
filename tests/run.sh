#!/usr/bin/env bash
set -u

# Цвета только если stdout — терминал
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
    GREEN=$(tput setaf 2)
    RED=$(tput setaf 1)
    YELLOW=$(tput setaf 3)
    BOLD=$(tput bold)
    RESET=$(tput sgr0)
else
    GREEN="" RED="" YELLOW="" BOLD="" RESET=""
fi

cd "$(dirname "$0")/.."

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17 -O2 -Wall -Wextra -fwrapv}"
SRC="zvm.cpp"
ZVM="./zvm"
CASES_DIR="tests/cases"
FILTER="${1:-}"

echo "Сборка: $CXX $CXXFLAGS $SRC"
if ! "$CXX" $CXXFLAGS -o "$ZVM" "$SRC"; then
    echo
    echo "Сборка провалилась — тесты не запускаются."
    exit 1
fi
echo

total_pass=0
total_fail=0
failed_cases=()

for case_dir in "$CASES_DIR"/*/; do
    name=$(basename "$case_dir")

    if [ -n "$FILTER" ] && [ "$name" != "$FILTER" ]; then
        continue
    fi

    prog="$case_dir/program.zasm"
    if [ ! -f "$prog" ]; then
        echo "SKIP $name (нет program.zvm)"
        continue
    fi

    inputs=()
    while IFS= read -r line; do
        inputs+=("$line")
    done < <(find "$case_dir" -maxdepth 1 -name '*.in' | sort)

    if [ "${#inputs[@]}" -eq 0 ]; then
        echo "SKIP $name (нет ни одного .in)"
        continue
    fi

    case_pass=0
    case_fail=0
    case_fail_details=()

    for inp in "${inputs[@]}"; do
        base="${inp%.in}"
        tag="$(basename "$base")"

        tmp_out=$(mktemp)
        tmp_err=$(mktemp)

        "$ZVM" "$prog" < "$inp" > "$tmp_out" 2> "$tmp_err"
        rc=$?

        actual=$(cat "$tmp_out")
        rm -f "$tmp_out" "$tmp_err"

        if [ -f "${base}.expect_failure" ]; then
            if [ $rc -ne 0 ]; then
                case_pass=$((case_pass + 1))
            else
                case_fail=$((case_fail + 1))
                case_fail_details+=("$tag: ожидалась ошибка, но код возврата 0")
            fi
            continue
        fi

        if [ ! -f "${base}.out" ]; then
            case_fail_details+=("$tag: нет ни .out, ни .expect_failure (пропущен)")
            continue
        fi

        if [ $rc -ne 0 ]; then
            case_fail=$((case_fail + 1))
            case_fail_details+=("$tag: код возврата $rc, ожидался 0")
            continue
        fi

        expected=$(cat "${base}.out")

        if [ "$actual" = "$expected" ]; then
            case_pass=$((case_pass + 1))
        else
            case_fail=$((case_fail + 1))
            case_fail_details+=("$tag: ожидалось «$expected», получено «$actual»")
        fi
    done

    total=$((case_pass + case_fail))
    total_pass=$((total_pass + case_pass))
    total_fail=$((total_fail + case_fail))

    if [ $case_fail -eq 0 ]; then
        echo "${GREEN}PASS${RESET} $name ($case_pass/$total)"
    else
        echo "${RED}FAIL${RESET} $name ($case_pass/$total)"
        for d in "${case_fail_details[@]}"; do
            echo "    ${YELLOW}$d${RESET}"
        done
        failed_cases+=("$name")
    fi
done

echo
echo "Тестов: $total_pass пройдено, $total_fail провалено"

if [ $total_fail -gt 0 ]; then
    echo "${RED}Проваленные кейсы:${RESET}"
    for n in "${failed_cases[@]}"; do
        echo "  - $n"
    done
    exit 1
fi