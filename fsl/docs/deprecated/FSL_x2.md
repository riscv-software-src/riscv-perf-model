# Fusion/Fracture Specification Language 

1. Document History

1. Introduction to FSL
    1. High Level Operation
    1. FSL Example
    1. Is/Is not

1. Language Description
    1. Scope
    1. Keywords
    1. Identifiers
    1. Constants
    1. Operators
        1. Arithmetic Operators
        1. Range Operators
        1. Relational Operators
        1. Logical Operators
        1. Assignment Operators
        1. Concatenation Operator
    1. Comments
    1. Preprocessor Support
        1. Conditional Compilation
        1. Includes
    1. Transform Structure
    1. Transform Specification
    1. Prolog Elements
    1. isa Element
        1. isa Methods
    1. uarch Element
        1. uarch methods
    1. ioput Element
        1. ioput methods
    1. Sequence Clause
        1. Sequence methods
    1. Constraints Clause
        1. Constraints methods
    1. Conversion Clause

1. Tools and Utilities
    1. Condor Fusion API
    1. Olympia Performance Model
    1. Mavis
    1. STF Library

1. Appendix
    1. FSL BNF 
    1. FSL style considerations
        1. Encapsulated Style
        1. Library Style
    1. FSL Instruction Types
    1. Syntax Highlight Control File
        1. VIM
1. References

----------------------------------------------------------------
### Document History

| Version |    Date    |  Contact  | Description                           |
|:-------:|:----------:|:---------:|:--------------------------------------|
|  x.0    | 2024.03.04 | Jeff Nye  | circulated for comment
|  x.1    | 2024.03.08 | Jeff Nye  | typos, replace asserts with exceptions, grammar changes
|  x.2    | 2024.03.10 | Jeff Nye  | removed operators, div/mod not needed,++/--/extensive assignment operators violate SA

### TODO

- Initiate some discussion on the copy-on-write vs in-place modification of the
instruction buffers. The goal is to support both without needing it reflected
in the conversion clause syntax.

- Revisit the single assignment decisions

- Revisit the decision to include instr types

- Revisit the decision on minimal control flow. Is the ternary operator
sufficient or is if-then-else syntax more clear, are for loops required,
etc?

- add the BNF or ANTRL/Bison grammar

----------------------------------------------------------------
# Introduction to FSL

Fusion/Fracture Specification Language is used to express properties,
constraints and a process for conversion of standard microprocessor
instructions into alternative instruction forms. 

Generally, this transformation process converts an N-tuple of processor
instructions to an M-tuple of processor instructions. 

The most commonly referenced transforms are fusion and fracture.

When N > M holds, this is the case for fusion. 

When N < M holds, this is the case for fracture. 

Informally, fusion converts simpler operations into more complex operations
with the intent of improving execution attributes. Fracture commonly replaces
more complex operations with a set of simpler operations, with the 
same intent of improving execution attributes.

Often N-tuple instructions are elements of a standardized ISA and
the M-tuple are customizations of the N-tuple set.  The M-tuple
instructions are the result of transformation and are internal to the
processor. 

FSL assists the expression of the properties of these transformations and
takes advantage of the bounds of the domain to provide an efficient syntax
for these expressions.

## High Level Operation

Each N to M tuple mapping is expressed in a named FSL transform specification.

The input to a transform is a sequence of instructions. The output of 
a transform is a possibly transformed sequence. The input sequence can
be modified in place or into a separate buffer. This is controlled by 
FSL syntax.

The operation of an FSL transform consists of a matching phase,
a constraints checking phase, and the conversion phase.  The transform 
phases are represented by three clauses in an FSL transform specification. 

A phase returns an indication of success or failure. If a phase reports
a failure, control is returned to the Fusion API. The API will continue
processing and notify the system as required.

The sequence clause declares the matching attributes to be applied to 
the input sequence. The matching attributes are a list of
instruction objects, either as assembly language or as a list of UIDs.

