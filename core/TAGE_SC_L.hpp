#pragma once

namespace olympia
{
    class TageBIM
    {
        public:
            TageBIM(uint32_t tage_bim_size, uint8_t tage_bim_ctr_bits) :
                tage_bim_size_(tage_bim_size), 
                tage_bim_ctr_bits_(tage_bim_ctr_bits) 
                {
                    tage_bim_ctr_bits_val_ = pow(2, tage_bim_ctr_bits_);
                }

        private:
            const uint32_t tage_bim_size_;
            const uint8_t  tage_bim_ctr_bits_;
            const uint8_t  tage_bim_ctr_bits_val_;

    };

    class TageTaggedComponent
    {
        public:
            TageTaggedComponent(uint16_t tag, uint8_t ctr, uint8_t u) :
                tag_(tag), ctr_(ctr), u_(u) {}
        private:
            const uint16_t tag_;
            const uint8_t  ctr_;
            const uint8_t  u_;
    };

    class Tage
    {
        public:
            
    };
}