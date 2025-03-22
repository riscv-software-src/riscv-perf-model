#pragma once

#include <vector>
#include <cstdint>

namespace olympia
{
    namespace BranchPredictor
    {
        class TageTaggedComponentEntry
        {
          public:
            TageTaggedComponentEntry(uint8_t tage_ctr_bits, uint8_t tage_useful_bits, 
              uint8_t ctr_initial, uint8_t useful_initial);

            void incrementCtr();
            void decrementCtr();
            void incrementUseful();
            void decrementUseful();

            uint16_t tag;

          private:
            const uint8_t tage_ctr_bits_;
            const uint8_t tage_useful_bits_;

            uint8_t ctr_;
            uint8_t useful_;
        };

        class TageTaggedComponent
        {
          public:
            TageTaggedComponent(uint8_t tage_ctr_bits, uint8_t tage_useful_bits,
                                uint16_t num_tagged_entry);

          private:
            const uint8_t tage_ctr_bits_;
            const uint8_t tage_useful_bits_;
            const uint16_t num_tagged_entry_;
            std::vector<TageTaggedComponentEntry> tage_tagged_component_;
        };

        class TageBIM
        {
          public:
            TageBIM(uint32_t tage_bim_table_size, uint8_t tage_base_ctr_bits);

            void incrementCtr(uint32_t ip);
            void decrementCtr(uint32_t ip);
            uint8_t getPrediction(uint32_t ip);

          private:
            const uint32_t tage_bim_table_size_;
            const uint8_t tage_bim_ctr_bits_;
            const uint8_t tage_bim_max_ctr_;
            std::vector<uint8_t> Tage_Bimodal_;
        };

        class Tage
        {
          public:
            Tage(uint32_t tage_bim_table_size, uint8_t tage_bim_ctr_bits,
                 uint16_t tage_max_index_bits, uint8_t tage_tagged_ctr_bits, 
                 uint8_t tage_tagged_useful_bits, uint32_t tage_global_hist_buff_len,
                 uint32_t tage_min_hist_len, uint8_t tage_hist_alpha,
                 uint32_t tage_reset_useful_interval, uint8_t tage_num_component);

            uint8_t predict(uint64_t ip);

          private:
            const uint32_t tage_bim_table_size_;
            const uint8_t tage_bim_ctr_bits_;
            uint16_t tage_max_index_bits_;
            const uint8_t tage_tagged_ctr_bits_;
            const uint8_t tage_tagged_useful_bits_;
            const uint32_t tage_global_hist_buff_len_;
            const uint32_t tage_min_hist_len_;
            const uint8_t tage_hist_alpha_;
            const uint32_t tage_reset_useful_interval_;
            const uint8_t tage_num_component_;
            TageBIM tage_bim_;
            std::vector<TageTaggedComponent> tage_tagged_components_;
            std::vector<uint8_t> global_history_buff_;
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