The output of the sequence clause indicates which instructions
are to be transformed, if any. If no match is found the sequence clause
returns fail.

In the simplest case, a positive match is indicated by an index and a length.
The index is the position of the first matching instruction in the input
sequence, length is the number of instructions to be transformed, inclusive of
the index.

A valid sequence indication is passed to the constraints clause. Subsequently,
the constraints specification is applied to the indicated range and a 
true/false flag is returned.

If the constraints are satisfied, the constraints clause passes true to the
conversion clause, along with the sequence indication.

If the conversion clause receives true, it will apply the transformation
on the input sequence beginning at the specified index and places the result 
in an output sequence or modifies the input sequence in place as directed
by syntax.

## FSL Example

An example of an FSL transform specification written in the encapsulated
style is shown below.

The three clauses, sequence, constraints and conversion, are preceded by
the variables which declare the instruction set architecture, 
the micro-architecture of the processor and a 
sequence container reference, ioput. ioput links the transform to the
performance model's decoded instructions containers.

The isa/uarch/ioput are referred to as the 'Prolog' in discussions
below.

```
transform fs1 {

  isa   rv64g
  uarch oly1
  ioput iop1

  sequence seq1(iop1,rv64g) {
    c.lui    g1, c1
    c.addi   g1, g1, c2
    _req_ 2
    c.xor    g2, g2, g1
    c.slli   g2, g2, c3
    _opt_ 1
    c.srli   g2,     c3
  }

  constraints cons1(iop1,seq1,rv64g,oly1) {
    gpr g1,g2
    s20 c1
    s12 c2
    u6  c3

    g1 != g2
  }

  conversion conv1(iop1,seq1,cons1) {
    instr fused(opcode=0x1234)
    iop1.input.replace(seq1,fused)
  }
}
```

## Is/Is not

FSL is not a programming language; it is a "constrained
transformation expression language". FSL operates on stylized or symbolic
microprocessor instructions.

As such many aspects of a programming language are not required by FSL.

The FSL syntax style is a reduced boilerplate style of C. e.g. braces are 
used, no semi-colons, indentation is not a syntax element.

There are no user defined functions.

There are a limited number of data types unique to FSL.

The standard native types are not in the syntax.

For user defined variables type assignment is done my inference.

This is no need for string or container types.  The typical native
types such as float, double, and string are not required by FSL and 
therefore not inferred.

FSL uses single assignment for variables. When using object methods
multiple assignments with the same method is supported.

There are no console or file I/O mechanisms.

Expression operations are limited.

Arithmetic expressions are limited.

const'ness is not a feature of FSL.

There are no namespaces.

----------------------------------------------------------------
# Language Description

Note: This is the current state of the definition. The design choices 
made for simplicity may change as the definition matures.

## Scope

Note: I'd like to keep the scoping requirements to a minimum. Time will 
tell if this is practical.

There is global scope for named tranforms and a local scoping 
convention.  Variable scoping is lexical.

Anonymous block scoping, { ...scope... }, is not necessary for FSL operation.

## Keywords

The set of keywords is small. Some objects have process/method names
where legal use is indicated by context and are not keywords.

```
_req_       _opt_       _pass_        _fail_      emit
transform   constraints sequence      conversion  
ioput       isa         uarch         iword
instr       encoding    encode_order  instr
csr         fpr         gpr           vvr
```

## Identifiers

Legal variable names are conventional. 

```
^[a-zA-Z_][a-zA-Z0-9_]*$
```

----------------------------------------------------------------
## Constants

The constraints and conversion clauses are the target use cases for 
defining constant support in FSL.

Constants are expressed as decimal, hexadecimal or the verilog style.

```
123
0x456
8'b10101010
```

For Verilog style constants the '?' value indicates a match all or
don't care element. The width of '?' is relative to the width of 
native elements within the constant's base.

```
 8'b1010101?      bit 0 is a don't care
12'h45?           bits [3:0] are a don't care
```

----------------------------------------------------------------
## Operators

The constraints clause is the target use case for defining operator
support in FSL.

