// <InstGenerator.hpp> -*- C++ -*-

//!
//! \file InstGenerator.hpp
//! \brief Definition of the CoreModel InstGenerator
//!

#pragma once

#include <string>
#include <memory>

#include "Inst.hpp"
#include "MavisUnit.hpp"
#include "sparta/utils/SpartaAssert.hpp"

#include "stf-inc/stf_inst_reader.hpp"

namespace nlohmann {
    using json = class nlohmann::basic_json<>;
}

namespace olympia
{
    /*
     * \class InstGenerator
     * \brief Instruction generator base class
     *
     * Base class used to fetch an instruction to send down the core
     * pipe.  Possible derivations are STF Instruction reader, JSON
     * generator, etc
     */
    class InstGenerator
    {
    public:
        InstGenerator(MavisType * mavis_facade) : mavis_facade_(mavis_facade) {}
        virtual ~InstGenerator() {}
        virtual InstPtr getNextInst(const sparta::Clock * clk) = 0;
        static std::unique_ptr<InstGenerator> createGenerator(MavisType * mavis_facade,
                                                              const std::string & filename,
                                                              const bool skip_nonuser_mode);
        virtual bool isDone() const = 0;
        virtual void reset(const InstPtr &, const bool) = 0;

    protected:
        MavisType * mavis_facade_ = nullptr;
        uint64_t    unique_id_ = 0;
        uint64_t    program_id_ = 1;
    };

    // Generates instructions from a JSON file
    class JSONInstGenerator : public InstGenerator
    {
    public:
        JSONInstGenerator(MavisType * mavis_facade,
                          const std::string & filename);
        InstPtr getNextInst(const sparta::Clock * clk) override final;

        bool isDone() const override final;
        void reset(const InstPtr &, const bool) override final;


    private:
        std::unique_ptr<nlohmann::json> jobj_;
        uint64_t                        curr_inst_index_ = 0;
        uint64_t                        n_insts_ = 0;
    };

    // Generates instructions from an STF Trace file
    class TraceInstGenerator : public InstGenerator
    {
    public:
        // Creates a TraceInstGenerator with the given mavis facade
        // and filename.  The parameter skip_nonuser_mode allows the
        // trace generator to skip system instructions if present
        TraceInstGenerator(MavisType * mavis_facade,
                           const std::string & filename,
                           const bool skip_nonuser_mode);

        InstPtr getNextInst(const sparta::Clock * clk) override final;

        bool isDone() const override final;
        void reset(const InstPtr &, const bool) override final;
    private:
        std::unique_ptr<stf::STFInstReader> reader_;

        // Always points to the *next* stf inst
        stf::STFInstReader::iterator next_it_;
    };
}
