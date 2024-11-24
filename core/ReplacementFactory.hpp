#pragma once

#include "cache/ReplacementIF.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include <memory>
#include <string>

namespace olympia
{
    /**
     * @brief Factory class for creating cache replacement policies
     * 
     * Creates and returns Sparta replacement policies based on the policy name.
     * Currently supports:
     * - "TreePLRU" (default)
     */
    class ReplacementFactory {
    public:
        /**
         * @brief Create a replacement policy instance
         * @param policy_name Name of the policy to create
         * @param num_ways Number of ways in the cache set
         * @return Unique pointer to the created replacement policy
         */
        static std::unique_ptr<sparta::cache::ReplacementIF> selectReplacementPolicy(
            const std::string& policy_name, 
            uint32_t num_ways) 
        {
            if (policy_name == "TreePLRU") 
            {
                return std::unique_ptr<sparta::cache::ReplacementIF>(
                    new sparta::cache::TreePLRUReplacement(num_ways));
            }
            // Add LRU and MRU from sparta here
            
            return std::unique_ptr<sparta::cache::ReplacementIF>(
                new sparta::cache::TreePLRUReplacement(num_ways));
        }
    };
} // namespace olympia
