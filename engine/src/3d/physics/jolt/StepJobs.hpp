#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/JobSystemWithBarrier.h>

#include <memory>

namespace haylen::core {
class JobSystem;
}

namespace haylen::physics3d::jolt {

// Runs the jobs of the steps of one world on the job system of the engine. The thread that steps the world runs every job that no worker started yet while it waits for the step, so a step never waits behind other work queued on the pool. Waking the workers costs more than a small step saves, so a step that is not parallel posts nothing and its thread runs every job, as it does wherever the engine runs no worker.
class StepJobs final : public JPH::JobSystemWithBarrier, public std::enable_shared_from_this<StepJobs> {
  public:
    // A job system of null runs every job on the thread that steps the world.
    StepJobs(core::JobSystem* jobSystem, int threadCount);
    ~StepJobs() override;

    StepJobs(const StepJobs&) = delete;
    StepJobs& operator=(const StepJobs&) = delete;

    // Decides before a step whether its jobs go to the workers of the engine.
    void setParallel(bool value) noexcept;
    [[nodiscard]] bool isParallel() const noexcept {
        return parallel;
    }

    [[nodiscard]] int GetMaxConcurrency() const noexcept override;
    JobHandle CreateJob(const char* name, JPH::ColorArg color, const JobFunction& function, JPH::uint32 dependencies) noexcept override;
    void CreateJobs(const char* name, JPH::ColorArg color, const JobFunction& function, JobHandle* handles, JPH::uint32 count, JPH::uint32 dependencies) noexcept override;

  protected:
    void QueueJob(Job* job) noexcept override;
    void QueueJobs(Job** queued, JPH::uint count) noexcept override;
    void FreeJob(Job* job) noexcept override;

  private:
    // A job on the pool of the engine, which keeps the job and its job system alive until it ran or the pool dropped it. Its members end in reverse order, so the job returns to the free list before the job system can go.
    struct PostedJob {
        std::shared_ptr<StepJobs> owner;
        JobHandle job;

        void operator()() const;
    };

    // The most jobs and barriers of one step of a world, as Jolt sizes them.
    static constexpr JPH::uint kMaxJobs = 2048;
    static constexpr JPH::uint kMaxBarriers = 8;

    [[nodiscard]] Job* allocateJob(const char* name, JPH::ColorArg color, const JobFunction& function, JPH::uint32 dependencies);

    core::JobSystem* system = nullptr;
    int threads = 1;
    bool parallel = false;
    JPH::FixedSizeFreeList<Job> jobs;
};

} // namespace haylen::physics3d::jolt
