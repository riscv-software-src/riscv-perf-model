// <CPU.cpp> -*- C++ -*-

#include "CPU.hpp"

//! \brief Name of this resource. Required by sparta::UnitFactory
constexpr char olympia::CPU::name[];

//! \brief Constructor of this CPU Unit
olympia::CPU::CPU(sparta::TreeNode* node, const olympia::CPU::CPUParameterSet* params) :
    sparta::Unit{node}
{
}

//! \brief Destructor of this CPU Unit
olympia::CPU::~CPU() = default;