The list of operators and their support in FSL is listed below. FSL adds
the range operator to the standard C style operators. The syntax is 
similar to Verilog.

These operators are unnecessary in FSL: Divide, modulus, size_of,
pointer operations, type conversions cast/implicit/explicit.

The comma operator support is limited to concatentaion expressions, operation
chaining is not necessary in FSL.

Within the supported operators FSL uses conventional operator precedence.


### Arithmetic Operators

```
    + (Addition)
    - (Subtraction)
    * (Multiplication)
```

### Range Operators

```
    [M:N] (range select)
    [M]   (index)
```

### Relational Operators

```
    ==  (Equal to)
    !=  (Not equal to)
    >   (Greater than)
    <   (Less than)
    >=  (Greater than or equal to)
    <=  (Less than or equal to)
```

### Logical Operators

```
    &&  (Logical AND)
    ||  (Logical OR)
    !   (Logical NOT)
```


### Assignment Operator

```
    =    (Simple assignment)
```


### Concatenation Operator

Concatenation expressions use a style similar to verilog, {a,b,c};

```
  xyz = { 3'b00, 4'h3,8'd10 }
  abc = { a, b, c }
  
```

## Comments

Comments use '# ' (hash/space) for single line comments or '###' as 
the delimiter for comment blocks. Comments can be embedded in other comments.

```
#  ### commenting out
#  my multi line
#  comment by
#  prefixing the '#'
#  end of ML comment ###
```

Mixing block and line comments is supported, the first comment token
takes precedence. This practice is not recommended.

```
#  ### commenting out
#  my multi line
   this line will be processed
#  prefixing the '#'
#  end of ML comment ###
```

In this case the block comment start and stop markers are not processed 
because they are delimited by line comments.  This is supported but 'this
line will be processed' will be tokenized and tested for legal grammar. 

## Preprocessor Support

The FSL parser uses a pre-processor with features similar to that found
in the C pre-processor.  Within the constraints of the FSL language most 
pre-processor operations found in C will be available in FSL with slight
changes for FSL syntax choices.
 
Note that the typical # indicator, as in #include, is replaced with
'`' as in `include.

### Conditional Compilation

FSL supports a common set of conditional compilation directives, but 
with the typical # replaced by back-tick.

```
`ifdef X
`ifndef X
`if X == Y

`endif
`endif
`endif
```

### Includes

The include keyword declares external files. The parser handles 
included files by expanding them into the current position in
the current translation unit. Include recursion is illegal.

The syntax is:

`include myfile.fh

The .fh extension is preferred but not enforced by the parser. Quotes
around the file name are not used.

The single inclusion pragma is declared as:

`once

This pragma must be expressed before any other non-comment statement.

## Transform Structure

FSL transform specifications are read at runtime. The FSL file 
list is passed to the Fusion API constructor. The API invokes the
FSL parser.

A transform's internal structure consists of a number of clauses,
the sequence, constraints, and conversion clauses, along with a number
elements in the prolog which specify the context of the fusion operation,
ISA, microarchitecture and hooks into the performance model.

## Transform Specification 

The transform statement is a top level structure.  Each transform statement
is named. The name must be unique across all translation units.

The generic form is shown below. The order of the sections and clauses
is arbitrary. What is shown is the recommended convention.

```
transform <name> {
  <prolog section>
  <sequence clause>
  <constraints clause>
  <conversion clause>
}
```

## Prolog Elements

Prolog is not a keyword. The prolog elements provide the necessary 
context for the transform operation and they are commonly referred to 
as a group.  The common elements are shown:

```
  isa    myIsa
  uarch  myImplementation
  ioput  iop1
