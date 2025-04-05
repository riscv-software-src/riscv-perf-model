#include "TAGE_SC_L.hpp"
#include <iostream>

namespace olympia
{
    namespace BranchPredictor
    {

        // Tage Tagged Component Entry
        TageTaggedComponentEntry::TageTaggedComponentEntry(uint8_t tage_ctr_bits,
                                                           uint8_t tage_useful_bits,
                                                           uint8_t ctr_initial,
                                                           uint8_t useful_initial) :
            tage_ctr_bits_(tage_ctr_bits),
            tage_useful_bits_(tage_useful_bits),
            ctr_(ctr_initial),
            useful_(useful_initial),
            tage_ctr_max_val_(1 << tage_ctr_bits_),
            tage_useful_max_val_(1 << tage_useful_bits_)
        {
        }

        void TageTaggedComponentEntry::incrementCtr() 
        {
            if(ctr_ < tage_ctr_max_val_) {
                ctr_++;
            }
        }

        void TageTaggedComponentEntry::decrementCtr() 
        {
            if(ctr_ > 0) {
                ctr_--;
            }
        }

        uint8_t TageTaggedComponentEntry::getCtr() { return ctr_; }

        void TageTaggedComponentEntry::incrementUseful() 
        {
            if(useful_ < tage_useful_max_val_) {
                useful_++;
            }
        }

        void TageTaggedComponentEntry::decrementUseful() 
        {
            if(useful_ > 0) {
                useful_--;
            }
        }

        uint8_t TageTaggedComponentEntry::getUseful() { return useful_; }

        // Tagged component
        TageTaggedComponent::TageTaggedComponent(uint8_t tage_ctr_bits, uint8_t tage_useful_bits,
                                                 uint16_t num_tagged_entry) :
            tage_ctr_bits_(tage_ctr_bits),
            tage_useful_bits_(tage_useful_bits),
            num_tagged_entry_(num_tagged_entry),
            tage_tagged_component_(num_tagged_entry_, {tage_ctr_bits_, tage_useful_bits_, 0, 1})
        {
        }

        TageTaggedComponentEntry TageTaggedComponent::getTageComponentEntryAtIndex(uint16_t index)
        {
            return tage_tagged_component_[index];
        }

        // Tage Bimodal
        TageBIM::TageBIM(uint32_t tage_bim_table_size, uint8_t tage_base_ctr_bits) :
            tage_bim_table_size_(tage_bim_table_size),
            tage_bim_ctr_bits_(tage_base_ctr_bits),
            Tage_Bimodal_(tage_bim_table_size_)
        {
            // recheck value
            tage_bim_max_ctr_ = 1 << tage_bim_ctr_bits_;

            // initialize counter at all index to be 0
            for (auto & val : Tage_Bimodal_)
            {
                val = 0;
            }
        }

        void TageBIM::incrementCtr(uint32_t ip)
        {
            if (Tage_Bimodal_[ip] < tage_bim_max_ctr_)
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
                   uint16_t tage_max_index_bits, uint8_t tage_tagged_ctr_bits,
                   uint8_t tage_tagged_useful_bits, uint32_t tage_global_history_len,
                   uint32_t tage_min_hist_len, uint8_t tage_hist_alpha,
                   uint32_t tage_reset_useful_interval, uint8_t tage_num_component) :
            tage_bim_table_size_(tage_bim_table_size),
            tage_bim_ctr_bits_(tage_bim_ctr_bits),
            tage_max_index_bits_(tage_max_index_bits),
            tage_tagged_ctr_bits_(tage_tagged_ctr_bits),
            tage_tagged_useful_bits_(tage_tagged_useful_bits),
            tage_global_history_len_(tage_global_history_len),
            tage_min_hist_len_(tage_min_hist_len),
            tage_hist_alpha_(tage_hist_alpha),
            tage_reset_useful_interval_(tage_reset_useful_interval),
            tage_num_component_(tage_num_component),
            tage_bim_(tage_bim_table_size_, tage_bim_table_size_),
            // for now number of tagged component entry in each component is set as 10
            tage_tagged_components_(tage_num_component_,
                                    {tage_tagged_ctr_bits_, tage_tagged_useful_bits_, 10}),
            tage_global_history_(tage_global_history_len_, 0)
        {
        }

        uint32_t Tage::hashAddr(uint64_t PC, uint8_t component_number)
        {
            // TODO: Write logic to calculate hash
            uint32_t hashedValue = 0;
            return hashedValue;
        }

        uint16_t Tage::calculatedTag(uint64_t PC, uint8_t component_number)
        {
            // uint16_t tag_i = (PC ^ (PC >> shift1) ^ compressed_ghr);
            uint16_t tag_i = 0;
            return tag_i;
        }

        // for now return 1
        uint8_t Tage::predict(uint64_t ip)
        {
            uint8_t def_pred = tage_bim_.getPrediction(ip);
            uint8_t num_counter = 0;
            uint8_t tage_pred = def_pred;
            for (auto tage_i : tage_tagged_components_)
            {
                uint64_t effective_addr = hashAddr(ip, num_counter);
                TageTaggedComponentEntry tage_entry =
                    tage_i.getTageComponentEntryAtIndex(effective_addr);

                if (tage_entry.tag == calculatedTag(ip, num_counter))
                {
                    tage_pred = tage_entry.getCtr();
                }
                num_counter++;
            }
            std::cout << "getting tage prediction\n";
            return tage_pred;
        }
    } // namespace BranchPredictor
} // namespace olympia