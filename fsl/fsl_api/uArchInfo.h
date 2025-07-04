//
// Created by David Murrell on 12/2/19.
//
#pragma once
#include "json.hpp"
#include "fsl_api/uArchInfoExceptions.h"
#include "mavis/DecoderExceptions.h"
#include "mavis/DecoderTypes.h"

#include <memory>
#include <ostream>
#include <iostream>
#include <string>

/**
 * uArchInfo: encapsulates "static" u-arch specific information
 */
class uArchInfo {
public:
  typedef std::shared_ptr<uArchInfo> PtrType;

  // TODO: flesh this out
  enum class UnitSet : uint64_t {
    AGU = 1ull << 0,
    INT = 1ull << 1,
    FLOAT = 1ull << 2,
    MULTIPLY = 1ull << 3,
    DIVIDE = 1ull << 4,
    BRANCH = 1ull << 5,
    LOAD = 1ull << 6,
    STORE = 1ull << 7,
    SYSTEM = 1ull << 8,
    VECTOR = 1ull << 9,
  };

  // BEGIN: Stuff imported from the old StaticInstructionData object
  enum RegFile { INTEGER, FLOAT, INVALID, N_REGFILES = INVALID };

  static inline const char *const regfile_names[] = {"integer", "float"};

  // TODO: resolve this against the UnitSet
  enum IssueTarget : std::uint16_t {
    IEX,
    FEX,
    BR,
    LSU,
    ROB, // Instructions that go right to retire
    N_ISSUE_TARGETS
  };

  static constexpr uint32_t MAX_ARCH_REGS = 5;
  // END: Stuff imported from the old StaticInstructionData object

private:
  // Map unit names to unit ID's
  static inline std::map<std::string, UnitSet> umap_ = {
      {"agu", UnitSet::AGU},       {"int", UnitSet::INT},
      {"float", UnitSet::FLOAT},   {"mul", UnitSet::MULTIPLY},
      {"div", UnitSet::DIVIDE},    {"branch", UnitSet::BRANCH},
      {"load", UnitSet::LOAD},     {"store", UnitSet::STORE},
      {"system", UnitSet::SYSTEM}, {"vector", UnitSet::VECTOR},
  };

  // TEMPORARY: map unit names to TargetUnit for back-level compatibility
  static inline std::map<std::string, IssueTarget> issue_target_map_ = {
      {"int", IssueTarget::IEX},    {"float", IssueTarget::FEX},
      {"branch", IssueTarget::BR},  {"load", IssueTarget::LSU},
      {"store", IssueTarget::LSU},  {"system", IssueTarget::ROB}, // TEMPORARY!
      {"vector", IssueTarget::FEX},                               // TEMPORARY!
      {"rob", IssueTarget::ROB},                                  // TEMPORARY!
  };

public:
  /**
   * \brief This object encapsulates all the micro-architectural information
   * that depends on the instruction type. It is "static" and cached by Mavis in
   * the instruction factories. Mavis will pass the nlohmann::json object to
   * this constructor so that the user can parse any of the desired fields from
   * the u-arch JSON file supplied to Mavis \param jobj nlohmann::json object
   * for the given instruction
   */
  explicit uArchInfo(const nlohmann::json &jobj) { parse_(jobj); }

  uArchInfo() = default;
  uArchInfo(const uArchInfo &) = delete;

  void update(const nlohmann::json &jobj) {
    // TODO: identical to constructor(jobj) for now, but we may want to provide
    // update restrictions
    parse_(jobj);
  }

  bool isUnit(UnitSet u) const {
    return (static_cast<uint64_t>(u) & units_) != 0;
  }

  IssueTarget getIssueTarget() const { return issue_target_; }

  uint32_t getLatency() const { return latency_; }

  bool isPipelined() const { return pipelined_; }

  bool isSerialized() const { return serialize_; }

  bool isROBGrpStart() const { return rob_grp_start_; }

  bool isROBGrpEnd() const { return rob_grp_end_; }

private:
  uint64_t units_ = 0; ///< Bit mask of target execution units (from UnitSet)
  IssueTarget issue_target_ = IssueTarget::N_ISSUE_TARGETS; ///< Issue target
  uint32_t latency_ = 0;       ///< Execution latency
  bool pipelined_ = true;      ///< Pipelined execution (non-blocking)?
  bool serialize_ = false;     ///< Serializes execution?
  bool rob_grp_start_ = false; ///< Starts a new ROB group?
  bool rob_grp_end_ = false;   ///< Ends a ROB group?

private:
  friend std::ostream &operator<<(std::ostream &os, const uArchInfo &ui);
  void print(std::ostream &os) const {
    os << "{units: 0x" << std::hex << units_ << ", lat: " << std::dec
       << latency_ << ", piped: " << pipelined_ << ", serialize: " << serialize_
       << ", ROB group begin: " << rob_grp_start_
       << ", ROB group end: " << rob_grp_end_ << "}";
  }

  /**
   * \brief Parse the JSON file
   * \param jobj
   */
  void parse_(const nlohmann::json &jobj) {
    // Issue target (from IssueTarget)
    if (jobj.find("issue") != jobj.end()) {
      const auto itr = issue_target_map_.find(jobj["issue"]);
      if (itr == issue_target_map_.end()) {
        throw uArchInfoUnknownIssueTarget(jobj["mnemonic"], jobj["issue"]);
      }
      issue_target_ = itr->second;
    }

    // Target execution unit (from UnitSet) -- bit mask allows multiple targets
    if (jobj.find("unit") != jobj.end()) {
      mavis::UnitNameListType ulist =
          jobj["unit"].get<mavis::UnitNameListType>();
      for (const auto &u : ulist) {
        const auto itr = umap_.find(u);
        if (itr == umap_.end()) {
          throw uArchInfoUnknownUnit(jobj["mnemonic"], u);
        }
        units_ |= static_cast<uint64_t>(itr->second);
      }
    }

    // Instruction latency
    if (jobj.find("latency") != jobj.end()) {
      latency_ = jobj["latency"];
    }

    // Whether the instruction is piplined (non-blocking)
    if (jobj.find("pipelined") != jobj.end()) {
      pipelined_ = jobj["pipelined"];
    }

    // Whether the instruction serializes execution
    if (jobj.find("serialize") != jobj.end()) {
      serialize_ = jobj["serialize"];
    }

    // Whether the instruction starts a new ROB group
    if (jobj.find("rob_group") != jobj.end()) {
      mavis::StringListType slist =
          jobj["rob_group"].get<mavis::StringListType>();
      for (const auto &str : slist) {
        if (str == "begin") {
          rob_grp_start_ = true;
        } else if (str == "end") {
          rob_grp_end_ = true;
        } else {
          throw uArchInfoROBGroupParseError(jobj["mnemonic"], str);
        }
      }
    }

    std::cout << "uArchInfo: " << jobj["mnemonic"] << std::endl;
  }
};

inline std::ostream &operator<<(std::ostream &os, const uArchInfo &ui) {
  ui.print(os);
  return os;
}