```

Following parsing/interpretation of the FSL files, the API will walk through
each transform specification to determine its pertinence to the current
context. 

The prolog elements are used for this purpose. The elements
of isa, uarch and ioput are matched to references in the Fusion API.
Unmatched references cause the API to throw an exception.

## isa Element

The isa element name is used by the Fusion API to match an instruction set
description API to the transform specification. The API looks up the 
'myIsa' string to find the associated object pointer.

The Fusion API will throw an exception if the named isa element does not have an
associated object.

The ISA description API is used to validate instruction references in
the transform specification.

Mavis is the instruction set description API supported in this release.

### isa Methods

The isa object has no internal methods accessible through FSL syntax.
Operations which use the isa element do so implicitly during FSL
parsing.

## uarch Element

The uarch element name is used by the Fusion API to match a micro-architecture
description class instance to the transform specification.

The Fusion API will throw an exception if the named uarch element does not 
have an associated object.

The microarchitecture description class is used to validate processor
implementation queries made in the constraints and conversion clauses.

### uarch methods

The uarch methods are documented in MachineInfo.hpp as part of the
Fusion API doxygen documentation.

## ioput Element

The Fusion API links references to C++ buffers containing instruction
representations to the FSL parser. This linkage is done through a similar
name matching scheme as the other prolog elements.

ioput is the keyword. Within ioput are two objects representing two
instruction buffers, by name they are simply .input and .output.

During construction the Fusion API maps ioput.input and ioput.output
to the appropriate model containers. 'input' and 'output' are indexes
into a map containing references to the model's instruction buffers.

The Fusion API will throw an exception if it can not match the elements in
ioput to STL container objects.  Using Olympia as an example, the Fusion 
API will attempt to match the specified name, ioput.input, to a 
Fusion::InstQueue reference.

Note: The FSL syntax implies a copy-on-write idiom. However by mapping the
ioput.input and ioput.output objects to the same buffer a modification
in-place idiom can be implemented. This isolates the style used in
the conversion clause from these external mechanics.

### ioput methods

ioput elements have these methods available. 

```
.clear()                 empties the associated container
.erase(index,size)       removes size elements begining with index,
                         size is inclusive of the index position.
.insert(index,instr)     insert an instr object before index. 
                         instr is an FSL type
.replace(sequence,instr) removes sequence.length objects beginning
                         at sequence.index, and inserts instr 
```

## Sequence clause

The sequence clause is used to match the incoming instructions to a
known transform. 

The arguments to the sequence clause are the input sequence name and
the Mavis facade name, declared using ioput and isa, respectively.

The ioput and isa objects give the sequence clause access to the 
instruction containers and known instruction definitions. This
access is used for validation of the sequence and for matching.

The sequence clause implicitly uses the 1st element of ioput, the 
input container. The sequence clause does not modify the ioput containers.

```
ioput iop1

sequence seq1(iop1,myISA) {
   # sequence of instructions
}
```

The FSL parser uses the Fusion API and access to the isa object to
validate the instruction sequence. An exception is thrown if a problem
is detected.

The specified sequence of instructions is pattern matched against 
the contents of the input container.  If a match is detected the sequence
object, seq1, will update its internal index field and length field. This
information is passed to the constraints clause. On a match the sequence
clause returns \_pass\_ to the Fusion API.

The index field is the zero-referenced position of the start of the match 
within ioput. The length is the number of instructions, inclusive of the
starting instruction.

An instruction sequence is expressed as a simple list of Mavis-assigned
unique identifiers, or as a set of assembly language instructions with
abstracted operands. The choice is based on the constraints.

The UID list is sufficient if there are no constraints on the operands
in the list of instructions in the sequence.

For example, these two instructions are a frequent sequence in compiled
C.  The left column is the encoding, the comment is the Mavis unique identifier 
for each instruction which is independent of the operands.

```
0x0512     c.slli x10,4    ; 0xf
0x8111     c.srli x10,4    ; 0x13
```

If you do not require constraints on the operands and therefore have no
need to refer to the operands by name, you can simply specify the UIDs 
as a list, one per line.

```
ioput iop1

