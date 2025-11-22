#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <bit>
#include <cstdint>

enum class OpCode : uint8_t {
    // --- Load/store ---
    LOAD_CONST, LOAD_NULL, LOAD_TRUE, LOAD_FALSE,
    LOAD_INT, LOAD_FLOAT,
    MOVE,
    // --- Binary ---
    ADD, SUB, MUL, DIV, MOD, POW,
    EQ, NEQ, GT, GE, LT, LE,
    // --- Unary ---
    NEG, NOT,
    // --- Constant & variable
    GET_GLOBAL, SET_GLOBAL,
    GET_UPVALUE, SET_UPVALUE,
    CLOSURE,
    CLOSE_UPVALUES,
    // --- Control flow ---
    JUMP,
    JUMP_IF_FALSE, JUMP_IF_TRUE,
    CALL, CALL_VOID,
    RETURN,
    HALT,
    // --- Data structure ---
    NEW_ARRAY, NEW_HASH,
    GET_INDEX, SET_INDEX,
    GET_KEYS, GET_VALUES,
    // --- Classes & objexts ---
    NEW_CLASS, NEW_INSTANCE,
    GET_PROP, SET_PROP, SET_METHOD,
    INHERIT,
    GET_SUPER,
    // --- Bitwise ---
    BIT_AND, BIT_OR, BIT_XOR, BIT_NOT, LSHIFT, RSHIFT,
    // --- Try/catch ---
    THROW,
    SETUP_TRY,
    POP_TRY,
    // --- Module ---
    IMPORT_MODULE,
    EXPORT,
    GET_EXPORT,
    IMPORT_ALL,
    // --- Metadata ---
    TOTAL_OPCODES
};

// Cấu trúc file binary dựa trên src/module/loader/binary_loader.cpp:
// 1. Magic (u32): 0x4D454F57 ("MEOW")
// 2. Version (u32): 1
// 3. Main Proto Index (u32)
// 4. Proto Count (u32)
// 5. Prototypes...

void write_u8(std::ofstream& out, uint8_t v) { out.write((char*)&v, 1); }
void write_u16(std::ofstream& out, uint16_t v) { out.write((char*)&v, 2); }
void write_u32(std::ofstream& out, uint32_t v) { out.write((char*)&v, 4); }
void write_u64(std::ofstream& out, uint64_t v) { out.write((char*)&v, 8); }
void write_str(std::ofstream& out, const std::string& s) {
    write_u32(out, s.size());
    out.write(s.data(), s.size());
}

int main() {
    std::ofstream out("test.meowb", std::ios::binary);
    if (!out) return 1;

    // --- Header ---
    write_u32(out, 0x4D454F57); // Magic "MEOW"
    write_u32(out, 1);          // Version
    write_u32(out, 0);          // Main Proto Index (là proto đầu tiên)
    write_u32(out, 1);          // Total Prototypes (chỉ có 1 hàm main)

    // --- Prototype 0 (Main) ---
    write_u32(out, 3); // Num Registers (R0, R1, R2)
    write_u32(out, 0); // Num Upvalues
    write_u32(out, 0); // Name Index trong Constant Pool (sẽ trỏ tới string "main")

    // --- Constant Pool ---
    // Ta cần 1 constant là string "main" để làm tên hàm.
    write_u32(out, 1); // Pool Size = 1

    // Constant 0: String "main"
    // Tag: 0=NULL, 1=INT, 2=FLOAT, 3=STRING
    write_u8(out, 3);      // Tag String
    write_str(out, "main"); // Payload

    // --- Upvalue Descs ---
    write_u32(out, 0); // Count = 0 (không có upvalue)

    // --- Bytecode ---
    // Logic:
    // LOAD_INT R1, 10
    // LOAD_INT R2, 20
    // ADD      R0, R1, R2  (R0 = 10 + 20)
    // HALT                 (Machine sẽ in R0 ra)
    
    std::vector<uint8_t> code;
    
    // LOAD_INT r1, 10
    code.push_back((uint8_t)OpCode::LOAD_INT);
    code.push_back(1); code.push_back(0); // r1 (u16)
    // 10 (i64)
    uint64_t val1 = 10;
    for(int i=0; i<8; ++i) code.push_back((val1 >> (i*8)) & 0xFF);

    // LOAD_INT r2, 20
    code.push_back((uint8_t)OpCode::LOAD_INT);
    code.push_back(2); code.push_back(0); // r2 (u16)
    // 20 (i64)
    uint64_t val2 = 20;
    for(int i=0; i<8; ++i) code.push_back((val2 >> (i*8)) & 0xFF);

    // ADD r0, r1, r2
    code.push_back((uint8_t)OpCode::ADD);
    code.push_back(0); code.push_back(0); // dst r0
    code.push_back(1); code.push_back(0); // src r1
    code.push_back(2); code.push_back(0); // src r2

    // HALT
    code.push_back((uint8_t)OpCode::HALT);

    // Write Bytecode Size & Data
    write_u32(out, code.size());
    out.write((char*)code.data(), code.size());

    out.close();
    std::cout << "Created test.meowb successfully!" << std::endl;
    return 0;
}