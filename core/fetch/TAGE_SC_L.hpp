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
            TageTaggedComponentEntry(uint8_t tage_ctr_bits, uint8_t tage_useful_bits);

            void incrementCtr();
            void decrementCtr();
            void incrementUseful();
            void decrementUseful();

          private:
            uint16_t tag_;
            uint8_t tage_ctr_bits_;
            uint8_t tage_useful_bits_;
            uint8_t ctr_;
            uint8_t useful_;
        };

        class TageTaggedComponent
        {
          public:
            TageTaggedComponent(uint8_t tage_ctr_bits, uint8_t tage_useful_bits,
                                uint16_t num_tagged_entry);

          private:
            uint8_t tage_ctr_bits_;
            uint8_t tage_useful_bits_;
            uint16_t num_tagged_entry_;
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
            uint32_t tage_bim_table_size_;
            uint8_t tage_base_ctr_bits_;
            uint8_t tage_base_max_ctr_;
            std::vector<uint8_t> Tage_Bimodal_;
        };

        class Tage
        {
          public:
            Tage(uint32_t tage_bim_table_size, uint8_t tage_bim_ctr_bits,
                 uint8_t tage_tagged_ctr_bits, uint8_t tage_tagged_useful_bits,
                 uint8_t num_tage_component);

          private:
            uint32_t tage_bim_table_size_;
            uint8_t tage_bim_ctr_bits_;
            uint8_t tage_tagged_ctr_bits_;
            uint8_t tage_tagged_useful_bits_;
            uint8_t num_tage_component_;

            TageBIM tage_bim_;
            std::vector<TageTaggedComponent> tage_tagged_components_;
        };

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
    } // namespace BranchPredictor
} // namespace olympia