sequence seq1(iop1,rv64g) {
  0xf
  0x13
}
```

With the UID sequence expression there are no operand specific 
constraints and therefore the conversion clause must handle any legal
combinations of the two rd and two constants.

This results in a transform to a generalized fusion op of a shift left 
followed by right and not the zero extension expressed by x10,4.

If the constraints clause implements operand contraints the instruction
sequence should be expressed using the abstract assembly syntax so the
operands can be referenced by name.

The expression below takes advantage of positional constraints:

```
ioput iop1

sequence seq1(iop1,rv64g) {
  c.slli g1,c1
  c.srli g1,c1
}
```

In this case the operands labeled g1 are, by association, required to be
the same value, similarly with the constant operands.

Another option would be to fully enumerate the operands and let the
constraints clause enforce the required limits on operands.

```
ioput iop1

sequence seq1(iop1,rv64g) {
  c.slli g1,c1
  c.srli g2,c2
}
```

Note both the later two cases can be made equivalent by construction of
the equivalent constraints in the clause.

Now in both cases the transformation expresses a fusion operation for
zero extention of the common rd register by the common constant.

```
g1 = g2
c1 = c2
```

In some cases positional constraints can save development time at no 
expense to performance.

The example sequences above have an implied strict ordering. There are 
sequence pragmas to relax the strictness of sequence matching. 

The required pragma, \_req\_, indicates that an unspecified instruction
is required in that position. The \_req\_ case does not constrain 
what instruction can be present in the gap. A trailing integer 
can specify more than one required instruction.

The optional pragma, \_opt\_, indicates that the match will not be 
rejected if there is an instruction or not in this position. A
trailing integer can be used to specify up to N optional instructions.

```
_opt_ 2    indicates 0 - 2 instructions will match
_req_ 2    indicates there must be 2 instructions in this position
           in the sequence. 
```

The trailing integer is optional in the syntax:
```
_opt_ and _opt_ 1 are equivalent
_req_ and _req_ 1 are equivalent
```

```
ioput iop1

sequence seq1(iop1,rv64g) {
  c.lui    g1, c1
  c.addi   g1, g1, c2
  _req_ 2
  c.xor    g2, g2, g1
  c.slli   g2, g2, c3
  _opt_ 2
  c.srli   g2,     c3
}
```

Notes: The sequence pragmas are also supported for the UID case.  Mixing UID 
and abstract operand sequences is supported. 

### Sequence methods

sequence objects have implicit data members

```
.state        This is _pass_ or _fail_ match status
.length       number of matching instructions, if non-zero this is
              the number of UIDs or instruction object epxress in the
              sequence. length is 0 if no match is found.
 
.index        if length is non-zero this index of the first matching
              instruction
```

## Constraints clause

The constraints clause defines relationships between operands and the
machine implementation.

The sequence clause is provided to give the constraints access to 
the operand abstractions and UID list. The ioput object is supplied
so the constraints clause can query the encoding of the instructions in
the input buffer. The isa object is supplied to validate field widths etc
used in the constraints declarations. Finally the uarch object provides
the constraints clause access to machine implementatain details.

When the sequence object contains only UIDs there are no operand
specific constraints, and the constraints clause can simply return
\_pass\_.

```
constraints cons1() {
  _pass_
}
```

Having a populated argument list in this case is optional.
But if the arguments to the constraints clause is empty
the parser will expect only a \_pass\_ or (a \_fail\_) statement. 
Anything else indicates a programming error.

This is a contrived example for explanation.

```
ioput iop1

sequence seq1(iop1,rv64g) {
  c.slli g1,c1
  c.srli g2,c2
}

constraints cons1(seq1,iop1,rv64g,oly1) {
  gpr g1,g2
  u6  c1,c2

  g1 != g2
  c1[0] == c2[1]

}
```

Variables beginning with s or u are signed/unsigned constants.
The number in the name is the bit width of the constant.

gpr is a keyword, the signed and unsigned designators are treated
as keywords.

When performing comparisons on register operands the comparison
is performed on the index of the operand and not the contents
of the operand. The inequality statement, g1 != g2, is testing that 
g1 and g2 are not the same register.

The \_pass\_ or \_fail\_ pragmas are implicit. If any statement is
false \_fail\_ is returned. If the clause is processed to the end
\_pass\_ is returned.

For error checking any named operand in the sequence clause must
have a declaration in the constraints clause even if no operation
uses that declaration.

```
ioput iop1

