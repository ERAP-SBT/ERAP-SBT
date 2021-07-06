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
static i64 @x1  // declare static variables which are always assigned to the same location (if needed)
static i64 @x2
static i64 @x3

static memory @M
```

Example operations:
```
// these operation are normally contained in a basic block
i64 v1 <- immediate 500
i64 v2 <- add i64 v1, i64 v1 // constants can't be used directly with instructions like add, but might be inlined
```

Example basic blocks:
```
// this basic block would normally be contained in a function
block b122233(v1 <- @x1, v2 <- @x3) <= [Predecessors] {
    Op01
    Op02
    Op03
} => (ControlFlowOperation, [(Successor?, Parameter-Mapping)], AdditionalInformation)

// these blocks can't be jumped to but can still be included in functions
block b01(i64 v1, i64 v2, i64 v3) <= [Predecessors] {
  v4 <- add i64 v1, i64 v2
} => (ControlFlowOperation, [(b122233, v4)], AdditionalInformation)
```

Example functions:
```
function f1902831023(v1 <- @x1, v2 <- @x3) {
    b01
    b02
    b03
} (v3 -> @r01)
```

## Types

Integers
* Int64 (i64)
* Int32 (i32)
* Int16 (i16)
* Int8 (i8)

Floating Point Types
* Float64 (f64)
* Float32 (f32)

### Memory types

Memory references (addresses) are simply stored as i64 or i32.

To preserve the order of loads/stores when reordering instructions, a memory token type `mt` is used.
This type is _NOT_ an address; it only exists in the IR.


## Variables

All variables are SSA and are only valid in the scope of the BasicBlock they were created in.

## Statics

For any values which needs to be preserved at some points in a known locations to preserve ABI compatibility.
Their meaning is defined by the Lifter / Compiler and depends on the source architecture.
They are declared globally.

```
/* RISC-V register */
static i64 @x1

/* Memory token */
static memory @M
```

## BasicBlock

Only Basic Blocks that have exclusively static vars as input/output are valid jump targets for indirect jumps
and start/end of functions.

```
block <name> (<ssa-var> <- <static-var>, <ssa-var>, ...) <= [Predecessors] {
    Operation1
    Operation2
    Operation3
}(ControlFlowOperation, [(Successor?, Parameter-Mapping)], AdditionalInformation)
Parameter-Mapping := (<static-var> <- <ssa-var> | <ssa-var>)*
```

```
block b122233(v1 <- @x1, v2 <- @x3) <= [b1, b202] {
    v3 <- add v1, v2
}(jump, [b555, (@x1 <- v2)], null)
```

### Instructions / Operations

* `<memory-token> <- store <addr: Int64>, <value>, <memory-token>`
* `<result> <- load <addr: Int64>, <memory-token>`
* `<result> <- add <a>, <b>`
* `<result> <- sub <a>, <b>`
* `<result> <- mul_l <a>, <b>`
* `<result> <- ssmul_h <signed>, <signed>`
* `<result> <- uumul_h <unsigned>, <unsigned>`
* `<result> <- sumul_h <signed>, <unsigned>`
* `<result>, <remainder> <- div <a>, <b>`
* `<result>, <remainder> <- udiv <a>, <b>`
* `<result> <- shl <value>, <amount>`
* `<result> <- shr <value>, <amount>`
* `<result> <- sar <value>, <amount>`
* `<result> <- or <a>, <b>`
* `<result> <- and <a>, <b>`
* `<result> <- not <value>`
* `<result> <- xor <a>, <b>`
* `<result> <- cast <value>`
* `<result> <- slt <a>, <b>, <value if less>, <value otherwise>`
* `<result> <- sltu <a>, <b>, <value if less>, <value otherwise>`
* `<result> <- sign_extend <value>`
* `<result> <- zero_extend <value>`
* `<result> <- setup_stack`

More can be added as necessary or useful.

#### Immediate
* `<variable> <- imm 1337`
This is a pseudo-instruction that is only printed as one but the variable is instantly assigned with the value upon its creation

#### Intrinsic Instructions

* `<variable: Int64> <- setup_stack <memory-token>`

#### Control-Flow

* `jump`: Jump, has 1 Successors: a known BasicBlock
* `ijump`: Indirect Jump, always taken to a known/unknown BasicBlock
* `cjump`: Conditional Jump, has at least 2 Successors to known/unknown BasicBlocks
* `call`: Call, always taken, has 2 Successors, a Function and a continuation BasicBlock
* `icall`, Indirect Call, always taken
* `return`
* `unreachable`: Behavior is undefined if reached.
* `syscall`: Similar to function call
