#include "IecAssignment.h"

namespace system_runtime {

IecAssignment::IecAssignment(IecValueRef target, IecValueRef source)
    : target_(target), source_(source) {}

void IecAssignment::evaluate() {
    /*
     * Skeleton only.
     * Future:
     *  - type checking
     *  - value copy
     */
}

} // namespace system_runtime
