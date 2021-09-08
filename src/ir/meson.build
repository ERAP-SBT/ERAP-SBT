ir_sources = [
  'ir.cpp', 'basic_block.cpp', 'function.cpp', 'operation.cpp', 'variable.cpp', 'type.cpp', 'instruction.cpp', 'eval.cpp',
  'optimizer/common.cpp', 'optimizer/const_folding.cpp', 'optimizer/dce.cpp', 'optimizer/dedup_imm.cpp'
]
ir = static_library('ir', ir_sources, include_directories : inc)

subdir('tests')
