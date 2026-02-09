#pragma once

#include "cache/ReplacementIF.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include "cache/LRUReplacement.hpp"
#include <memory>
#include <string>
#include <stdexcept>

namespace olympia
{
    /**
     * @brief Factory class for creating cache replacement policies
     * 
     * Creates and returns Sparta replacement policies based on the policy name.
     * Currently supports:
     * - "TreePLRU" (Tree-based Pseudo-LRU)
     * - "LRU" (Least Recently Used)
     * - "MRU" (Most Recently Used - implemented using LRU's infrastructure)
     */
    class ReplacementFactory {
    public:
        /**
         * @brief Create a replacement policy instance
         * @param policy_name Name of the policy to create ("TreePLRU", "LRU", or "MRU")
         * @param num_ways Number of ways in the cache set
         * @return Unique pointer to the created replacement policy
         * @throws std::invalid_argument if policy name is not recognized
         */
        static std::unique_ptr<sparta::cache::ReplacementIF> selectReplacementPolicy(
            const std::string& policy_name, 
            uint32_t num_ways) 
        {
            if (policy_name == "TreePLRU") {
                return std::unique_ptr<sparta::cache::ReplacementIF>(
                    new sparta::cache::TreePLRUReplacement(num_ways));
            }
            else if (policy_name == "LRU" || policy_name == "MRU") {
                // Both LRU and MRU use the same LRUReplacement class
                // The difference is in how the cache controller uses touchLRU/touchMRU
                return std::unique_ptr<sparta::cache::ReplacementIF>(
                    new sparta::cache::LRUReplacement(num_ways));
            }
            
            throw std::invalid_argument("Unknown replacement policy: " + policy_name + 
                ". Supported policies are: TreePLRU, LRU, MRU");
        }
    };
} // namespace olympia
