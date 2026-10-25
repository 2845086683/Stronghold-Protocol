# Independent Win64 compiler thunks preserve register and stack arguments.
# RIP-relative memory jumps are valid Windows x64 unwind epilogues.
.text

.globl D3DAssemble
.def D3DAssemble; .scl 2; .type 32; .endef
.seh_proc D3DAssemble
D3DAssemble:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $1, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+8(%rip)
.seh_endproc

.globl DebugSetMute
.def DebugSetMute; .scl 2; .type 32; .endef
.seh_proc DebugSetMute
DebugSetMute:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $2, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+16(%rip)
.seh_endproc

.globl D3DCompile
.def D3DCompile; .scl 2; .type 32; .endef
.seh_proc D3DCompile
D3DCompile:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $3, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+24(%rip)
.seh_endproc

.globl D3DCompile2
.def D3DCompile2; .scl 2; .type 32; .endef
.seh_proc D3DCompile2
D3DCompile2:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $4, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+32(%rip)
.seh_endproc

.globl D3DCompileFromFile
.def D3DCompileFromFile; .scl 2; .type 32; .endef
.seh_proc D3DCompileFromFile
D3DCompileFromFile:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $5, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+40(%rip)
.seh_endproc

.globl D3DCompressShaders
.def D3DCompressShaders; .scl 2; .type 32; .endef
.seh_proc D3DCompressShaders
D3DCompressShaders:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $6, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+48(%rip)
.seh_endproc

.globl D3DCreateBlob
.def D3DCreateBlob; .scl 2; .type 32; .endef
.seh_proc D3DCreateBlob
D3DCreateBlob:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $7, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+56(%rip)
.seh_endproc

.globl D3DCreateFunctionLinkingGraph
.def D3DCreateFunctionLinkingGraph; .scl 2; .type 32; .endef
.seh_proc D3DCreateFunctionLinkingGraph
D3DCreateFunctionLinkingGraph:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $8, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+64(%rip)
.seh_endproc

.globl D3DCreateLinker
.def D3DCreateLinker; .scl 2; .type 32; .endef
.seh_proc D3DCreateLinker
D3DCreateLinker:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $9, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+72(%rip)
.seh_endproc

.globl D3DDecompressShaders
.def D3DDecompressShaders; .scl 2; .type 32; .endef
.seh_proc D3DDecompressShaders
D3DDecompressShaders:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $10, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+80(%rip)
.seh_endproc

.globl D3DDisassemble
.def D3DDisassemble; .scl 2; .type 32; .endef
.seh_proc D3DDisassemble
D3DDisassemble:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $11, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+88(%rip)
.seh_endproc

.globl D3DDisassemble10Effect
.def D3DDisassemble10Effect; .scl 2; .type 32; .endef
.seh_proc D3DDisassemble10Effect
D3DDisassemble10Effect:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $12, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+96(%rip)
.seh_endproc

.globl D3DDisassemble11Trace
.def D3DDisassemble11Trace; .scl 2; .type 32; .endef
.seh_proc D3DDisassemble11Trace
D3DDisassemble11Trace:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $13, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+104(%rip)
.seh_endproc

.globl D3DDisassembleRegion
.def D3DDisassembleRegion; .scl 2; .type 32; .endef
.seh_proc D3DDisassembleRegion
D3DDisassembleRegion:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $14, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+112(%rip)
.seh_endproc

.globl D3DGetBlobPart
.def D3DGetBlobPart; .scl 2; .type 32; .endef
.seh_proc D3DGetBlobPart
D3DGetBlobPart:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $15, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+120(%rip)
.seh_endproc

.globl D3DGetDebugInfo
.def D3DGetDebugInfo; .scl 2; .type 32; .endef
.seh_proc D3DGetDebugInfo
D3DGetDebugInfo:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $16, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+128(%rip)
.seh_endproc

.globl D3DGetInputAndOutputSignatureBlob
.def D3DGetInputAndOutputSignatureBlob; .scl 2; .type 32; .endef
.seh_proc D3DGetInputAndOutputSignatureBlob
D3DGetInputAndOutputSignatureBlob:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $17, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+136(%rip)
.seh_endproc

.globl D3DGetInputSignatureBlob
.def D3DGetInputSignatureBlob; .scl 2; .type 32; .endef
.seh_proc D3DGetInputSignatureBlob
D3DGetInputSignatureBlob:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $18, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+144(%rip)
.seh_endproc

.globl D3DGetOutputSignatureBlob
.def D3DGetOutputSignatureBlob; .scl 2; .type 32; .endef
.seh_proc D3DGetOutputSignatureBlob
D3DGetOutputSignatureBlob:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $19, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+152(%rip)
.seh_endproc

.globl D3DGetTraceInstructionOffsets
.def D3DGetTraceInstructionOffsets; .scl 2; .type 32; .endef
.seh_proc D3DGetTraceInstructionOffsets
D3DGetTraceInstructionOffsets:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $20, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+160(%rip)
.seh_endproc

.globl D3DLoadModule
.def D3DLoadModule; .scl 2; .type 32; .endef
.seh_proc D3DLoadModule
D3DLoadModule:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $21, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+168(%rip)
.seh_endproc

.globl D3DPreprocess
.def D3DPreprocess; .scl 2; .type 32; .endef
.seh_proc D3DPreprocess
D3DPreprocess:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $22, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+176(%rip)
.seh_endproc

.globl D3DReadFileToBlob
.def D3DReadFileToBlob; .scl 2; .type 32; .endef
.seh_proc D3DReadFileToBlob
D3DReadFileToBlob:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $23, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+184(%rip)
.seh_endproc

.globl D3DReflect
.def D3DReflect; .scl 2; .type 32; .endef
.seh_proc D3DReflect
D3DReflect:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $24, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+192(%rip)
.seh_endproc

.globl D3DReflectLibrary
.def D3DReflectLibrary; .scl 2; .type 32; .endef
.seh_proc D3DReflectLibrary
D3DReflectLibrary:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $25, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+200(%rip)
.seh_endproc

.globl D3DReturnFailure1
.def D3DReturnFailure1; .scl 2; .type 32; .endef
.seh_proc D3DReturnFailure1
D3DReturnFailure1:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $26, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+208(%rip)
.seh_endproc

.globl D3DSetBlobPart
.def D3DSetBlobPart; .scl 2; .type 32; .endef
.seh_proc D3DSetBlobPart
D3DSetBlobPart:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $27, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+216(%rip)
.seh_endproc

.globl D3DStripShader
.def D3DStripShader; .scl 2; .type 32; .endef
.seh_proc D3DStripShader
D3DStripShader:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $28, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+224(%rip)
.seh_endproc

.globl D3DWriteBlobToFile
.def D3DWriteBlobToFile; .scl 2; .type 32; .endef
.seh_proc D3DWriteBlobToFile
D3DWriteBlobToFile:
    subq $136, %rsp
    .seh_stackalloc 136
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movl $29, %ecx
    call SO_ResolveCompilerExport
    movq 32(%rsp), %rcx
    movq 40(%rsp), %rdx
    movq 48(%rsp), %r8
    movq 56(%rsp), %r9
    movdqu 64(%rsp),  %xmm0
    movdqu 80(%rsp),  %xmm1
    movdqu 96(%rsp),  %xmm2
    movdqu 112(%rsp), %xmm3
    addq $136, %rsp
    jmp *SO_CompilerTargets+232(%rip)
.seh_endproc
