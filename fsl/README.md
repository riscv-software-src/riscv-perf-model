# Fusion/Fracture Specification Language

# Brief
Repo for FSL Interpreter, FSL API and a fusion implementation

- fsl/api    - API source and stand alone tests
- fsl/docs   - API and interpreter documentation
- fsl/interp - Interpreter source and stand alone tests
- fsl/fusion - Instruction fusion implementation for Olympia
- fsl/test   - Combined API and interpreter tests 

# Submodules

FSL has a copy of the cpm.mavis sub-module, https://github.com/Condor-Performance-Modeling/cpm.mavis
 
# Quick start

```
git clone --recurse-submodules https://github.com/Condor-Performance-Modeling/fsl.git
cd fsl
mkdir release
cd release
cmake ..
make -j32

To make the docs:
make docs

The html documentation will be in fsl/docs/docs_interp and fsl/docs/docs_api.
See below for user guides and references.

```

# Documentation - fsl/docs

Some of the references to docs below are forward looking to future docs. 

```
  docs/md/FSL_USER_REF.md           -- Fusion/Fracture Specification Language
                                       user reference guide, see also the
                                       interp doxygen files

  docs/md/FSL_API_USER_REF.md       -- Fusion/Fracture C++ API user reference,
                                       see also the API doxygen files
                                       (Planned)

  docs/md/FSL_APPLICATION_REF.md    -- A walk through of the Olympia fusion
                                       implementation using FSL.
                                       (Planned)

  docs/md/RISCV_ENCODING_FORMATS.md -- Self created encoding formats.
                                       You should consult the official RV
                                       documents for critical uses.

  docs/docs_interp/index.html       -- Doxygen generated implementation
                                       documents for the interpreter.
                                       Create with CMake.

  docs/docs_api/index.html          -- Doxygen generated implementation
                                       documents for the C++ API.
                                       Create with CMake.
```

# FSL Examples

```
  docs/fsl_examples                 -- FSL syntax examples

  test/interp/syntax_tests          -- Syntax example used as part of the
                                       regression.
```

