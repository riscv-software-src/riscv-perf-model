#pragma once

#include <vector>
#include <cstdint>
#include <cmath>

namespace olympia
{
    namespace BranchPredictor
    {
        class TageTaggedComponentEntry
        {
          public:
            TageTaggedComponentEntry(const uint8_t & tage_ctr_bits, const uint8_t & tage_useful_bits,
                                     const uint8_t & ctr_initial, const uint8_t & useful_initial);

            void incrementCtr();
            void decrementCtr();
            uint8_t getCtr();
            void incrementUseful();
            void decrementUseful();
            uint8_t getUseful();

            uint16_t tag;

          private:
            const uint8_t tage_ctr_bits_;
            const uint8_t tage_useful_bits_;
            const uint8_t tage_ctr_max_val_;
            const uint8_t tage_useful_max_val_;

            uint8_t ctr_;
            uint8_t useful_;
        };

        class TageTaggedComponent
        {
          public:
            TageTaggedComponent(const uint8_t & tage_ctr_bits, const uint8_t & tage_useful_bits,
                                const uint16_t & num_tagged_entry);

            TageTaggedComponentEntry getTageComponentEntryAtIndex(const uint16_t & index);

          private:
            const uint8_t tage_ctr_bits_;
            const uint8_t tage_useful_bits_;
            const uint16_t num_tagged_entry_;
            std::vector<TageTaggedComponentEntry> tage_tagged_component_;
        };

        class TageBIM
        {
          public:
            TageBIM(const uint32_t & tage_bim_table_size, const uint8_t & tage_base_ctr_bits);

            void incrementCtr(const uint32_t & ip);
            void decrementCtr(const uint32_t & ip);
            uint8_t getPrediction(const uint32_t & ip);

          private:
            const uint32_t tage_bim_table_size_;
            const uint8_t tage_bim_ctr_bits_;
            uint8_t tage_bim_max_ctr_;
            std::vector<uint8_t> tage_bimodal_;
        };

        class Tage
        {
          public:
            Tage(const uint32_t & tage_bim_table_size, const uint8_t & tage_bim_ctr_bits,
                 const uint16_t & tage_max_index_bits, const uint8_t & tage_tagged_ctr_bits,
                 const uint8_t & tage_tagged_useful_bits, const uint32_t & tage_global_history_len,
                 const uint32_t & tage_min_hist_len, const uint8_t & tage_hist_alpha,
                 const uint32_t & tage_reset_useful_interval, const uint8_t & tage_num_component);

            uint8_t predict(const uint64_t & ip);

          private:
            const uint32_t tage_bim_table_size_;
            const uint8_t tage_bim_ctr_bits_;
            uint16_t tage_max_index_bits_;
            const uint8_t tage_tagged_ctr_bits_;
            const uint8_t tage_tagged_useful_bits_;
            const uint32_t tage_global_history_len_;
            const uint32_t tage_min_hist_len_;
            const uint8_t tage_hist_alpha_;
            const uint32_t tage_reset_useful_interval_;
            const uint8_t tage_num_component_;
            TageBIM tage_bim_;
            std::vector<TageTaggedComponent> tage_tagged_components_;
            std::vector<uint8_t> tage_global_history_;

            // Helper function to compress tage ghr into a numeric
            uint64_t compressed_ghr(const uint8_t & req_length);
            // Helper function to calculate hash too index into tage table
            uint32_t hashAddr(const uint64_t & PC, const uint8_t & component_number);
            // Helper function to calcuate tag
            uint16_t calculatedTag(const uint64_t & PC, const uint8_t & component_number);
        };

        /***
         * TODO
        class StatisticalCorrector
        {
          public:
          private:
        };

        class LoopPredictor
        {
          public:
          private:
        };

        class TAGE_SC_L
        {
          public:
          private:
            Tage tage;
            StatisticalCorrector sc;
            LoopPredictor l;
        };
        ***/
    } // namespace BranchPredictor
} // namespace olympia