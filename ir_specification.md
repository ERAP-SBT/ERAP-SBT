# IR Specification

Important: reduce implicit state. 

A list of functions, which contain a list of basic blocks.
```EBNF
IR = { function };
function = (*entry_point*) basic_block, (*blocks*){ basic_block };
basic_block = { operation }, (*inputs*){ static_value_mapping }, (*outputs*){ static_value_mapping }, (*predecessors*){ basic_block }, (*successors*){ basic_block }, control_flow_change;
operation = { variable }, instruction, { value };
control_flow_change = (jump | conditional_jump | return | call);
static_value_mapping = variable, ( "<-" | "->" ), static_variable;
value = (constant | variable);
variable = identifier, "{", type, "}";
constant = number, { number }, "{", type, "}";
static_variable = static, "@", identifier;
identifier = letter, { number };
comment = ("//", { letter } ) | ("/*", { letter }, "*/");
```
Example for static variable definitions (typically at the top of the file, global):
```
static @r01  // declare static variables which are always assigned to the same location (if needed)
static @r02
static @r03
```
Example operations:
```
// these operation are normally contained in a basic block
v1{Int64} <- constant 500{Constant64}
v2{Int64} <- add v1{Int64}, v1{Int64} // constants can't be used directly with instructions like add, but might be inlined
```
Example basic blocks:
```
// this basic block would normally be countained in a function
block b122233(v1 <- @r01, v2 <- @r03) <= [Predecessors] {
    Op01
    Op02
    Op03
}(v3 -> @r02) => [Successors]
```
Example functions:
```
function f1902831023(v1 <- @r01, v2 <- @r03) {
    b01
    b02
    b03
} (v3 -> @r01)
```

## Types

Constants
* Const64
* Const32
* Const16
* Const8

Integers
* Int64
* Int32
* Int16
* Int8

Floating Point Types
* Float64
* Float32

### Memory types

Memory References (Address | addr) := (Int64 | Int32)

## Variables

All variables are ssa and are only valid in the scope of the basic block they were created in.
For any values which should be preserved between basic blocks, the variables have to be mapped
to static variables which are always mapped to the same hardware register or memory address.

    static @r01
    static @r02

## BasicBlock

```
block <name> (<ssa-var> <- <static-var>, ...) <= [Predecessors] {
    Operation1
    Operation2
    Operation3
}(<ssa-var> -> <static-var>, ...) => [Successors]
```

```
block b122233(v1 <- @r01, v2 <- @r03) <= [b1, b202] {
    Op01
    Op02
    Op03
}(v3 -> @r02) => [b011, b212]
```

### Intrinsics

* `setup_stack() -> Int64`
* `syscall(...) -> Int64`
* `ijump(Int64) -> null`
* `icall(Int64, arg1, arg2, ...) -> Int64`

-> Usage like instructions

### Instructions / Operations

* `<variable:addr> <- store <addr>, <variable>`
* `<variable> <- load <addr>`
* `<variable> <- add <variable>, <variable>`
* `<variable> <- sub <variable>, <variable>`
* `<variable> <- mul <variable>, <variable>`
* `<variable> <- div <variable>, <variable>`
* `<variable> <- shl <variable>, <variable>`
* `<variable> <- shr <variable>, <variable>`
* `<variable> <- sar <variable>, <variable>`
* `<variable> <- or <variable>, <variable>`
* `<variable> <- and <variable>, <variable>`
* `<variable> <- not <variable>, <variable>`
* `<variable> <- xor <variable>, <variable>`
* `<variable> <- constant <constant>`

### Control-Flow

* `jump <addr>`
* `cjump <addr>, <variable>`
* `call <addr>`
* `return`
* `noreturn`
  "Instruction" which will be expanded to a panic call, but the IR says, the code won't be executed any longer...

### Syscalls

Special "function calls" / control-flow change instruction ?

## Examples Dump

### Example for tail-calls

```c
#include <stdint.h>

__attribute__((noinline))
uint64_t f1(uint64_t b) {
  return b + 1;
}

uint64_t f2(uint64_t a) {
  return f1(a);
}
```

### Example for load-immediate

```c
#include <stdint.h>

uint64_t f(void) {
  return 0xbeefbeefbeefbeef;
}
```

### Examples (not syntactically correct)

``` 
x1 <- load byte [x2]
[x1] <- store x2

memory write order:
w1 <- write byte[x1], x1, ...
w2 <- write byte[x1], x2, w1

/* mov x1 <- x2 */
x1 <- addi x0, x2
x1 <- addi x0, x3



x1 <- addi x0, x2
write x1 -> 0xbeef

x1 <- addi, x0, x3
write x1 -> 0xabc

x1 <- addi, x0, x2



=>
x1 <- addi, x0, x3
write x1 -> 0xabc
x1 <- addi x0, x2
write x1 -> 0xbeef


lui:

x1 <- lui(x1, 0xbeef)

  lui a0, 1048560
  addiw a0, a0, -1089
  slli a0, a0, 16
  addi a0, a0, -1089
  slli a0, a0, 16
  addi a0, a0, -1089
  slli a0, a0, 14
  addi a0, a0, -273 

=> liftet

/* risc-v lifter generiert */
register r0(64);
register x1(64);
register x2;

/* x86_64 lifter generiert */
register rip;
register rep;

basic_block b012223214567u8i(v1 <- @x1 ; v6 -> @x1) {
/* SSA begin */
    v1{u64} <- const 0xFFFF_0000_0000_0000
    v3 <- and v1, 0x0FFF_FFFF_FFFF
    v4 <- or v3, v2
    // addo
    v5 <- addi v4, -1089
    v6 <- shlli v5, 16
    // ...
/* SSA: end */
  ijump v6
}

function entry() {
  b01
}

/**

push x7 
call x1
pop x7
**/

basic_block b06(v5 <- @r10, v6 <- @r11, ...) {
  v1 <- add @rsp, 4
  store v1, @r7
  v2 <- @r1
  call v2(v5,v6,v7,v8,v9)
  load @r7, v1
  v3 <- sub v1, 4
}

static @r0

basic_block b05(v1 <- @r0; v2 -> @r1; v3 -> @r0) {
  v2 <- addi64 v1, 10
  v3 <- shll v2, 10
  v5 <- load
  v6 <- load
  jmp b06
}

basic_block b06(v1 <- @r1; v2 -> @r1) {
  v2 <- addi64 v1, 10
  return
}

function fn2() {
  b05
  b06
}

function fn2(v1 <- @r0; v2 -> @r1; v3 -> @r0) {
b04:
  v2 <- addi64 v1, 15
  jmp b06
b05:
  v3 <- addi64 v1, 10
  v4 <- shll v2, 10
  jmp b06
b06:
  v5 <- phi (v2, v3)
  v6 <- addi64 
}

function fn1() {
  b01
  b02
  b03 <- return
}
```