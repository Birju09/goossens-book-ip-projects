# RV32I Instruction Formats

Quick reference for the six base instruction encodings, bit ranges, and how they map to
this project's `decode.cpp` / `execute.cpp` fields.

## Bit field layout

| Format | 31:25 | 24:20 | 19:15 | 14:12 | 11:7 | 6:0 |
|---|---|---|---|---|---|---|
| **R-type** | `func7` | `rs2` | `rs1` | `func3` | `rd` | `opcode` |
| **I-type** | `imm[11:5]`\* | `imm[4:0]` (or `shamt`) | `rs1` | `func3` | `rd` | `opcode` |
| **S-type** | `imm[11:5]` | `rs2` | `rs1` | `func3` | `imm[4:0]` | `opcode` |
| **B-type** | `imm[12\|10:5]` | `rs2` | `rs1` | `func3` | `imm[4:1\|11]` | `opcode` |
| **U-type** | `imm[31:12]` (20 bits, spans 31:12) | | `rd` | `opcode` |
| **J-type** | `imm[20\|10:1\|11\|19:12]` (20 bits, spans 31:12) | | `rd` | `opcode` |

\* For shift immediates (`SLLI`/`SRLI`/`SRAI`), bits `[24:20]` hold `shamt` (5-bit shift
amount) instead of part of the immediate, and bits `[31:25]` are restricted to `0000000`
(logical) or `0100000` (arithmetic) — mirroring R-type's `func7`.

Notes:
- B-type and J-type immediates encode a **byte offset with bit 0 implicitly 0** — the bits
  are stored out of numeric order (see below) so the hardware needs less shuffling, and the
  LSB is always 0 since branch/jump targets are 2-byte (compressed-aligned) or 4-byte aligned.
- U-type and J-type both consume the full `[31:12]` field for their immediate; only `rd` and
  `opcode` remain from the low bits.

## Immediate bit reassembly

| Format | Immediate value (sign-extended from bit 31) |
|---|---|
| I-type | `{inst[31:20]}` |
| S-type | `{inst[31:25], inst[11:7]}` |
| B-type | `{inst[31], inst[7], inst[30:25], inst[11:8], 1'b0}` |
| U-type | `{inst[31:12], 12'b0}` |
| J-type | `{inst[31], inst[19:12], inst[20], inst[30:21], 1'b0}` |

## opcode → format (RV32I base set)

| opcode (bits 6:0) | Mnemonic group | Format |
|---|---|---|
| `0110011` | R-type ALU (`ADD`/`SUB`/`SLL`/`SLT`/`SLTU`/`XOR`/`SRL`/`SRA`/`OR`/`AND`) | R |
| `0010011` | I-type ALU imm (`ADDI`/`SLTI`/`SLTIU`/`XORI`/`ORI`/`ANDI`/`SLLI`/`SRLI`/`SRAI`) | I |
| `0000011` | `LOAD` (`LB`/`LH`/`LW`/`LBU`/`LHU`) | I |
| `1100111` | `JALR` | I |
| `1110011` | `SYSTEM` (`ECALL`/`EBREAK`) | I |
| `0100011` | `STORE` (`SB`/`SH`/`SW`) | S |
| `1100011` | `BRANCH` (`BEQ`/`BNE`/`BLT`/`BGE`/`BLTU`/`BGEU`) | B |
| `0110111` | `LUI` | U |
| `0010111` | `AUIPC` | U |
| `1101111` | `JAL` | J |

## func3 encodings

| func3 | ALU/OP_IMM (`R`/`I`) | BRANCH (`B`) | LOAD/STORE |
|---|---|---|---|
| `000` | `ADD`/`SUB`/`ADDI` | `BEQ` | `LB` / `SB` |
| `001` | `SLL`/`SLLI` | `BNE` | `LH` / `SH` |
| `010` | `SLT`/`SLTI` | — | `LW` / `SW` |
| `011` | `SLTU`/`SLTIU` | — | — |
| `100` | `XOR`/`XORI` | `BLT` | `LBU` |
| `101` | `SRL`/`SRA`/`SRLI`/`SRAI` | `BGE` | `LHU` |
| `110` | `OR`/`ORI` | `BLTU` | — |
| `111` | `AND`/`ANDI` | `BGEU` | — |

