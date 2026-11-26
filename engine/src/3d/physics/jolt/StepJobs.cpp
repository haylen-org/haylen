#include "3d/physics/jolt/StepJobs.hpp"

#include <stdexcept>

#include "haylen/core/JobSystem.hpp"

namespace haylen::physics3d::jolt {

StepJobs::StepJobs(core::JobSystem* jobSystem, int threadCount) : JobSystemWithBarrier(kMaxBarriers), system(jobSystem), threads(threadCount) {
    jobs.Init(kMaxJobs, kMaxJobs);
}

StepJobs::~StepJobs() = default;

void StepJobs::setParallel(bool value) noexcept {
    parallel = value && threads > 1 && system != nullptr && system->getWorkerCount() > 0;
}

int StepJobs::GetMaxConcurrency() const noexcept {
    return parallel ? threads : 1;
}

// Jolt sizes the jobs of one step within the free list, so a full list is a defect that ends the app with its reason.
StepJobs::Job* StepJobs::allocateJob(const char* name, JPH::ColorArg color, const JobFunction& function, JPH::uint32 dependencies) {
    const JPH::uint32 index = jobs.ConstructObject(name, color, this, function, dependencies);
    if (index == JPH::FixedSizeFreeList<Job>::cInvalidObjectIndex) {
        throw std::length_error("A step of a 3D physics world made more jobs than its job system holds.");
    }

    return &jobs.Get(index);
}

StepJobs::JobHandle StepJobs::CreateJob(const char* name, JPH::ColorArg color, const JobFunction& function, JPH::uint32 dependencies) noexcept {
    Job* job = allocateJob(name, color, function, dependencies);
    JobHandle handle(job);
    if (dependencies == 0) {
        QueueJob(job);
    }

    return handle;
}

void StepJobs::CreateJobs(const char* name, JPH::ColorArg color, const JobFunction& function, JobHandle* handles, JPH::uint32 count, JPH::uint32 dependencies) noexcept {
    for (JPH::uint32 index = 0; index < count; ++index) {
        handles[index] = JobHandle(allocateJob(name, color, function, dependencies));
    }

    if (dependencies == 0) {
        for (JPH::uint32 index = 0; index < count; ++index) {
            QueueJob(handles[index].GetPtr());
        }
    }
}

// A job that is not posted waits in its barrier, where the thread that steps the world runs it.
void StepJobs::QueueJob(Job* job) noexcept {
    if (!parallel) {
        return;
    }

    system->post(PostedJob{.owner = shared_from_this(), .job = JobHandle(job)});
}

void StepJobs::QueueJobs(Job** queued, JPH::uint count) noexcept {
    for (JPH::uint index = 0; index < count; ++index) {
        QueueJob(queued[index]);
    }
}

void StepJobs::FreeJob(Job* job) noexcept {
    jobs.DestructObject(job);
}

void StepJobs::PostedJob::operator()() const {
    job.GetPtr()->Execute();
}

} // namespace haylen::physics3d::jolt
