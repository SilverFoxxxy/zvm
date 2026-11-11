#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

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

    AND,
    OR,
    XOR,
    NOT,

    LOAD,
    STORE,
    ALLOC,

    JUMP,
    JUMPIF,
    MARK
};

struct Operand {
    bool is_const = false;
    int reg = -1;
    int64_t val = 0;
};

struct Address {
    Operand base;
    Operand offset;
    bool has_offset = false;
    bool subtract = false;  // true for [X - Y]
};

struct Instr {
    Op op;
    Operand a, b;
    Address addr;
    int c = -1;
    bool newline = false;  // WRITE NL
    std::string label;
    int target = -1;
    int line = 0;
    int col = 0;
};

// ---------------------------------------------------------------- helpers

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

    if (s == "AND") return Op::AND;
    if (s == "OR") return Op::OR;
    if (s == "XOR") return Op::XOR;
    if (s == "NOT") return Op::NOT;

    if (s == "LOAD") return Op::LOAD;
    if (s == "STORE") return Op::STORE;
    if (s == "ALLOC") return Op::ALLOC;

    if (s == "JUMP") return Op::JUMP;
    if (s == "JUMPIF") return Op::JUMPIF;
    if (s == "MARK") return Op::MARK;
    throw std::runtime_error("unknown instruction: " + s);
}

static int reg_from_name(const std::string& s) {
    if (s.size() < 2 || s[0] != 'R')
        throw std::runtime_error("expected register R0..R15, got: " + s);
    int n = 0;
    for (size_t i = 1; i < s.size(); ++i) {
        if (!std::isdigit((unsigned char)s[i]))
            throw std::runtime_error("expected register R0..R15, got: " + s);
        n = n * 10 + (s[i] - '0');
    }
    if (n < 0 || n > 15)
        throw std::runtime_error("register out of range R0..R15: " + s);
    return n;
}

// ASCII-only uppercase. Never touches non-ASCII bytes (e.g. UTF-8 Cyrillic
// in labels/comments), so it is locale- and encoding-independent.
static std::string ascii_upper(const std::string& s) {
    std::string r = s;
    for (char& c : r)
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    return r;
}

// ---------------------------------------------------------------- lexer

struct Token {
    std::string text;
    int col = 0;  // 1-based column of first char in the original line
};

// Splits a single source line into tokens, recording the column of each.
// Handles: whitespace, ';' comments, ',' separators, '->' arrow, '[' ']',
// standalone '+' '-', and everything else as a "word" token.
// Case-insensitive by uppercasing only ASCII letters of each token.
static std::vector<Token> lex_line(const std::string& line) {
    std::vector<Token> out;
    size_t i = 0;
    const size_t n = line.size();

    while (i < n) {
        unsigned char ch = (unsigned char)line[i];

        if (ch == ';') break;                       // comment to end of line
        if (ch == ' ' || ch == '\t' || ch == '\r') { ++i; continue; }

        if (ch == ',') { out.push_back({",", (int)i + 1}); ++i; continue; }
        if (ch == '[') { out.push_back({"[", (int)i + 1}); ++i; continue; }
        if (ch == ']') { out.push_back({"]", (int)i + 1}); ++i; continue; }

        if (ch == '-' && i + 1 < n && line[i + 1] == '>') {
            out.push_back({"->", (int)i + 1});
            i += 2;
            continue;
        }

        if (ch == '+' || ch == '-') {
            out.push_back({std::string(1, (char)ch), (int)i + 1});
            ++i;
            continue;
        }

        // "word" token: run of chars that are not special
        size_t start = i;
        while (i < n) {
            unsigned char c2 = (unsigned char)line[i];
            if (c2 == ' ' || c2 == '\t' || c2 == '\r' || c2 == ';' ||
                c2 == ',' || c2 == '[' || c2 == ']')
                break;
            if (c2 == '-' && i + 1 < n && line[i + 1] == '>') break;
            if (c2 == '+' || c2 == '-') break;
            ++i;
        }
        out.push_back({ascii_upper(line.substr(start, i - start)),
                       (int)start + 1});
    }
    return out;
}

// ---------------------------------------------------------------- parser

class Parser {
   public:
    Parser(std::string file, std::vector<Token> toks, int line_no,
           std::vector<Instr>& prog)
        : file_(std::move(file)),
          toks_(std::move(toks)),
          line_no_(line_no),
          prog_(prog) {}

