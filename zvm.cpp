#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum class Op : uint8_t {
  READ,
  WRITE,
  MOV,
  ADD,
  SUB,
  MUL,
  DIV,
  MOD,
  EQ,
  NE,
  LT,
  GT,
  LE,
  GE,
  JUMP,
  JUMPIF,
  MARK
};

struct Operand {
  bool is_const = false;
  int reg = -1;
  int64_t val = 0;
};

struct Instr {
  Op op;
  Operand a, b;
  int c = -1;
  bool newline = false;  // WRITE NL
  std::string label;
  int target = -1;
  int line = 0;
};

static Op op_from_name(const std::string& s) {
  if (s == "READ") return Op::READ;
  if (s == "WRITE") return Op::WRITE;
  if (s == "MOV") return Op::MOV;
  if (s == "ADD") return Op::ADD;
  if (s == "SUB") return Op::SUB;
  if (s == "MUL") return Op::MUL;
  if (s == "DIV") return Op::DIV;
  if (s == "MOD") return Op::MOD;
  if (s == "EQ") return Op::EQ;
  if (s == "NE") return Op::NE;
  if (s == "LT") return Op::LT;
  if (s == "GT") return Op::GT;
  if (s == "LE") return Op::LE;
  if (s == "GE") return Op::GE;
  if (s == "JUMP") return Op::JUMP;
  if (s == "JUMPIF") return Op::JUMPIF;
  if (s == "MARK") return Op::MARK;
  throw std::runtime_error("Неизвестная инструкция: " + s);
}

static int reg_from_name(const std::string& s) {
  if (s.size() < 2 || s[0] != 'R')
    throw std::runtime_error("Ожидался регистр вида R0..R15, а не: " + s);
  int n = 0;
  for (size_t i = 1; i < s.size(); ++i) {
    if (!std::isdigit((unsigned char)s[i]))
      throw std::runtime_error("Ожидался регистр вида R0..R15, а не: " + s);
    n = n * 10 + (s[i] - '0');
  }
  if (n < 0 || n > 15)
    throw std::runtime_error("Регистр вне диапазона R0..R15: " + s);
  return n;
}

static Operand parse_operand(const std::string& s) {
  Operand o;
  if (!s.empty() && (s[0] == 'R' || s[0] == 'r')) {
    o.is_const = false;
    o.reg = reg_from_name(s);
  } else {
    o.is_const = true;
    try {
      o.val = std::stoll(s);
    } catch (...) {
      throw std::runtime_error("Не регистр и не число: " + s);
    }
  }
  return o;
}

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

class Interp {
  std::vector<Instr> prog;
  int64_t regs[16] = {};
  size_t steps = 0;
  size_t max_steps;
  bool line_has_output = false;