sequence seq1(iop1,rv64g) {
  c.lui    g1, c1   # c1 is s6
  c.addi   g1, c2   # c2 is s6
  c.xor    g2, g1
  c.slli   g2, c3   # c3 is u6
  c.srli   g2, c3   # c3 is u6
}
```

As an example for this sequence the declared constraints clause 
will return \_pass\_.

```
ioput iop1

constraints cons1(seq1,iop1,rv64g,oly1) {
  gpr g1,g2
  u6  c1,c2,c3
}
```

However this example will fail, because c3 is not declared.

```
ioput iop1

constraints cons1(seq1,iop1,rv64g,oly1) {
  gpr g1,g2
  u6  c1,c2   # _fail_ because no c3
}
```

Unused declarations also fail. This is considered a programming
aberration that should be reported to the developer.

When specifying constraints on register operands the constraints
themselves are verified against the ISA. Constraints that violate
the requirements of the ISA are failed.

```
ioput iop1

sequence seq1(iop1,rv64g) {
  c.slli g1,c1
  c.srli g1,c1
}

constraints cons1(seq1,iop1,rv64g,oly1) {
  gpr g1,g2
  u6  c1,c2

  g1 == 0      # _fail_ the rd of c.slli/c.srli can not be x0
}
```

Being strict with operand declarations is intended to assist 
development by highlighting unexpected code construction.
TODO: is this having the desired effect?

### Constraints methods

The constraints clause has a single method.

```
.state        This is _pass_ or _fail_ constraints status
```

In addition to the .state method, the constraints clause declarations
are made available without prefix to the conversion clause. This is
described in the next section.

### Conversion clause

The conversion clause performs the instruction morphing to create
the new instruction(s). 

The conversion clause takes a reference to the sequence clause for 
access to the named operands or for access to the UID lists.

The conversion clause takes a reference to the ioput object for
reading the input instruction list as a destination for converted
instructions.

The constraints clause is provided to the conversion clause for 
access to operand relationships and types.

The conversion clause can modify the ioput object's input field 
in place with converted instructions or it can place modified
instructions in the output field of the ioput object.

instr objects are used to hold the newly created instructions.  The 
annotation fields for a new instruction are assigned to the instr
object.

In the example presume the sequence match has returned an index of 2 and a
length of 3. In this conversion the 3 unfused instructions are replaced
by 1 fusion created instruction.

The new instruction attributes are assigned. The ioput container
is modifed to remove the unfused instructions and insert the
fusion replacement.

```
ioput iop1

