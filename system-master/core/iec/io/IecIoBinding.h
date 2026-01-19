#pragma once

#include <vector>

#include "IecIoEndpoint.h"

namespace system_runtime {

/*
 * IEC IO Binding (skeleton).
 * Manages connections between variables and IO endpoints.
 */
class IecIoBinding {
public:
    void addInput(IecIoEndpoint* endpoint, IecValueRef target);
    void addOutput(IecIoEndpoint* endpoint, IecValueRef source);

    void readInputs();
    void writeOutputs();

private:
    struct InputBinding {
        IecIoEndpoint* endpoint;
        IecValueRef target;
    };

    struct OutputBinding {
        IecIoEndpoint* endpoint;
        IecValueRef source;
    };

    std::vector<InputBinding> inputs_;
    std::vector<OutputBinding> outputs_;
};

} // namespace system_runtime