`func7` bit 30 (`func7[5]`) is the only bit that distinguishes same-`func3` pairs:
`ADD`/`SUB`, `SRL`/`SRA` (and `SRLI`/`SRAI` via the immediate's high bits).

## Mapping to this repo's `decoded_instruction_t`

| Field (`decode.cpp`) | Bits extracted | Meaning |
|---|---|---|
| `opcode` | `instruction >> 2` (i.e. `[6:2]`, opcode's `[1:0]` are always `11` for RV32) | Instruction class |
| `rd` | `instruction >> 7` → `[11:7]` | Destination register |
| `func3` | `instruction >> 12` → `[14:12]` | Sub-operation selector |
| `rs1` | `instruction >> 15` → `[19:15]` | Source register 1 |
| `rs2` | `instruction >> 20` → `[24:20]` | Source register 2 (or `shamt` for shift-imm) |
| `func7` | `instruction >> 25` → `[31:25]` | R-type op variant bit (only meaningful when `type == R_TYPE`, or for shift immediates) |
| `imm` | via `immediate.cpp` helpers per `type` | Reassembled, sign-extended immediate |

## Full instruction list

### R-type (register-register ALU)

| Mnemonic | Brief | Example |
|---|---|---|
| `ADD` | `rd = rs1 + rs2` | `add x5, x6, x7` |
| `SUB` | `rd = rs1 - rs2` | `sub x5, x6, x7` |
| `SLL` | `rd = rs1 << rs2[4:0]` (logical left shift) | `sll x5, x6, x7` |
| `SLT` | `rd = (rs1 < rs2) ? 1 : 0`, signed compare | `slt x5, x6, x7` |
| `SLTU` | `rd = (rs1 < rs2) ? 1 : 0`, unsigned compare | `sltu x5, x6, x7` |
| `XOR` | `rd = rs1 ^ rs2` | `xor x5, x6, x7` |
| `SRL` | `rd = (unsigned)rs1 >> rs2[4:0]` (logical right shift) | `srl x5, x6, x7` |
| `SRA` | `rd = (signed)rs1 >> rs2[4:0]` (arithmetic right shift) | `sra x5, x6, x7` |
| `OR` | `rd = rs1 \| rs2` | `or x5, x6, x7` |
| `AND` | `rd = rs1 & rs2` | `and x5, x6, x7` |

### I-type (immediate ALU, loads, jump-register, system)

| Mnemonic | Brief | Example |
|---|---|---|
| `ADDI` | `rd = rs1 + imm` | `addi x5, x6, 10` |
| `SLTI` | `rd = (rs1 < imm) ? 1 : 0`, signed | `slti x5, x6, -1` |
| `SLTIU` | `rd = (rs1 < imm) ? 1 : 0`, unsigned | `sltiu x5, x6, 1` |
| `XORI` | `rd = rs1 ^ imm` (with `imm = -1`, bitwise NOT) | `xori x5, x6, -1` |
| `ORI` | `rd = rs1 \| imm` | `ori x5, x6, 0xF` |
| `ANDI` | `rd = rs1 & imm` | `andi x5, x6, 0xFF` |
| `SLLI` | `rd = rs1 << shamt` | `slli x5, x6, 2` |
| `SRLI` | `rd = (unsigned)rs1 >> shamt` | `srli x5, x6, 2` |
| `SRAI` | `rd = (signed)rs1 >> shamt` | `srai x5, x6, 2` |
| `LB` | `rd = sign_extend(mem8[rs1 + imm])` | `lb x5, 0(x6)` |
| `LH` | `rd = sign_extend(mem16[rs1 + imm])` | `lh x5, 0(x6)` |
| `LW` | `rd = mem32[rs1 + imm]` | `lw x5, 0(x6)` |
| `LBU` | `rd = zero_extend(mem8[rs1 + imm])` | `lbu x5, 0(x6)` |
| `LHU` | `rd = zero_extend(mem16[rs1 + imm])` | `lhu x5, 0(x6)` |
| `JALR` | `rd = pc + 4; pc = (rs1 + imm) & ~1` (indirect jump-and-link) | `jalr x1, x6, 0` |
| `ECALL` | Environment call (trap to system/OS) | `ecall` |
| `EBREAK` | Environment breakpoint (trap to debugger) | `ebreak` |

### S-type (stores)

| Mnemonic | Brief | Example |
|---|---|---|
| `SB` | `mem8[rs1 + imm] = rs2[7:0]` | `sb x5, 0(x6)` |
| `SH` | `mem16[rs1 + imm] = rs2[15:0]` | `sh x5, 0(x6)` |
| `SW` | `mem32[rs1 + imm] = rs2` | `sw x5, 0(x6)` |

### B-type (conditional branches, PC-relative)

| Mnemonic | Brief | Example |
|---|---|---|
| `BEQ` | branch if `rs1 == rs2` | `beq x5, x6, label` |
| `BNE` | branch if `rs1 != rs2` | `bne x5, x6, label` |
| `BLT` | branch if `rs1 < rs2`, signed | `blt x5, x6, label` |
| `BGE` | branch if `rs1 >= rs2`, signed | `bge x5, x6, label` |
| `BLTU` | branch if `rs1 < rs2`, unsigned | `bltu x5, x6, label` |
| `BGEU` | branch if `rs1 >= rs2`, unsigned | `bgeu x5, x6, label` |

### U-type (20-bit upper immediate)

| Mnemonic | Brief | Example |
|---|---|---|
| `LUI` | `rd = imm << 12` (load upper immediate) | `lui x5, 0x10000` |
| `AUIPC` | `rd = pc + (imm << 12)` (add upper immediate to PC, for PC-relative addressing) | `auipc x5, 0x1` |

### J-type (unconditional jump, PC-relative)

| Mnemonic | Brief | Example |
|---|---|---|
| `JAL` | `rd = pc + 4; pc = pc + imm` (jump-and-link) | `jal x1, label` |

Notes:
- `x0` is hardwired to zero; writes to `rd == 0` are discarded (see `write_reg` in
  `execute.cpp`, which skips writes when `d_i.rd == 0`).
- Common pseudo-instructions built from these: `nop` = `addi x0, x0, 0`, `mv rd, rs` =
  `addi rd, rs, 0`, `not rd, rs` = `xori rd, rs, -1`, `j label` = `jal x0, label`,
  `ret` = `jalr x0, x1, 0`, `li rd, imm` = `lui`+`addi` pair for large immediates.
