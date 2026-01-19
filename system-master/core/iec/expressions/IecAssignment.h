#pragma once

#include "IecExpression.h"
#include "../values/IecValueRef.h"

namespace system_runtime {

/*
 * Assignment expression skeleton.
 * Represents: target := expression
 */
class IecAssignment : public IecExpression {
public:
    IecAssignment(IecValueRef target, IecValueRef source);

    void evaluate() override;

private:
    IecValueRef target_;
    IecValueRef source_;
};

} // namespace system_runtime
