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

    /*!
     * \class Vector memory instruction config
     * \brief
     */
    class VectorMemConfig
    {
    public:
        using PtrType = sparta::SpartaSharedPointer<VectorMemConfig>;

        VectorMemConfig(uint32_t eew, uint32_t stride, uint32_t mop) :
            eew_(eew),
            stride_(stride),
            mop_(mop)
        {}

        VectorMemConfig() = default;

        void setEew(uint32_t eew) { eew_ = eew; }
        uint32_t getEew() const { return eew_; }

        void setMop(uint32_t mop) { mop_ = mop; }
        uint32_t getMop() const { return mop_; }

        void setStride(uint32_t stride) { stride_ = stride; }
        uint32_t getStride() const { return stride_; }

        void setTotalMemReqs(uint32_t vlsu_total_mem_reqs) { vlsu_total_mem_reqs_ = vlsu_total_mem_reqs; }
        uint32_t getTotalMemReqs() const { return vlsu_total_mem_reqs_; }

        void incrementNumMemReqsGenerated() { ++vlsu_num_mem_reqs_generated_; }
        uint32_t getNumMemReqsGenerated() const { return vlsu_num_mem_reqs_generated_; }

        void incrementNumMemReqsCompleted() { ++vlsu_num_mem_reqs_completed_; }
        uint32_t getNumMemReqsCompleted() const { return vlsu_num_mem_reqs_completed_; }
    private:
        uint32_t eew_ = 0;    // effective element width
        uint32_t stride_ = 0; // stride
        uint32_t mop_ = 0;    // memory addressing mode

        uint32_t vlsu_total_mem_reqs_ = 0;
        uint32_t vlsu_num_mem_reqs_generated_ = 0;
        uint32_t vlsu_num_mem_reqs_completed_ = 0;
    };

    using VectorConfigPtr = VectorConfig::PtrType;
    using VectorMemConfigPtr = VectorMemConfig::PtrType;

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
