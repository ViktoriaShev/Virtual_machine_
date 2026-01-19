#pragma once

#include <vector>

#include "IecVariable.h"

namespace system_runtime {

/*
 * IEC Memory skeleton.
 * Represents VAR / VAR_INPUT / VAR_OUTPUT sections.
 */
class IecMemory {
public:
    void addVar(IecVariable var);
    void addInput(IecVariable var);
    void addOutput(IecVariable var);

    const std::vector<IecVariable>& vars() const;
    const std::vector<IecVariable>& inputs() const;
    const std::vector<IecVariable>& outputs() const;

private:
    std::vector<IecVariable> vars_;
    std::vector<IecVariable> inputs_;
    std::vector<IecVariable> outputs_;
};

} // namespace system_runtime
