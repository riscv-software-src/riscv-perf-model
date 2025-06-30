#include "TAGE_SC_L.hpp"
#include <iostream>
#include <cmath>

namespace olympia
{
    namespace BranchPredictor
    {

        // Tage Tagged Component Entry
        TageTaggedComponentEntry::TageTaggedComponentEntry(const uint8_t & tage_ctr_bits,
                                                           const uint8_t & tage_useful_bits,
                                                           const uint8_t & ctr_initial,
                                                           const uint8_t & useful_initial) :
            tage_ctr_bits_(tage_ctr_bits),
            tage_useful_bits_(tage_useful_bits),
            tage_ctr_max_val_(1 << tage_ctr_bits_),
            tage_useful_max_val_(1 << tage_useful_bits_),
            ctr_(ctr_initial),
            useful_(useful_initial)
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

        void TageTaggedComponentEntry::resetUseful() 
        {
            useful_ >>= 1;
        }

        uint8_t TageTaggedComponentEntry::getUseful() { return useful_; }

        // Tagged component
        TageTaggedComponent::TageTaggedComponent(const uint8_t & tage_ctr_bits, const uint8_t & tage_useful_bits,
                                                 const uint16_t & num_tagged_entry) :
            tage_ctr_bits_(tage_ctr_bits),
            tage_useful_bits_(tage_useful_bits),
            num_tagged_entry_(num_tagged_entry),
            tage_tagged_component_(num_tagged_entry_, {tage_ctr_bits_, tage_useful_bits_, 0, 1})
        {
        }

        TageTaggedComponentEntry TageTaggedComponent::getTageComponentEntryAtIndex(const uint16_t & index)
        {
            return tage_tagged_component_[index];
        }

        // Tage Bimodal
        TageBIM::TageBIM(const uint32_t & tage_bim_table_size, const uint8_t & tage_base_ctr_bits) :
            tage_bim_table_size_(tage_bim_table_size),
            tage_bim_ctr_bits_(tage_base_ctr_bits),
            tage_bimodal_(tage_bim_table_size_)
        {
            tage_bim_max_ctr_ = 1 << tage_bim_ctr_bits_;

            // initialize counter at all index to be 0
            for (auto & val : tage_bimodal_)
            {
                val = 0;
            }
        }

        void TageBIM::incrementCtr(const uint32_t & ip)
        {
            if (tage_bimodal_[ip] < tage_bim_max_ctr_)
            {
                tage_bimodal_[ip]++;
            }
        }

        void TageBIM::decrementCtr(const uint32_t & ip)
        {
            if (tage_bimodal_[ip] > 0)
            {
                tage_bimodal_[ip]--;
            }
        }

        uint8_t TageBIM::getPrediction(const uint32_t & ip) { return tage_bimodal_[ip]; }

        // TAGE
        Tage::Tage(const uint32_t & tage_bim_table_size, const uint8_t & tage_bim_ctr_bits,
                   //const uint16_t & tage_max_index_bits, 
                   const uint8_t & tage_tagged_ctr_bits,
                   const uint8_t & tage_tagged_useful_bits, const uint32_t & tage_global_history_len,
                   const uint32_t & tage_min_hist_len, const uint8_t & tage_hist_alpha,
                   const uint32_t & tage_reset_useful_interval, const uint8_t & tage_num_component,
                   const uint16_t & tage_tagged_component_entry_num) :
            tage_bim_table_size_(tage_bim_table_size),
            tage_bim_ctr_bits_(tage_bim_ctr_bits),
            //tage_max_index_bits_(tage_max_index_bits),
            tage_tagged_ctr_bits_(tage_tagged_ctr_bits),
            tage_tagged_useful_bits_(tage_tagged_useful_bits),
            tage_global_history_len_(tage_global_history_len),
            tage_min_hist_len_(tage_min_hist_len),
            tage_hist_alpha_(tage_hist_alpha),
            tage_reset_useful_interval_(tage_reset_useful_interval),
            tage_num_component_(tage_num_component),
            tage_tagged_component_entry_num_(tage_tagged_component_entry_num),
            tage_bim_(tage_bim_table_size_, tage_bim_ctr_bits_),
            tage_tagged_components_(tage_num_component_,
                                    {tage_tagged_ctr_bits_, tage_tagged_useful_bits_, tage_tagged_component_entry_num_}),
            tage_global_history_(tage_global_history_len_, 0)
        {
        }

        uint64_t Tage::compressed_ghr(const uint8_t & req_length)
        {
            uint64_t resultant_ghr = 0;
            // if req_length is more than tage_global_history_len_ then return whole tage_global_history_
            uint8_t length = (req_length > tage_global_history_len_) ? tage_global_history_len_ : req_length;
            for(uint8_t idx = 0; idx < length; idx++) {
                resultant_ghr = (resultant_ghr << 1) | tage_global_history_[tage_global_history_len_ - idx - 1];
            }
            return resultant_ghr;
        }

        uint32_t Tage::hashAddr(const uint64_t & PC, const uint8_t & component_number)
        {
            uint8_t local_hist_len = tage_min_hist_len_ * pow(tage_hist_alpha_, component_number - 1);
            uint64_t effective_hist = compressed_ghr(local_hist_len);
            uint32_t hashedValue = (uint32_t) (PC ^ effective_hist);
            return hashedValue;
        }

        uint16_t Tage::calculatedTag(const uint64_t & PC, const uint8_t & component_number)
        {
            uint8_t local_hist_len = tage_min_hist_len_ * pow(tage_hist_alpha_, component_number - 1);
            uint64_t effective_hist = compressed_ghr(local_hist_len);
            uint16_t calculated_tag = (uint16_t) (PC ^ effective_hist);
            return calculated_tag;
        }

        void Tage::updateUsefulBits() 
        {
            for(auto & tage_table : tage_tagged_components_) 
            {
                for(auto & entry : tage_table.tage_tagged_component_) 
                {
                    entry.resetUseful();
                }
            }
        }

        uint8_t Tage::predict(const uint64_t & ip)
        {
            if(reset_counter < tage_reset_useful_interval_) 
            {       
                reset_counter++;
            }
            else if(reset_counter == tage_reset_useful_interval_) 
            {
                reset_counter = 0;
                updateUsefulBits();
            }

            uint8_t default_prediction = tage_bim_.getPrediction(ip);
            uint8_t num_counter = 1;
            uint8_t tage_prediction = default_prediction;
            for (auto tage_i : tage_tagged_components_)
            {
                uint64_t effective_addr = hashAddr(ip, num_counter);
                TageTaggedComponentEntry tage_entry =
                    tage_i.getTageComponentEntryAtIndex(effective_addr);

                // Tag match
                if (tage_entry.tag == calculatedTag(ip, num_counter))
                {
                    tage_prediction = tage_entry.getCtr();
                }
                num_counter++;
            }
            return tage_prediction;
        }
    } // namespace BranchPredictor
} // namespace olympia