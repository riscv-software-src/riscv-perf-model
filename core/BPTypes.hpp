#pragma once

namespace olympia
{
    class PredictionInput
    {
        public:
            uint64_t PC;
            uint8_t instType;
    };

    class PredictionOutput
    {
        public:
            bool predDirection;
            uint64_t predPC;
    };

    class UpdateInput
    {
        public:
            uint64_t instrPC;
            bool correctedDirection;
            uint64_t correctedPC;
    };

}