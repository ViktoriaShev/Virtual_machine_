#pragma once

#include <vector>

#include "../expressions/IecExpression.h"
#include "IecExecutionContext.h"

namespace system_runtime {

/*
 * IEC Execution Engine (skeleton).
 * Responsible for ordered execution of expressions.
 */
class IecExecutionEngine {
public:
    void addExpression(IecExpression* expr);
    void executeCycle(IecExecutionContext& ctx);

private:
    std::vector<IecExpression*> expressions_;
};

} // namespace system_runtime
