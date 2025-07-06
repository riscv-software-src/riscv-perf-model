# 5. Syntax description
## 5.1  Example sequence
This sample input sequence is used to explain the constraint syntax design. Presume this is a sequence which has a fused version. 

```
1.  UID   endcoding  mnem     operands
2.  
3.  0xb   0x76e9     c.lui    x13, +0xffffffffffffa000
4.  0xd   0x0685     c.addi   x13, x13, +0x1
5.  0x1c  0x8d35     c.xor    x10, x10, x13
6.  0xf   0x1542     c.slli   x10, x10, 0x30
7.  0x13  0x9141     c.srli   x10,      0x30
```
With the exception of the UID column, the other columns are typical of objdump. The UID column is the Mavis assigned unique identifier.

## 5.2  Example constraints
This is the set of constraints expressed using the proposed syntax:

```
C1.  The operands in the x13 positions can be any gpr as long as they are all the same gpr. 
C2.  The operands in the x10 positions can be any gpr as long as they are all the same gpr
C3.  The operands in the x10 and x13 positions may not be the same gpr (contrived)
C4.  The operands in the IMM=0x30 positions can be any value as long as they are the same value 
C5.  The value of the immediate in the c.lui instruction is not explicitly constrained
C6.  The value of the immediate in the c.addi instruction is not explicitly constrained
```

## 5.3  Abstracting the operands
The syntax for sequence expression is derived from assembly language syntax. The operand expressions are abstracted to allow using sequences in multiple scenarios. Below is an example, the left column is common assembly syntax. The right column shows the abstracted form.
```
1.  SAMPLE                                  ABSTRACTED OPERANDS
2.  c.lui    x13, +0xffffffffffffa000       c.lui    g1, c1
3.  c.addi   x13, x13, +0x1                 c.addi   g2, g3, c2
4.  c.xor    x10, x10, x13                  c.xor    g4, g5, g6
5.  c.slli   x10, x10, 0x30                 c.slli   g7, g8, c3
6.  c.srli   x10,      0x30                 c.srli   x10,    c4
```

The explicit operands on the left (x13, x10, etc) have been replace with named placeholder variables gN and the immediate constants are cN. 

The constraints are expressed as relationships between the variables. There is an explicit constraint form and a form which makes use of implicit positional constraints.

This syntax declares an anonymous sequence.  A sequence is an ordered list. ‘;’ is an optional statement separator. The sequence order is a principal constraint.
```
1.    sequence seq1 {
2.      c.lui    g1, c1
3.      c.addi   g2, g3, c2
4.      c.xor    g4, g5, g6
5.      c.slli   g7, g8, c3
6.      c.srli   g9,    c4
7.    }
```
Variables are declared as objects of known base types, gpr, csr, fpr, etc, constants are expressed as signed or unsigned with a bit range, s16, u6, etc. Using the case above, declaration of the variables and expressions implementing the constraints is shown below
```
1.  constraints {  
2.    gpr g1,g2,g3,g4,g5,g6,g7,g8,g9
3.    s20 c1
4.    s12 c2
5.    u6  c3,c4
6.  
7.    g1 = g2 = g3 = g6         // This expresses C1
8.    g4 = g5 = g7 = g8 = g9    // C2
9.    g1 != g4                  // C3, the contrived constraint
10.    c3 = c4                  // C4
11.  }                          // C5 and C6 do not require a statement
```

Aspects of the syntax
  -  Sequence and constraint sections can be anonymous or named
    -  In the above sample the constraints are anonymous and the sequence has a declared identifier seq1
  -  The section declaration syntax, sequence{} and constraints{}, is optional
  -  The statements within a sequence must be in the correct order
    -  A sequence with relaxed adjacency requirements uses two key words to identify gaps in the sequence, see Section 6.4
  -  The sequence statements must appear before any of the statements associated with the constraints
  -  The constraints statements must occur before any statements associated with transforms.

Syntax for simple assignment is also proposed. For example:
```
g1 = x13
```

