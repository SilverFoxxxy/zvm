#!/usr/bin/env bash
# Генерирует тест-кейсы. Запуск: bash tests/gen_cases.sh
set -euo pipefail

cd "$(dirname "$0")"
CASES="cases"

write() {
    local path="$CASES/$1"
    mkdir -p "$(dirname "$path")"
    cat > "$path"
}

empty() {
    local path="$CASES/$1"
    mkdir -p "$(dirname "$path")"
    : > "$path"
}

# --------------------------------------------------------------- 01
write 01_read_write/program.zasm <<'EOF'
READ -> R0
READ -> R1
WRITE R0
WRITE R1
WRITE NL
EOF
write 01_read_write/01.in <<'EOF'
10 20
EOF
write 01_read_write/01.out <<'EOF'
10 20
EOF
write 01_read_write/02.in <<'EOF'
-5 5
EOF
write 01_read_write/02.out <<'EOF'
-5 5
EOF
write 01_read_write/03.in <<'EOF'
0 0
EOF
write 01_read_write/03.out <<'EOF'
0 0
EOF

# --------------------------------------------------------------- 02
write 02_write_nl_formatting/program.zasm <<'EOF'
READ -> R0
READ -> R1
READ -> R2
WRITE R0
WRITE NL
WRITE R1
WRITE R2
WRITE NL
EOF
write 02_write_nl_formatting/01.in <<'EOF'
1 2 3
EOF
write 02_write_nl_formatting/01.out <<'EOF'
1
2 3
EOF
write 02_write_nl_formatting/02.in <<'EOF'
10 20 30
EOF
write 02_write_nl_formatting/02.out <<'EOF'
10
20 30
EOF

# --------------------------------------------------------------- 03
write 03_mov/program.zasm <<'EOF'
MOV 42 -> R0
MOV R0 -> R1
MOV -5 -> R2
WRITE R0
WRITE R1
WRITE R2
WRITE NL
EOF
empty 03_mov/01.in
write 03_mov/01.out <<'EOF'
42 42 -5
EOF

# --------------------------------------------------------------- 04
write 04_arith_all/program.zasm <<'EOF'
READ -> R0
READ -> R1

ADD R0, R1 -> R2
SUB R0, R1 -> R3
MUL R0, R1 -> R4
DIV R0, R1 -> R5
MOD R0, R1 -> R6

WRITE R2
WRITE R3
WRITE R4
WRITE R5
WRITE R6
WRITE NL
EOF
write 04_arith_all/01.in <<'EOF'
17 5
EOF
write 04_arith_all/01.out <<'EOF'
22 12 85 3 2
EOF
write 04_arith_all/02.in <<'EOF'
10 3
EOF
write 04_arith_all/02.out <<'EOF'
13 7 30 3 1
EOF
write 04_arith_all/03.in <<'EOF'
100 10
EOF
write 04_arith_all/03.out <<'EOF'
110 90 1000 10 0
EOF
write 04_arith_all/04.in <<'EOF'
-17 5
EOF
write 04_arith_all/04.out <<'EOF'
-12 -22 -85 -3 -2
EOF
write 04_arith_all/05.in <<'EOF'
-7 -3
EOF
write 04_arith_all/05.out <<'EOF'
-10 -4 21 2 -1
EOF

# --------------------------------------------------------------- 05
write 05_compare_all/program.zasm <<'EOF'
READ -> R0
READ -> R1

EQ R0, R1 -> R2
NE R0, R1 -> R3
LT R0, R1 -> R4
GT R0, R1 -> R5
LE R0, R1 -> R6
GE R0, R1 -> R7

WRITE R2
WRITE R3
WRITE R4
WRITE R5
WRITE R6
WRITE R7
WRITE NL
EOF
write 05_compare_all/01.in <<'EOF'
5 5
EOF
write 05_compare_all/01.out <<'EOF'
1 0 0 0 1 1
EOF
write 05_compare_all/02.in <<'EOF'
3 7
EOF
write 05_compare_all/02.out <<'EOF'
0 1 1 0 1 0
EOF
write 05_compare_all/03.in <<'EOF'
7 3
EOF
write 05_compare_all/03.out <<'EOF'
0 1 0 1 0 1
EOF

# --------------------------------------------------------------- 06
write 06_logic_all/program.zasm <<'EOF'
READ -> R0
READ -> R1

AND R0, R1 -> R2
OR  R0, R1 -> R3
XOR R0, R1 -> R4
NOT R0     -> R5

WRITE R2
WRITE R3
WRITE R4
WRITE R5
WRITE NL
EOF
write 06_logic_all/01.in <<'EOF'
1 1
EOF
write 06_logic_all/01.out <<'EOF'
1 1 0 0
EOF
write 06_logic_all/02.in <<'EOF'
1 0
EOF
write 06_logic_all/02.out <<'EOF'
0 1 1 0
EOF
write 06_logic_all/03.in <<'EOF'
0 0
EOF
write 06_logic_all/03.out <<'EOF'
0 0 0 1
EOF
write 06_logic_all/04.in <<'EOF'
5 7
EOF
write 06_logic_all/04.out <<'EOF'
1 1 0 0
EOF

