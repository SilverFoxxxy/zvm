// zvm.cpp
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------- Инструкции ----------------------

enum class Op : uint8_t {
  READ,
  WRITE,
  MOV,
  ADD,
  SUB,
  MUL,
  DIV,
  MOD,
};

// Источник: либо регистр, либо число-литерал
struct Operand {
  bool is_const = false;
  int64_t value = 0;  // если is_const — само число, иначе индекс регистра
};

struct Instr {
  Op op;
  Operand a, b, c;  // для READ/WRITE используется только a
  int line = 0;
};

// ---------------------- Разбор имён ----------------------

static Op op_from_name(const std::string& s) {
  if (s == "READ") return Op::READ;
  if (s == "WRITE") return Op::WRITE;
  if (s == "MOV") return Op::MOV;
  if (s == "ADD") return Op::ADD;
  if (s == "SUB") return Op::SUB;
  if (s == "MUL") return Op::MUL;
  if (s == "DIV") return Op::DIV;
  if (s == "MOD") return Op::MOD;
  throw std::runtime_error("Неизвестная инструкция: " + s);
}

// Токен — либо R0..R15, либо целое число. Регистр обязан быть приёмником.
static Operand parse_operand(const std::string& s) {
  Operand r;
  if (!s.empty() && (s[0] == 'R' || s[0] == 'r')) {
    if (s.size() < 2) throw std::runtime_error("Пустое имя регистра: " + s);
    int n = 0;
    for (size_t i = 1; i < s.size(); ++i) {
      if (!std::isdigit((unsigned char)s[i]))
        throw std::runtime_error("Ожидался R0..R15: " + s);
      n = n * 10 + (s[i] - '0');
    }
    if (n < 0 || n > 15)
      throw std::runtime_error("Регистр вне диапазона R0..R15: " + s);
    r.is_const = false;
    r.value = n;
  } else {
    // число со знаком
    size_t i = 0;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
    if (i == s.size()) throw std::runtime_error("Плохой операнд: " + s);
    for (; i < s.size(); ++i)
      if (!std::isdigit((unsigned char)s[i]))
        throw std::runtime_error("Плохой операнд: " + s);
    r.is_const = true;
    r.value = std::stoll(s);
  }
  return r;
}

static int parse_reg(const std::string& s) {
  Operand o = parse_operand(s);
  if (o.is_const)
    throw std::runtime_error("Тут нужен регистр, а не число: " + s);
  return (int)o.value;
}

// "ADD R1, R2 -> R3"  =>  "ADD R1 R2 -> R3"
static std::string normalize(const std::string& line) {
  std::string s;
  s.reserve(line.size() + 8);
  for (size_t i = 0; i < line.size();) {
    char ch = line[i];
    if (ch == '-' && i + 1 < line.size() && line[i + 1] == '>') {
      s += " -> ";
      i += 2;
    } else if (ch == ',') {
      s += ' ';
      ++i;
    } else {
      s += ch;
      ++i;
    }
  }
  return s;
}

// ---------------------- Интерпретатор ----------------------

class Interp {
  std::vector<Instr> prog;
  int64_t regs[16] = {};
  size_t steps = 0;
  size_t max_steps;

  int64_t src(const Operand& o) const {
    return o.is_const ? o.value : regs[o.value];
  }

 public:
  explicit Interp(size_t limit = 10'000'000) : max_steps(limit) {}

  void load(std::istream& in) {
    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
      ++lineno;
      auto sc = line.find(';');
      if (sc != std::string::npos) line.resize(sc);

      std::istringstream ss(normalize(line));
      std::vector<std::string> t;
      std::string tok;
      while (ss >> tok) t.push_back(tok);
      if (t.empty()) continue;

      Instr ins;
      ins.line = lineno;
      ins.op = op_from_name(t[0]);

      auto err = [&](const std::string& what) {
        throw std::runtime_error("строка " + std::to_string(lineno) + ": " +
                                 what);
      };

      switch (ins.op) {
        case Op::READ:
          if (t.size() != 3 || t[1] != "->") err("ожидалось: READ -> Rx");
          ins.a = parse_operand(t[2]);
          if (ins.a.is_const) err("READ требует регистр");
          break;

        case Op::WRITE:
          if (t.size() != 2) err("ожидалось: WRITE Rx");
          ins.a = parse_operand(t[1]);
          if (ins.a.is_const) err("WRITE требует регистр");
          break;

        case Op::MOV:
          if (t.size() != 4 || t[2] != "->") err("ожидалось: MOV Rx -> Ry");
          ins.a = parse_operand(t[1]);
          ins.c = parse_operand(t[3]);
          if (ins.c.is_const) err("MOV требует регистр-приёмник");
          break;

        case Op::ADD:
        case Op::SUB:
        case Op::MUL:
        case Op::DIV:
        case Op::MOD:
          if (t.size() != 5 || t[3] != "->") err("ожидалось: OP Rx, Ry -> Rz");
          ins.a = parse_operand(t[1]);
          ins.b = parse_operand(t[2]);
          ins.c = parse_operand(t[4]);
          if (ins.c.is_const) err("Приёмник арифметики — только регистр");
          break;
      }
      prog.push_back(ins);
    }
  }

  void run() {
    for (size_t ip = 0; ip < prog.size(); ++ip) {
      if (++steps > max_steps) throw std::runtime_error("Превышен лимит шагов");

      const Instr& ins = prog[ip];
      switch (ins.op) {
        case Op::READ:
          if (!(std::cin >> regs[ins.a.value]))
            throw std::runtime_error("строка " + std::to_string(ins.line) +
                                     ": не удалось прочитать число");
          break;
        case Op::WRITE:
          std::cout << regs[ins.a.value] << '\n';
          break;
        case Op::MOV:
          regs[ins.c.value] = src(ins.a);
          break;
        case Op::ADD:
          regs[ins.c.value] = src(ins.a) + src(ins.b);
          break;
        case Op::SUB:
          regs[ins.c.value] = src(ins.a) - src(ins.b);
          break;
        case Op::MUL:
          regs[ins.c.value] = src(ins.a) * src(ins.b);
          break;
        case Op::DIV: {
          int64_t d = src(ins.b);
          if (d == 0)
            throw std::runtime_error("строка " + std::to_string(ins.line) +
                                     ": деление на ноль");
          regs[ins.c.value] = src(ins.a) / d;
          break;
        }
        case Op::MOD: {
          int64_t d = src(ins.b);
          if (d == 0)
            throw std::runtime_error("строка " + std::to_string(ins.line) +
                                     ": остаток по нулю");
          regs[ins.c.value] = src(ins.a) % d;
          break;
        }
      }
    }
  }
};

// ---------------------- main ----------------------

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Использование: " << argv[0] << " program.zasm\n";
    return 1;
  }
  try {
    std::ifstream f(argv[1]);
    if (!f) {
      std::cerr << "Не могу открыть " << argv[1] << '\n';
      return 1;
    }
    Interp vm;
    vm.load(f);
    vm.run();
  } catch (const std::exception& e) {
    std::cerr << "Ошибка: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
