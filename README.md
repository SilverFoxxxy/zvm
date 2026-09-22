# ZVM (Zhelezyaka Virtual Machine)

**Железяка**, или **ЖВМ** — Железячная Вычислительная Машина.

```sh
g++ -std=c++17 -O2 -o zvm zvm.cpp
```

```sh
zvm hello.zasm
```

```zh
; пересылка
MOV R1 -> R2

; ввод-вывод
READ -> R1
WRITE R1

; арифметика (регистры)
ADD R1, R2 -> R3
SUB R1, R2 -> R3
MUL R1, R2 -> R3
DIV R1, R2 -> R3
MOD R1, R2 -> R3

; логика (регистры)
AND R1, R2 -> R3
OR  R1, R2 -> R3
NOT R1 -> R2

; сравнения (регистры, дают 0/1)
EQ R1, R2 -> R3       ; R3 = (R1 == R2)
NE R1, R2 -> R3
LT R1, R2 -> R3
GT R1, R2 -> R3
LE R1, R2 -> R3
GE R1, R2 -> R3

; память
LOAD  [R5+100] -> R1
STORE R1 -> [R5+100]

; управление
MARK LOOP
JUMP LOOP
JUMPIF R1, LOOP       ; если R1 != 0
```
