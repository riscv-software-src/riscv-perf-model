// <VectorConfig.hpp> -*- C++ -*-

#pragma once

namespace olympia
{
    /*!
     * \class Vector config
     * \brief
     */
    class VectorConfig
    {
    public:

        // Vector register length in bits
        static const uint32_t VLEN = 1024;

        using PtrType = sparta::SpartaSharedPointer<VectorConfig>;

        VectorConfig(uint32_t vl, uint32_t sew, uint32_t lmul, uint32_t vta) :
            sew_(sew),
            lmul_(lmul),
            vl_(vl),
            vlmax_(vlmax_formula_()),
            vta_(vta)
        {
            // Check validity of vector config
            sparta_assert(lmul_ <= 8,
                "LMUL (" << lmul_ << ") cannot be greater than " << 8);
            sparta_assert(vl_ <= vlmax_,
                "VL (" << vl_ << ") cannot be greater than VLMAX ("<< vlmax_ << ")");
        }

        VectorConfig() = default;

        uint32_t getSEW() const { return sew_; }
        void setSEW(uint32_t sew)
        {
            sew_ = sew;
            vlmax_ = vlmax_formula_();
        }

        uint32_t getLMUL() const { return lmul_; }
        void setLMUL(uint32_t lmul)
        {
            lmul_ = lmul;
            vlmax_ = vlmax_formula_();
        }

        uint32_t getVL() const { return vl_; }
        void setVL(uint32_t vl)
        {
            vl_ = vl;
        }

        uint32_t getVLMAX() const { return vlmax_; }

        uint32_t getVTA() const { return vta_; }
        void setVTA(uint32_t vta)
        {
            vta_ = vta;
        }

    private:
        uint32_t sew_ = 8;  // set element width
        uint32_t lmul_ = 1; // effective length
        uint32_t vl_ = 16;  // vector length
        uint32_t vlmax_ = vlmax_formula_();
        bool vta_ = false;  // vector tail agnostic, false = undisturbed, true = agnostic
            
        uint32_t vlmax_formula_()
        {
            return (VLEN / sew_) * lmul_;
        }
    };

    using VectorConfigPtr = VectorConfig::PtrType;

    inline std::ostream & operator<<(std::ostream & os, const VectorConfig & vector_config)
    {
        os << "e" << vector_config.getSEW() <<
              "m" << vector_config.getLMUL() <<
              (vector_config.getVTA() ? "ta" : "") <<
              " vl: " << vector_config.getVL() <<
              " vlmax: " << vector_config.getVLMAX();
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os, const VectorConfig * vector_config)
    {
        if (vector_config)
        {
            os << *vector_config;
        }
        else
        {
            os << "nullptr";
        }
        return os;
    }
} // namespace olympia