## 5.4  Source using explicit constraints
Below is an example declaration of a fusion operation that uses explicit constraints between operands uses
```
1.  fusion_decl fs1 {
2.    sequence seq {
3.      c.lui    g1, c1
4.      c.addi   g2, g3, c2
5.      c.xor    g4, g5, g6
6.      c.slli   g7, g8, c3
7.      c.srli   g9,    c4
8.    }
9.  
10.    constraints cons {  
11.      gpr g1,g2,g3,g4,g5,g6,g7,g8,g9
12.      s20 c1
13.      s12 c2
14.      u6  c3,c4
15.  
16.      g1 = g2 = g3 = g6       // This expresses C1
17.      g4 = g5 = g7 = g8 = g9  // C2
18.      g1 != g4                // C3, the contrived constraint
19.      c3 = c4                 // C4
20.    }
21.  
22.    transform t1.seq.cons     // Still working on the transform syntax
23.                              // this statement is a reference to an 
24.                              // externally described transform
25.  }
```
## 5.5  Source using positional constraints
The above can be simplified by redefining the variables and using the implicit constraints of the sequence operand positions. G[1,2,3,6] are replaced with g1, g[4,5,7,8,9] are replaced by g2. The result is that the constraints can be simplified as well.
```
1.  fusion_decl fs1 {
2.    sequence {                // Sequence using positional constraints
3.      0xb      g1, c1         // I’ve shown the alternative syntax where
4.      c.addi   g1, g1, c2     // the mnemonic is replaced with the Mavis
5.      c.xor    g2, g2, g1     // UID, for the c.lui equiv function
6.      c.slli   g2, g2, c3     
7.      c.srli   g2,     c3
8.    }
9.  
10.    gpr g1,g2
11.    s20 c1
12.    s12 c2
13.    u6 c3
14.  
15.    //The constraints declarations
16.    g1 != g2                 // C3, the contrived constraint
17.                             // In this version of the syntax the
18.    transform t1             // scope of sequence and constraints is
19.                             // within this fusion_decl
20.  }
```

## 5.6  Using externally defined functions
The final bit of syntax is proposed to allow for sharing of sequences and transforms across groups. Transform sharing is implied above. Transforms, constraints and sequences can be predefined and reused. The proposed syntax for this is:
```
1.  fusion_decl fs1 {
2.    sequence    seq1          // S1/C1 and T1 are defined elsewhere
3.    constraints con1.seq1     // The ‘.’ Is the chaining operator
4.    transform   t1.seq1.con1  // ordering of the chained elements does not 
5.  }                           // matter t1.con1.seq1 is also fine.
```

## 5.7  Common library
Implicit in the source above is inclusion of the common library. This can also be done explicitly using the include keyword. 

The native types are defined in the common library, as are standard operators. This is an example of the common library native type definitions
```
1.  export gpr fpr
2.  
3.  basetype  gpr  x0..x31
4.  basetype  fpr  f0..f31
5.  aliastype gpr  zero,ra,sp,gp,tp,t0..t2,(s0|fp),s1,a0..a7,s2..s11,t3..t6
6.  aliastype fpr  ft0..ft7,fs0,fs1,fa0..fa7,fs2..fs11,ft8..ft11
```

## 5.8  Expressing non-adjacent sequences
Two keywords are used to express two types of adjacency patterns.
  - _req_  this expresses a required gap in the sequence.
  - _opt_ this expresses an optional gap in the sequence

Presume we have this sequence; the operands are not shown:
```
1.  Sequence seq2 {
2.      AAA
3.      _req_    //required gap
4.      BBB
5.      CCC
6.      _opt_    //optional gap
7.      DDD
8.  }
```

Expressing the incoming sequence using lower case,  This sequence will not 
match seq2 because it lacks a required gap between the A position and B 
position.

```
1.  aaa
2.  bbb
3.  ccc
4.  ddd
```

This new sequence will match seq2 ‘xxx’ indicates a sequence element between the A and B positions.

```
1.  aaa
2.  xxx        // an arbitrary instruction in the required position
3.  bbb
4.  ccc
5.  ddd
```

This sequence will also match seq2, it has the required non-adjacent element and an element in the optional space.
```
1.  aaa
2.  xxx       // an arbitrary instruction in the required position
3.  bbb
4.  ccc
5.  yyy       // an arbitrary instruction in the optional position
6.  ddd
```