# --------------------------------------------------------------- 07
write 07_const_operands/program.zasm <<'EOF'
READ -> R0
ADD R0, 100 -> R1
SUB R1, 1   -> R2
MUL R2, 2   -> R3

WRITE R0
WRITE R1
WRITE R2
WRITE R3
WRITE NL
EOF
write 07_const_operands/01.in <<'EOF'
5
EOF
write 07_const_operands/01.out <<'EOF'
5 105 104 208
EOF
write 07_const_operands/02.in <<'EOF'
0
EOF
write 07_const_operands/02.out <<'EOF'
0 100 99 198
EOF

# --------------------------------------------------------------- 08
write 08_jump/program.zasm <<'EOF'
READ -> R0
JUMP SKIP
MOV 999 -> R1
MARK SKIP
WRITE R0
WRITE R1
WRITE NL
EOF
write 08_jump/01.in <<'EOF'
7
EOF
write 08_jump/01.out <<'EOF'
7 0
EOF

# --------------------------------------------------------------- 09
write 09_jumpif/program.zasm <<'EOF'
READ -> R0
GT R0, 0 -> R1
JUMPIF R1, POS
MOV 0 -> R2
WRITE R2
WRITE NL
JUMP END
MARK POS
MOV 1 -> R2
WRITE R2
WRITE NL
MARK END
EOF
write 09_jumpif/01.in <<'EOF'
5
EOF
write 09_jumpif/01.out <<'EOF'
1
EOF
write 09_jumpif/02.in <<'EOF'
0
EOF
write 09_jumpif/02.out <<'EOF'
0
EOF
write 09_jumpif/03.in <<'EOF'
-3
EOF
write 09_jumpif/03.out <<'EOF'
0
EOF

# --------------------------------------------------------------- 10
write 10_loop_countdown/program.zasm <<'EOF'
READ -> R0

MARK LOOP
LE R0, 0 -> R1
JUMPIF R1, END
WRITE R0
SUB R0, 1 -> R0
JUMP LOOP

MARK END
WRITE NL
EOF
write 10_loop_countdown/01.in <<'EOF'
3
EOF
write 10_loop_countdown/01.out <<'EOF'
3 2 1
EOF
write 10_loop_countdown/02.in <<'EOF'
1
EOF
write 10_loop_countdown/02.out <<'EOF'
1
EOF
write 10_loop_countdown/03.in <<'EOF'
0
EOF
empty 10_loop_countdown/03.out

# --------------------------------------------------------------- 11
write 11_loop_nested/program.zasm <<'EOF'
READ -> R0

MOV 1 -> R1
MARK OUTER
GT R1, R0 -> R2
JUMPIF R2, END

MOV 0 -> R3
MARK INNER
GE R3, R1 -> R4
JUMPIF R4, NEXT_ROW

WRITE R1
ADD R3, 1 -> R3
JUMP INNER

MARK NEXT_ROW
WRITE NL
ADD R1, 1 -> R1
JUMP OUTER

MARK END
EOF
write 11_loop_nested/01.in <<'EOF'
0
EOF
empty 11_loop_nested/01.out
write 11_loop_nested/02.in <<'EOF'
1
EOF
write 11_loop_nested/02.out <<'EOF'
1
EOF
write 11_loop_nested/03.in <<'EOF'
2
EOF
write 11_loop_nested/03.out <<'EOF'
1
2 2
EOF
write 11_loop_nested/04.in <<'EOF'
3
EOF
write 11_loop_nested/04.out <<'EOF'
1
2 2
3 3 3
EOF

# --------------------------------------------------------------- 12
write 12_error_div_zero/program.zasm <<'EOF'
READ -> R0
READ -> R1
DIV R0, R1 -> R2
WRITE R2
WRITE NL
EOF
write 12_error_div_zero/01.in <<'EOF'
5 0
EOF
empty 12_error_div_zero/01.expect_failure
write 12_error_div_zero/02.in <<'EOF'
0 0
EOF
empty 12_error_div_zero/02.expect_failure

# --------------------------------------------------------------- 13
write 13_error_unknown_label/program.zasm <<'EOF'
JUMP NOWHERE
EOF
empty 13_error_unknown_label/01.in
empty 13_error_unknown_label/01.expect_failure

# --------------------------------------------------------------- 14
write 14_error_unknown_instruction/program.zasm <<'EOF'
FOO R0
EOF
empty 14_error_unknown_instruction/01.in
empty 14_error_unknown_instruction/01.expect_failure

# --------------------------------------------------------------- 15
write 15_error_infinite_loop/program.zasm <<'EOF'
MARK LOOP
JUMP LOOP
EOF
empty 15_error_infinite_loop/01.in
empty 15_error_infinite_loop/01.expect_failure

# --------------------------------------------------------------- 16
write 16_overflow/program.zasm <<'EOF'
READ -> R0
ADD R0, 1 -> R1
WRITE R0
WRITE R1
WRITE NL
EOF
write 16_overflow/01.in <<'EOF'
9223372036854775807
EOF
write 16_overflow/01.out <<'EOF'
9223372036854775807 -9223372036854775808
EOF

echo "Готово. Создано:"
ls -1 "$CASES" | sed 's/^/  /'