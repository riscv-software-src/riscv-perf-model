#include "TAGE_SC_L.hpp"

namespace olympia
{
    namespace BranchPredictor
    {

        // Tage Tagged Component Entry
        TageTaggedComponentEntry::TageTaggedComponentEntry(uint8_t tage_ctr_bits,
                                                           uint8_t tage_useful_bits) :
            tage_ctr_bits_(tage_ctr_bits),
            tage_useful_bits_(tage_useful_bits)
        {
        }

        void TageTaggedComponentEntry::incrementCtr() {}

        void TageTaggedComponentEntry::decrementCtr() {}

        void TageTaggedComponentEntry::incrementUseful() {}

        void TageTaggedComponentEntry::decrementUseful() {}

        // Tagged component
        TageTaggedComponent::TageTaggedComponent(uint8_t tage_ctr_bits, uint8_t tage_useful_bits,
                                                 uint16_t num_tagged_entry) :
            tage_ctr_bits_(tage_ctr_bits),
            tage_useful_bits_(tage_useful_bits),
            num_tagged_entry_(num_tagged_entry),
            tage_tagged_component_(num_tagged_entry_, {tage_ctr_bits_, tage_useful_bits_})
        {
        }

        // Tage Bimodal
        TageBIM::TageBIM(uint32_t tage_bim_table_size, uint8_t tage_base_ctr_bits) :
            tage_bim_table_size_(tage_bim_table_size),
            tage_base_ctr_bits_(tage_base_ctr_bits)
        {
            // recheck value
            tage_base_max_ctr_ = 2 << tage_base_ctr_bits_;

            // initialize counter at all index to be 0
            for (auto & val : Tage_Bimodal_)
            {
                val = 0;
            }
        }

        void TageBIM::incrementCtr(uint32_t ip)
        {
            if (Tage_Bimodal_[ip] < tage_base_max_ctr_)
            {
                Tage_Bimodal_[ip]++;
            }
        }

        void TageBIM::decrementCtr(uint32_t ip)
        {
            if (Tage_Bimodal_[ip] > 0)
            {
                Tage_Bimodal_[ip]--;
            }
        }

        uint8_t TageBIM::getPrediction(uint32_t ip) { return Tage_Bimodal_[ip]; }

        // TAGE
        Tage::Tage(uint32_t tage_bim_table_size, uint8_t tage_bim_ctr_bits,
                   uint8_t tage_tagged_ctr_bits, uint8_t tage_tagged_useful_bits,
                   uint8_t num_tage_component) :
            tage_bim_table_size_(tage_bim_table_size),
            tage_bim_ctr_bits_(tage_bim_ctr_bits),
            tage_tagged_ctr_bits_(tage_tagged_ctr_bits),
            tage_tagged_useful_bits_(tage_tagged_useful_bits),
            num_tage_component_(num_tage_component),
            tage_bim_(tage_bim_table_size_, tage_bim_table_size_),
            // for now number of tagged component entry in each component is set as 10
            tage_tagged_components_(num_tage_component_,
                                    {tage_tagged_ctr_bits_, tage_tagged_useful_bits_, 10})
        {
        }
    } // namespace BranchPredictor
} // namespace olympia