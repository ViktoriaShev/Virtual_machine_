#include "IecExecutionEngine.h"

namespace system_runtime {

void IecExecutionEngine::addExpression(IecExpression* expr) {
    expressions_.push_back(expr);
}

void IecExecutionEngine::executeCycle(IecExecutionContext& ctx) {
    ctx.beginCycle();

    for (auto* expr : expressions_) {
        expr->evaluate();
    }

    ctx.endCycle();
}

} // namespace system_runtime