conversion conv1(seq1,iop1,cons1) {
  instr newInstr

  newInstr(opc=0x1234,uid=0x3)
  newInstr(src={g1},dst={g2},imm={c1,c1})
  newInstr(type="fused")

  iop1.input.replace(seq1,newInstr)
}
```

There are a large number of fields available in the instr object. Not
all are required by all models. Inclusion of these fields is implementation
dependent.

### Conversion methods

The conversion clause has a single method.

```
.state        This is _pass_ or _fail_ constraints status
```

The instr type is valid within a conversion clause.

----------------------------------------------------------------
# Tools and Utilities

A summary of tools and utilities useful for instruction transform work. 

## Condor Fusion API

FSL domain specific language is a component of a larger C++ API. With this API
it is possible to directly create instruction transforms in C++, or through
the FSL domain specific language or other ad hoc schemes. The API has 
been tested with the Condor Performance Model and the Olympia 
performance model.

The Fusion API shares a repository with the Olympia performance model.  The
shared URL is contained here:
https://github.com/riscv-software-src/riscv-perf-model/tree/master/fusion.
Doxygen documentation is available for this API.

## Olympia Performance Model

Olympia is a performance model written in C++ for the RISC-V community as an 
example of an Out-of-Order RISC-V CPU Performance Model based on the Sparta 
Modeling Framework.

The repository URL is https://github.com/riscv-software-src/riscv-perf-model/tree/master.

## Mavis 

Mavis is a header only framework designed for decoding the RISC-V ISA into 
custom instruction class types, along with custom extensions to those 
class types.

The repository URL is https://github.com/sparcians/mavis/tree/main. Doxygen
documentation is available for this API.

## STF Library 

The STF (Simulation Tracing Format) library provides an API for 
reading and generation of STF trace files.

The repository URL is https://github.com/sparcians/stf_lib. Doxygen
documentation is available for this API.

----------------------------------------------------------------
# Appendix

## FSL BNF

## FSL style considerations

There are two styles of FSL shown, the first shows an encapsulated
style, the second shows a library element style.

Subjectively I think the first style is easier to comprehend and the second
style is where large implementations will likely migrate, because of the
opportunities for reuse.

In operation there is no difference in the two examples.

### Encapsulated Style
```
transform fs1 {

  isa   rv64g
  uarch oly1
  ioput iop1

  sequence seq1(iop1,rv64g) {
    c.lui    g1, c1
    c.addi   g1, g1, c2
    _req_
    c.xor    g2, g2, g1
    c.slli   g2, g2, c3
    _opt_
    c.srli   g2,     c3
  }

  constraints cons1(seq1,iop1,rv64g,oly1) {
    gpr g1,g2
    s20 c1
    s12 c2
    u6  c3

    g1 != g2
  }

  conversion conv1(seq1,iop1,cons1) {
    instr fused(opcode=0x1234)
    iop1.input.replace(seq1,fused)
  }
}
```

### Library Style

transform fs1 {
  prolog prolog1
  sequence seq1
  constraints cons1
  conversion conv1
}

prolog prolog1 {
  isa   rv64g
  uarch oly1
  ioput iop1
}

sequence seq1(iop1,rv64g) {
  c.lui    g1, c1
  c.addi   g1, g1, c2
  _req_
  c.xor    g2, g2, g1
  c.slli   g2, g2, c3
  _opt_
  c.srli   g2,     c3
}

constraints cons1(seq1,iop1,rv64g,oly1) {
  gpr g1,g2
  s20 c1
  s12 c2
  u6  c3

  g1 != g2
}

conversion conv1(seq1,iop1,cons1) {
  instr fused(opcode=0x1234)
  iop1.input.replace(seq1,fused)
}

conversion conv2(seq1,iop1,cons1) {
  encoding word1(seq1,opc=0x1234) {
    u10 opc    # 57:48   unsigned 10b
    u6  c3     # 47:42   unsigned 6b
    s12 c2     # 41:30   signed 12b
    s20 c1     # 29:10   signed 20b
    gpr g1     # 9:5     gpr 5b
    gpr g2     # 4:0     gpr 5b
    encode_order{opc,c3,c2,c1,g1,g2}
  }

  instr fused(encoding=word1)
  iop1.input.replace(seq1,fused)
}

## FSL Instruction Types

Instruction types have a mapping to the ISA description API. The
lower case strings here are mapped to an equivalent type in the ISA
API. When assigning a type to an FSL instr object use the lower
case strings below, without quotes.

```
"int"                INT
"float"              FLOAT
"arith"              ARITH
"mul"                MULTIPLY
"div"                DIVIDE
"branch"             BRANCH
"pc"                 PC
"cond"               CONDITIONAL
"jal"                JAL
"jalr"               JALR
"load"               LOAD
"store"              STORE
"mac"                MAC
"sqrt"               SQRT
"convert"            CONVERT
"compare"            COMPARE
"move"               MOVE
"classify"           CLASSIFY
"vector"             VECTOR
"maskable"           MASKABLE
"unit_stride"        UNIT_STRIDE
"stride"             STRIDE
"ordered_indexed"    ORDERED_INDEXED
"unordered_indexed"  UNORDERED_INDEXED
"segment"            SEGMENT
"faultfirst"         FAULTFIRST
"whole"              WHOLE
"mask"               MASK
"widening"           WIDENING
"hypervisor"         HYPERVISOR
"crypto"             CRYPTO
"prefetch"           PREFETCH
"ntl"                NTL
"hint"               HINT
"cache"              CACHE
"atomic"             ATOMIC
"fence"              FENCE
"system"             SYSTEM
"csr"                CSR
```

## Syntax Highlight Control Files

### VIM
```
" Vim syntax file
" Language:     Fusion/Fracture Specification Language
" Maintainer:   Jeff Nye
" Last Update:  2024

