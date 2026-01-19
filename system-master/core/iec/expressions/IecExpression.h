#pragma once

namespace system_runtime {

/*
 * Base IEC expression skeleton.
 * Evaluation logic will be added later.
 */
class IecExpression {
public:
    virtual ~IecExpression() = default;
    virtual void evaluate() = 0;
};

} // namespace system_runtime