    void parse_line() {
        if (toks_.empty()) return;
        k_ = 0;

        Op op;
        try {
            op = op_from_name(toks_[0].text);
        } catch (const std::exception&) {
            err(toks_[0].col, "unknown instruction: " + toks_[0].text);
        }

        Instr ins;
        ins.op = op;
        ins.line = line_no_;
        ins.col = toks_[0].col;
        ++k_;

        switch (op) {
            case Op::READ:
                expect_arrow();
                ins.c = reg(take_word());
                expect_end();
                break;

            case Op::WRITE: {
                std::string w = take_word();
                if (w == "NL") {
                    ins.newline = true;
                } else {
                    ins.c = reg(w);
                }
                expect_end();
                break;
            }

            case Op::MOV:
            case Op::NOT: {
                ins.a = parse_operand();
                expect_arrow();
                ins.c = reg(take_word());
                expect_end();
                break;
            }

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
            case Op::AND:
            case Op::OR:
            case Op::XOR: {
                ins.a = parse_operand();
                expect_comma();
                ins.b = parse_operand();
                expect_arrow();
                ins.c = reg(take_word());
                expect_end();
                break;
            }

            case Op::LOAD:
                ins.addr = parse_address();
                expect_arrow();
                ins.c = reg(take_word());
                expect_end();
                break;

            case Op::STORE:
                ins.a = parse_operand();
                expect_arrow();
                ins.addr = parse_address();
                expect_end();
                break;

            case Op::ALLOC:
                ins.a = parse_operand();
                expect_arrow();
                ins.c = reg(take_word());
                expect_end();
                break;

            case Op::MARK:
            case Op::JUMP:
                ins.label = take_word();
                expect_end();
                break;

            case Op::JUMPIF:
                ins.a = parse_operand();
                expect_comma();
                ins.label = take_word();
                expect_end();
                break;
        }

        prog_.push_back(std::move(ins));
    }

   private:
    std::string file_;
    std::vector<Token> toks_;
    int line_no_;
    std::vector<Instr>& prog_;
    size_t k_ = 0;

    [[noreturn]] void err(int col, const std::string& what) const {
        throw std::runtime_error(file_ + ":" + std::to_string(line_no_) + ":" +
                                 std::to_string(col) + ": error: " + what);
    }

    bool at_end() const { return k_ >= toks_.size(); }

    int end_col() const {
        if (toks_.empty()) return 1;
        const Token& last = toks_.back();
        return last.col + (int)last.text.size();
    }

    const Token& peek() const {
        if (at_end()) err(end_col(), "unexpected end of line");
        return toks_[k_];
    }

    std::string take_word() {
        const Token& t = peek();
        std::string w = t.text;
        ++k_;
        return w;
    }

    void expect_tok(const char* what, const char* printed) {
        if (at_end() || toks_[k_].text != what)
            err(at_end() ? end_col() : toks_[k_].col,
                std::string("expected '") + printed + "'");
        ++k_;
    }

    void expect_arrow() { expect_tok("->", "->"); }
    void expect_comma() { expect_tok(",", ","); }

    void expect_end() {
        if (!at_end())
            err(toks_[k_].col, "unexpected token: " + toks_[k_].text);
    }

    Operand parse_operand() {
        bool neg = false;
        if (!at_end() && toks_[k_].text == "-") {
            neg = true;
            ++k_;
        } else if (!at_end() && toks_[k_].text == "+") {
            ++k_;
        }

        const Token& t = peek();
        const std::string& s = t.text;
        Operand o;
        if (!s.empty() && (s[0] == 'R' || s[0] == 'r')) {
            o.is_const = false;
            o.reg = reg(s);
            ++k_;
        } else {
            o.is_const = true;
            try {
                size_t used = 0;
                o.val = std::stoll(s, &used);
                if (used != s.size())
                    throw std::runtime_error("trailing");
            } catch (...) {
                err(t.col, "not a register or number: " + s);
            }
            if (neg) o.val = -o.val;
            ++k_;
        }
        return o;
    }

    Address parse_address() {
        // caller knows first token is '['
        expect_tok("[", "[");
        Address a;
        a.base = parse_operand();
        if (!at_end() && (toks_[k_].text == "+" || toks_[k_].text == "-")) {
            a.subtract = (toks_[k_].text == "-");
            a.has_offset = true;
            ++k_;
            a.offset = parse_operand();
        }
        expect_tok("]", "]");
        return a;
    }

    int reg(const std::string& s) {
        try {
            return reg_from_name(s);
        } catch (const std::exception& e) {
            // Column of the token that produced it: it's the previous
            // consumed token, i.e. toks_[k_-1] if we just consumed it.
            int col = at_end()
                          ? end_col()
                          : (k_ >= 1 ? toks_[k_ - 1].col : 1);
            err(col, e.what());
        }
    }
};

// ---------------------------------------------------------------- interpreter

class Interp {
    std::vector<Instr> prog;
    int64_t regs[16] = {};
    std::vector<int64_t*> allocations;
    size_t steps = 0;
    size_t max_steps;
    bool line_has_output = false;