  int64_t value_of(const Operand& o) const {
    return o.is_const ? o.val : regs[o.reg];
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
          ins.c = reg_from_name(t[2]);
          break;

        case Op::WRITE:
          if (t.size() != 2) err("ожидалось: WRITE Rx или WRITE NL");
          if (t[1] == "NL") {
            ins.newline = true;
          } else {
            ins.c = reg_from_name(t[1]);
          }
          break;

        case Op::MOV:
          if (t.size() != 4 || t[2] != "->") err("ожидалось: MOV X -> Ry");
          ins.a = parse_operand(t[1]);
          ins.c = reg_from_name(t[3]);
          break;

        case Op::ADD:
        case Op::SUB:
        case Op::MUL:
        case Op::DIV:
        case Op::MOD:
        case Op::EQ:
        case Op::NE:
        case Op::LT:
        case Op::GT:
        case Op::LE:
        case Op::GE:
          if (t.size() != 5 || t[3] != "->") err("ожидалось: OP X, Y -> Rz");
          ins.a = parse_operand(t[1]);
          ins.b = parse_operand(t[2]);
          ins.c = reg_from_name(t[4]);
          break;

        case Op::MARK:
          if (t.size() != 2) err("ожидалось: MARK label");
          ins.label = t[1];
          break;
        case Op::JUMP:
          if (t.size() != 2) err("ожидалось: JUMP label");
          ins.label = t[1];
          break;
        case Op::JUMPIF:
          if (t.size() != 3) err("ожидалось: JUMPIF Rx, label");
          ins.a = parse_operand(t[1]);
          ins.label = t[2];
          break;
      }
      prog.push_back(ins);
    }

    std::unordered_map<std::string, int> marks;
    for (size_t i = 0; i < prog.size(); ++i) {
      if (prog[i].op == Op::MARK) {
        if (marks.count(prog[i].label))
          throw std::runtime_error("Дублирующаяся метка: " + prog[i].label);
        marks[prog[i].label] = (int)i;
      }
    }
    for (auto& ins : prog) {
      if (ins.op == Op::JUMP || ins.op == Op::JUMPIF) {
        auto it = marks.find(ins.label);
        if (it == marks.end())
          throw std::runtime_error("Неизвестная метка: " + ins.label);
        ins.target = it->second;
      }
    }
  }

  void run() {
    size_t ip = 0;
    while (ip < prog.size()) {
      if (++steps > max_steps)
        throw std::runtime_error(
            "Превышен лимит шагов — похоже на бесконечный цикл");

      const Instr& ins = prog[ip];
      bool jumped = false;

      switch (ins.op) {
        case Op::MARK:
          break;

        case Op::READ:
          if (!(std::cin >> regs[ins.c]))
            throw std::runtime_error("строка " + std::to_string(ins.line) +
                                     ": не удалось прочитать число");
          break;

        case Op::WRITE:
          if (ins.newline) {
            std::cout << '\n';
            line_has_output = false;
          } else {
            if (line_has_output) std::cout << ' ';
            std::cout << regs[ins.c];
            line_has_output = true;
          }
          break;

        case Op::MOV:
          regs[ins.c] = value_of(ins.a);
          break;
        case Op::ADD:
          regs[ins.c] = value_of(ins.a) + value_of(ins.b);
          break;
        case Op::SUB:
          regs[ins.c] = value_of(ins.a) - value_of(ins.b);
          break;
        case Op::MUL:
          regs[ins.c] = value_of(ins.a) * value_of(ins.b);
          break;
        case Op::DIV: {
          int64_t d = value_of(ins.b);
          if (d == 0)
            throw std::runtime_error("строка " + std::to_string(ins.line) +
                                     ": деление на ноль");
          regs[ins.c] = value_of(ins.a) / d;
          break;
        }
        case Op::MOD: {
          int64_t d = value_of(ins.b);
          if (d == 0)
            throw std::runtime_error("строка " + std::to_string(ins.line) +
                                     ": остаток по нулю");
          regs[ins.c] = value_of(ins.a) % d;
          break;
        }

        case Op::EQ:
          regs[ins.c] = (value_of(ins.a) == value_of(ins.b)) ? 1 : 0;
          break;
        case Op::NE:
          regs[ins.c] = (value_of(ins.a) != value_of(ins.b)) ? 1 : 0;
          break;
        case Op::LT:
          regs[ins.c] = (value_of(ins.a) < value_of(ins.b)) ? 1 : 0;
          break;
        case Op::GT:
          regs[ins.c] = (value_of(ins.a) > value_of(ins.b)) ? 1 : 0;
          break;
        case Op::LE:
          regs[ins.c] = (value_of(ins.a) <= value_of(ins.b)) ? 1 : 0;
          break;
        case Op::GE:
          regs[ins.c] = (value_of(ins.a) >= value_of(ins.b)) ? 1 : 0;
          break;

        case Op::JUMP:
          ip = (size_t)ins.target;
          jumped = true;
          break;
        case Op::JUMPIF:
          if (value_of(ins.a) != 0) {
            ip = (size_t)ins.target;
            jumped = true;
          }
          break;
      }
      if (!jumped) ++ip;
    }
  }
};

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Использование: " << argv[0] << " program.asm\n";
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
