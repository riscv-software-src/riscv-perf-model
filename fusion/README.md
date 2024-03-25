# Fusion API

```
Contact: Jeff Nye
         Condor Computing
         2023.12.12
```

There are (will be) two interfaces provided to support implementation of fusion and fracture transforms in Map/Sparta based models, the examples below will be from the CAM/Olympia implementations.

# Changes

A summary of some recent changes is below. This is an overview.

## From FusionSequence to FusionGroup

\note As the requirements on ordering are relaxed the term 'sequence'
became less accurate. I am switching to 'fusion group' or just group,
but there may be some text/code that still uses the term sequence.

## From Init/Search/ConstraintsTransform to just Transform

\note previously the transform process had multiple functors for
each expected phase of the fusion process. This was restrictive for an
API. There is now a single functor for transform specified within
a FusionGroup and a single functor for the top level fusion operator, see below.

### Similar functor methods in top level

\note The top level fusion operation is wrapped into a single 
functor called Fusion::fusionOperator. This encompasses the complete
fusion operation for a list of Inst::PtrTypes. The default fusion operation
is a simple append. The intent is fusionOperator is overridden with a
user defined method that matches the functor signature, declared in
Fusion.


```

    using FusionFuncType = std::function<void(Fusion &, InstPtrListType &,
                                              InstPtrListType &)>;


    static void defaultFusionOpr(Fusion & inst, InstPtrListType & in,
                                 InstPtrListType & out)
    {
        out.insert(out.end(), in.begin(), in.end());
        in.clear();
    }

```

# Usage of the draft PR

Pull the DPR, build and run the local testbench, create the doxygen docs.

The dpr is on branch jeffnye-gh/fusion_dpr

```
<build the release version of olympia>
cd riscv-perf-model
mkdir release; cd release; cmake .. -DCMAKE_BUILD_TYPE=Release

<build/run fusion>
cd ../fusion
mkdir -p release; cd release; cmake ..
make -j32 regress
make fusion_docs
```

# C++ API and testbench

A C++ API is defined and a baseline set of implementations are provided. 
The implementations are linked to the performance model using the conventional 
callback mechanism. The C++ API is intended for compiled execution, regressions, etc.

The testbench is contained in TestBench, test/testbench.h.

## C++ API summary


 The primary classes in the API are:

```
    Fusion
    FusionGroup (FusionGroupCfg)
    MachineInfo
    FieldExtractor
```

The top level class is Fusion, fusion/fusion.h.  This is the primary
interface to Sparta based decoders.

FusionGroup holds the definition of a fusable group of instructions. FusionGroup
provides functor handles for user defined methods. FusionGroupCfg is a
ctor helper struct/class for FusionGroup.

MachineInfo is a placeholder (currently) for machine microarchitecture information pertinent to fusion and fusion performance modeling.

FieldExtractor is a shim between FusionGroup functors and Mavis. It provides
boolean and field extraction methods. I expect this to change, either remove
the shim or increase the number of FieldExtractor setter/getter methods.

There are multiple input mechanisms intended to simplify implementing
the fusion operation. Currently I am using UID. There will be support
for opcodes, assembly text, a DSL syntax and possibly a Json input format.

UID based fusion is the example in this draft. The others are planned.

## Other classes

The support classes for Fusion are:

```
  FusionContext         fusioncontext.h
  FusionExceptionBase   fusionexceptions.h
  using and typedefs    fusiontypes.h
  RadixTrie             radixtrie.h
```

There is a single context supported in this version of the implementation but
generalization to multiple contexts has placeholders in the source.

FusionExceptionBase is the parent for a a number of fusion/dsl/json/run-time exceptions specific to fusion.

RadixTrie is experimental. It is implemented and provided for performance analysis during development. The currently used container is an unordered\_map 
used for functionality and test development.

## Usage within a Sparta unit

The currently considered procedure for Fusion is an implementation in a 
Sparta unit typically in the Decode. The primary input to the 
fusionOperator() is a form of the InstQueue, sparta::Queue<InstPtr>.
(Other forms such as InstGroup are likely).  An output buffer of similar type is also provided to fusionOperator(). The fusionOperator() performs the
search across the current FusionGroup context and executes the transform
functor in the FusionGroup and places the results as appropriate in the output
queue and modifies the input queue as needed. 

The format of the input/output queues will likely be adjusted or additional
functor signatures will be provided. 

Both the top level and FusionGroup functors can be overridden by the user. The
API provides the functor signature and handle but otherwise the API does
not constrain how the functors  operate.

This gives the necessary generality and extensibility while keeping the
API definition unobtrusive.

The remainder of the code and support classes are template based. 

## Domain specific language

A domain specific language is available, the Fusion/Fracture Specification
Language, FSL, is domcumented in FSL.MD.
