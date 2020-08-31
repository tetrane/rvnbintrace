Described in this file is the version 1.0 of the binary trace format.

# Format overview

------
Header
 - Compression scheme
------
Machine description
 - Arch
 - Register table
 - Static registers
------
Initial memory sections
------
Initial registers
------
Events
 - event information
 - context diff
------

# Section description

Sections are declared as a stream of bytes, with sizes. Each section starts with a `Section size` value, which stores the size of the section _excluding this size parameter_.

Note that strings are ASCII, and are stored in the following way unless specified otherwise:
1B: string character count (no trailing null character)
XB: string, 1B per character.

## Header

8B: Section size
1B: Compression scheme used. 0: No compression

## Machine description

See section [architecture-definitions] for requirements on architecture. These include:
 - The architecture magic
 - The mandatory register list
 - The static register list: for example, required CPUID values

This section will declare what IDs are mapped to which register in this file.

8B: Section size
4B: Architecture magic
1B: Physical address size (in bytes) -- will name this PHY_SIZE
4B: Physical memory regions count
For each memory region:
    PHY_SIZE B: Start address
    PHY_SIZE B: size
4B: Register list size
For each register:
    2B: Register ID (must be unique)
    2B: Register size
    String: Register name
4B: Register operations count
For each register operation:
    1B: Register ID used for an operation (cannot overlap with existing register ID, cannot be 0xff, must be unique)
    2B: Destination Register ID (must be defined in previous list)
    1B: Operator (0 SET, 1 ADD, 2 AND, 3 OR)
    XB: Value used as operand for the operation (same size as register content)
4B: Static register list size
For each static register:
    String: Static register name (must be unique)
    1B: Following content size
    XB: Register content

## Initial Memory

8B: Section size: must be consistent with memory regions declaration: all memory region sizes added together.
XB: Memory regions

## Initial registers

The initial CPU context will contain the list of initial registers. All registers defined in machine description must be initialized.

8B: Section size
4B: Register list size
For each register:
    2B: Register ID
    XB! Register content (size depends on register description)

## Events

8B: Section size
8B: Trace event count
XB: events.

### Event general description

Each event contains metadata along with the corresponding context diff.

How to determine event's type:

1B: if < 0xff: is instruction event (or continuation) with diff (and this byte is first diff's byte)
    if = 0xff: is not instruction event:
        1B: Event type:
            0xff: other event, described by string

### Diff description

1B: upper half is Reg Count in the context diff: R
    lower half is Mem Count in the context diff: M

Note: both R and M cannot be equal to 0xf at the same time (see event type).

If any R or M is equal to 0xf, corresponding count is actually 0xe, and next event is a simple diff, continuation of current one.

For M:
    Memory content, described this way:
    PHY_SIZE B: Physical address
    1B: if < 0xff, is content size
        if = 0xff:
            PHY_SIZE B: content size

For R:
    Register content, described this way:

    1B: if < 0xff, is Reg ID on 1byte
        if = 0xff:
            2B: Reg ID

    If RegID is not an operation:
        XB: register content (according to declared size in Machine Description)
    XB: content (depending on previous size)

### Other event

String: Event description

Then Diff

# Architecture Definitions

Files must follow the specifiations below, depending on which architecture they declare using.

## amd64 version 1

### Architecture magic

'x641' or 0x31343678

### Static register list

CPUIDs (see Intel documentation):

- `cpuid_pat`
- `cpuid_pse36`
- `cpuid_1gb_pages`
- `cpuid_max_phy_addr`
- `cpuid_max_lin_addr`

### Mandatory register list

Some necessary registers are expected:

- `gdtr_base`: 64 bits
- `gdtr_limit`: 16 bits
- `ldtr_base`: 64 bits
- `ldtr_limit`: 24 bits
- `idtr_base`: 64 bits
- `idtr_limit`: 16 bits
- `tr_base`: 64 bits
- `tr_limit`: 24 bits
- `cs_shadow`: shadow registers format is the same as segment descriptors' (64 bits)
- `ds_shadow`
- `es_shadow`
- `ss_shadow`
- `fs_shadow`
- `gs_shadow`
- `cr0`: 64 bits
- `cr3`: 64 bits
- `cr4`: 64 bits
- `msr_c0000080`: EFER MSR (64 bits)
- `msr_c0000101`: GS_BASE (64 bits)
- `msr_c0000100`: FS_BASE (64 bits)
- `pkru`: 32 bits
- `eflags`: : 32 bits. Note `rflags` is not supported, as the upper 32 bits are unused.

### Optional register list

So far, all general purpose registers are optional, and follow a rather logical lower-case naming scheme: `rax`, `rbx`, `pkru`, etc.

Here is the comprehensive list:

- Amd64 registers: `rax`, `rcx`, `rdx`, `rbx`, `rsp`, `rbp`, `rsi`, `rdi`, `r8`, `r9`,  `r10`, `r11`,  `r12`,  `r13`,  `r14`,  `r15`,  `rip`, `cs`, `ds`, `es`, `ss`, `fs`, `gs`, `cr0`, `cr1`, `cr2`, `cr3`, `cr4`, `cr5`, `cr6`, `cr7`, `cr8`, `cr9`, `cr10`, `cr11`, `cr12`, `cr13`, `cr14`, `cr15`, `dr0`, `dr1`, `dr2`, `dr3`, `dr4`, `dr5`, `dr6`, `dr7`, `dr8`, `dr9`, `dr10`, `dr11`, `dr12`, `dr13`, `dr14`, `dr15`,
- x87 FPU and MMX registers: `x87r0`, `x87r1`, `x87r2`, `x87r3`, `x87r4`, `x87r5`, `x87r6`, `x87r7`, `top`, `x87status`,`x87control`, `x87tags`, `x87opcode`, `x87lastcs`, `x87lastip`, `x87lastds`, `x87lastdp`
- SSE registers: `zmm0`, `zmm1`, `zmm2`, `zmm3`, `zmm4`, `zmm5`, `zmm6`, `zmm7`, `zmm8`, `zmm9`, `zmm10`, `zmm11`, `zmm12`, `zmm13`, `zmm14`, `zmm15`, `zmm16`, `zmm17`, `zmm18`, `zmm19`, `zmm20`, `zmm21`, `zmm22`, `zmm23`, `zmm24`, `zmm25`, `zmm26`, `zmm27`, `zmm28`, `zmm29`, `zmm30`, `zmm31`, `mxcsr`,
- MSRs: they have the format `msr_xxxxxxxx` where `xxxxxxxx` is the 8-character, lower-case hexadecimal id of the MSR and are always stored on 64 bits, tsc_aux,

TODO: better specify registers sizes
