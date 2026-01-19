#pragma once

namespace system_runtime {

/*
 * IEC Runtime State (skeleton).
 * Holds execution status and flags.
 */
class IecRuntimeState {
public:
    enum class Status {
        Idle,
        Running,
        Error
    };

    void setStatus(Status status);
    Status status() const;

private:
    Status status_ = Status::Idle;
};

} // namespace system_runtime