    int64_t value_of(const Operand& o) const {
        return o.is_const ? o.val : regs[o.reg];
    }

    int64_t cell_of(const Address& a) const {
        int64_t cell = value_of(a.base);
        if (a.has_offset) {
            int64_t off = value_of(a.offset);
            cell += a.subtract ? -off : off;
        }
        return cell;
    }

    int64_t* ptr_of(const Address& a) const {
        return reinterpret_cast<int64_t*>(cell_of(a) * 8);
    }

    [[noreturn]] static void rerr(const Instr& in, const std::string& file,
                                  const std::string& what) {
        throw std::runtime_error(file + ":" + std::to_string(in.line) + ":" +
                                 std::to_string(in.col) + ": error: " + what);
    }

   public:
    std::string file_name;
    explicit Interp(size_t limit = 10'000'000) : max_steps(limit) {}

    ~Interp() {
        for (int64_t* p : allocations) delete[] p;
    }

    Interp(const Interp&) = delete;
    Interp& operator=(const Interp&) = delete;

    void load(std::istream& in) {
        std::string line;
        int lineno = 0;
        std::vector<Instr> parsed;

        while (std::getline(in, line)) {
            ++lineno;
            std::vector<Token> toks = lex_line(line);
            if (toks.empty()) continue;
            Parser p(file_name, std::move(toks), lineno, parsed);
            p.parse_line();
        }

        // resolve labels
        std::unordered_map<std::string, int> marks;
        for (size_t i = 0; i < parsed.size(); ++i) {
            if (parsed[i].op == Op::MARK) {
                auto it = marks.find(parsed[i].label);
                if (it != marks.end()) {
                    rerr(parsed[i], file_name,
                         "duplicate label: " + parsed[i].label);
                }
                marks[parsed[i].label] = (int)i;
            }
        }
        for (auto& ins : parsed) {
            if (ins.op == Op::JUMP || ins.op == Op::JUMPIF) {
                auto it = marks.find(ins.label);
                if (it == marks.end())
                    rerr(ins, file_name,
                         "unknown label: " + ins.label);
                ins.target = it->second;
            }
        }
        prog = std::move(parsed);
    }

    void run() {
        size_t ip = 0;
        while (ip < prog.size()) {
            if (++steps > max_steps)
                throw std::runtime_error(
                    file_name + ":" + "0:0: error: step limit exceeded "
                                       "(possible infinite loop)");

            const Instr& ins = prog[ip];
            bool jumped = false;

            switch (ins.op) {
                case Op::MARK:
                    break;

                case Op::READ:
                    if (!(std::cin >> regs[ins.c]))
                        rerr(ins, file_name, "failed to read a number");
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
                    if (d == 0) rerr(ins, file_name, "division by zero");
                    regs[ins.c] = value_of(ins.a) / d;
                    break;
                }
                case Op::MOD: {
                    int64_t d = value_of(ins.b);
                    if (d == 0) rerr(ins, file_name, "modulo by zero");
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

                case Op::AND:
                    regs[ins.c] =
                        (value_of(ins.a) != 0 && value_of(ins.b) != 0) ? 1 : 0;
                    break;
                case Op::OR:
                    regs[ins.c] =
                        (value_of(ins.a) != 0 || value_of(ins.b) != 0) ? 1 : 0;
                    break;
                case Op::XOR:
                    regs[ins.c] =
                        ((value_of(ins.a) != 0) != (value_of(ins.b) != 0)) ? 1
                                                                           : 0;
                    break;
                case Op::NOT:
                    regs[ins.c] = (value_of(ins.a) == 0) ? 1 : 0;
                    break;

                case Op::LOAD:
                    regs[ins.c] = *ptr_of(ins.addr);
                    break;

                case Op::STORE:
                    *ptr_of(ins.addr) = value_of(ins.a);
                    break;

                case Op::ALLOC: {
                    int64_t n = value_of(ins.a);
                    if (n < 0)
                        rerr(ins, file_name,
                             "ALLOC with negative size");
                    int64_t* p = new int64_t[n];
                    allocations.push_back(p);
                    regs[ins.c] = reinterpret_cast<int64_t>(p) / 8;
                    break;
                }

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

// ---------------------------------------------------------------- main

int main(int argc, char** argv) {
#ifdef _WIN32
    // Make sure non-ASCII paths/messages in any future output work.
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " program.asm\n";
        return 1;
    }

    std::ifstream f(argv[1]);
    if (!f) {
        std::cerr << argv[1] << ": error: cannot open file\n";
        return 1;
    }

    try {
        Interp vm;
        vm.file_name = argv[1];
        vm.load(f);
        vm.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}