" For version 6.x: Quit when a syntax file was already loaded
if version < 600
   syntax clear
elseif exists("b:current_syntax")
   finish
endif

" Set the local value of the 'iskeyword' option.
if version >= 600
   setlocal iskeyword=@,48-57,63,_,192-255
else
   set iskeyword=@,48-57,63,_,192-255
endif

" Rules
syn keyword fslKeyword  transform sequence constraints emit conversion
syn keyword fslKeyword  isa uarch ioput 
syn keyword fslSequence _req_ _opt_ _pass_ _fail_

syn match   fslFunction /\w\+\ze\s*(/

syn keyword fslType  fpr vvr gpr csr opc instr encoding encode_order iword
syn match   fslType  "\<[su]\d\+\>"

syn keyword fslTODO contained TODO FIXME

syn match   fslOperator "[&|~><!)(*%@+/=?:;}{,.\^\-\[\]]"

syn region  fslComment   start="###" end="###" contains=verilogTodo,@Spell
syn match   fslComment   "# .*" contains=fslTODO,@Spell

syn match   fslFileName "[a-zA-Z_]+[a-zA-Z0-9_]*\.(?:fsl|fslh|fh)"
syn match   fslInCline  "`include .*" contains=fslFileName
syn match   fslString   /"[^"]*"/

syn match   fslDirective "`if"
syn match   fslDirective "`else"
syn match   fslDirective "`define"
syn match   fslDirective "`endif"
syn match   fslDirective "`ifdef"
syn match   fslDirective "`ifndef"
syn match   fslDirective "`once"

syn match   fslHexNumber  "\<0x\x\+\>"
syn match   fslVlogNumber "\(\<\d\+\|\)'[sS]\?[bB]\s*[0-1_xXzZ?]\+\>"
syn match   fslVlogNumber "\(\<\d\+\|\)'[sS]\?[oO]\s*[0-7_xXzZ?]\+\>"
syn match   fslVlogNumber "\(\<\d\+\|\)'[sS]\?[dD]\s*[0-9_xXzZ?]\+\>"
syn match   fslVlogNumber "\(\<\d\+\|\)'[sS]\?[hH]\s*[0-9a-fA-F_xXzZ?]\+\>"
syn match   fslVlogNumber "\<[+-]\=[0-9_]\+\(\.[0-9_]*\|\)\(e[0-9_]*\|\)\>"

"Modify the following as needed.  The trade-off is performance versus
"functionality.

syn sync minlines=50

hi def link fslKeyword         Statement
hi def link fslType            FslType
hi def link fslSequence        Label
hi def link fslDirective       Label

hi def link fslTODO            Todo
hi def link fslComment         Comment
hi def link fslFunction        Function

hi def link fslString          String

hi def link fslHexNumber       Number
hi def link fslVlogNumber      Number

hi def link fslOperator        Special

hi def link fslIncline         SpecialComment
hi def link fslFileName        ThisType

highlight FslType ctermfg=2 gui=bold guifg=SeaGreen

let b:current_syntax = "fsl"
```

1. References

[1] Celio, Christopher, et al. "The renewed case for the reduced instruction
    set computer: Avoiding isa bloat with macro-op fusion for risc-v." arXiv
    preprint arXiv:1607.02318 (2016).
