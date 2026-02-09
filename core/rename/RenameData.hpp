// <RenameData.hpp> -*- C++ -*-

#pragma once

#include "mavis/OpcodeInfo.h"

#include "InstArchInfo.hpp"
#include "CoreTypes.hpp"
#include "MiscUtils.hpp"

#include <map>
#include <functional>

#include <boost/container/small_vector.hpp>

namespace olympia
{
    static constexpr size_t DEFAULT_NUM_SRCS_ = 2;
    static constexpr size_t DEFAULT_NUM_DESTS_ = 1;

    class RenameData
    {
    public:
        // Operand information
        using OpInfoList = mavis::DecodedInstructionInfo::OpInfoList;

        struct OpInfoWithRegfile
        {
            OpInfoWithRegfile() = default;

            mavis::OperandInfo::OpcodeFieldValueType field_value =
                std::numeric_limits<mavis::OperandInfo::OpcodeFieldValueType>::max();
            mavis::InstMetaData::OperandFieldID field_id =
                mavis::InstMetaData::OperandFieldID::NONE;
            core_types::RegFile reg_file = core_types::RegFile::RF_INVALID;
            bool is_x0 = false;

            OpInfoWithRegfile(const OpInfoList::value_type &);
        };

        template <size_t DefaultSize>
        using OpInfoWithRegfileList =
            boost::container::small_vector<OpInfoWithRegfile, DefaultSize>;

        using SrcOpInfoWithRegfileList = OpInfoWithRegfileList<DEFAULT_NUM_SRCS_>;
        using DestOpInfoWithRegfileList = OpInfoWithRegfileList<DEFAULT_NUM_DESTS_>;

        // A register consists of its value and its register file.
        struct Reg
        {
            uint32_t phys_reg = 0;
            uint32_t prev_dest = std::numeric_limits<uint32_t>::max();
            OpInfoWithRegfile op_info;

            Reg() = default;

            Reg(const uint32_t preg, const OpInfoWithRegfile & opinfo, const uint32_t pv_dest) :
                phys_reg(preg),
                prev_dest(pv_dest),
                op_info(opinfo)
            {
            }

            Reg(const uint32_t preg, const OpInfoWithRegfile & opinfo) :
                Reg(preg, opinfo, std::numeric_limits<uint32_t>::max())
            {
            }

            friend inline std::ostream & operator<<(std::ostream & os, const Reg & reg)
            {
                os << reg.op_info.reg_file << " " << reg.op_info.field_value << " -> "
                   << reg.phys_reg;

                if (reg.prev_dest != std::numeric_limits<uint32_t>::max())
                {
                    os << " from " << reg.prev_dest;
                }

                return os;
            }
        };

        template <size_t DefaultSize>
        using RegList = boost::container::small_vector<Reg, DefaultSize>;

        using SrcRegList = RegList<DEFAULT_NUM_SRCS_>;
        using DestRegList = RegList<DEFAULT_NUM_DESTS_>;

        using SrcRegs = boost::container::small_vector<SrcRegList, core_types::RegFile::N_REGFILES>;
        using DestRegs =
            boost::container::small_vector<DestRegList, core_types::RegFile::N_REGFILES>;

        void addSource(const Reg & source)
        {
            src_[source.op_info.reg_file].emplace_back(source);
            ++num_sources_;
        }

        void addSource(Reg && source)
        {
            src_[source.op_info.reg_file].emplace_back(std::forward<Reg>(source));
            ++num_sources_;
        }

        const SrcRegList & getSourceList(const core_types::RegFile reg_file) const
        {
            return src_[reg_file];
        }

        uint32_t getNumSources() const { return num_sources_; }

        void addDestination(const Reg & destination)
        {
            dest_[destination.op_info.reg_file].emplace_back(destination);
            ++num_dests_;
        }

        void addDestination(Reg && destination)
        {
            dest_[destination.op_info.reg_file].emplace_back(std::forward<Reg>(destination));
            ++num_dests_;
        }

        const DestRegList & getDestList(const core_types::RegFile reg_file) const
        {
            return dest_[reg_file];
        }

        uint32_t getNumDests() const { return num_dests_; }

        // Store data register
        void setDataReg(Reg && data_reg) { data_reg_ = std::move(data_reg); }

        const Reg & getDataReg() const { return data_reg_; }

        void clear(const core_types::RegFile reg_file)
        {
            src_[reg_file].clear();
            dest_[reg_file].clear();
            if (data_reg_.op_info.reg_file == reg_file)
            {
                data_reg_ = Reg();
            }
        }

      private:
        SrcRegs src_{core_types::RegFile::N_REGFILES};
        uint32_t num_sources_ = 0;
        DestRegs dest_{core_types::RegFile::N_REGFILES};
        uint32_t num_dests_ = 0;
        Reg data_reg_;
    };
} // namespace olympia
