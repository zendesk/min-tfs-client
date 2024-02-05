/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/core/distributed_runtime/coordination/coordination_service_barrier_proxy.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/platform/statusor.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/core/platform/threadpool.h"
#include "tsl/distributed_runtime/call_options.h"
#include "tsl/distributed_runtime/coordination/coordination_client.h"
#include "tsl/distributed_runtime/coordination/coordination_service_agent.h"
#include "tsl/protobuf/coordination_config.pb.h"
#include "tsl/protobuf/coordination_service.pb.h"

namespace tensorflow {
namespace {

using ::testing::_;
using ::testing::Return;
using tsl::CallOptions;
using tsl::CoordinationClient;
using tsl::CoordinationServiceAgent;

class MockCoordinationServiceAgent : public CoordinationServiceAgent {
 public:
  MOCK_METHOD(Status, WaitAtBarrier,
              (const std::string& barrier_id, absl::Duration timeout,
               const std::vector<CoordinatedTask>& tasks),
              (override));
  MOCK_METHOD(Status, CancelBarrier, (const std::string& barrier_id),
              (override));

  // All the following member functions are not needed for testing.
  MOCK_METHOD(Status, Initialize,
              (Env * env, const std::string& job_name, int task_id,
               const CoordinationServiceConfig& configs,
               std::unique_ptr<CoordinationClient> leader_client,
               StatusCallback error_fn),
              (override));
  MOCK_METHOD(Status, Initialize,
              (Env * env, const CoordinatedTask& task,
               const CoordinationServiceConfig& configs,
               std::unique_ptr<CoordinationClient> leader_client,
               StatusCallback error_fn),
              (override));
  MOCK_METHOD(bool, IsInitialized, (), (override));
  MOCK_METHOD(bool, IsConnected, (), (override));
  MOCK_METHOD(bool, IsError, (), (override));
  MOCK_METHOD(Status, Connect, (), (override));
  MOCK_METHOD(Status, WaitForAllTasks, (const DeviceInfo& local_devices),
              (override));
  MOCK_METHOD(const DeviceInfo&, GetClusterDeviceInfo, (), (override));
  MOCK_METHOD(StatusOr<CoordinatedTask>, GetOwnTask, (), (override));
  MOCK_METHOD(StatusOr<std::vector<CoordinatedTaskStateInfo>>, GetTaskState,
              (const std::vector<CoordinatedTask>& task), (override));
  MOCK_METHOD(Status, ReportError, (const Status& error), (override));
  MOCK_METHOD(Status, Shutdown, (), (override));
  MOCK_METHOD(Status, Reset, (), (override));
  MOCK_METHOD(StatusOr<std::string>, GetKeyValue, (const std::string& key),
              (override));
  MOCK_METHOD(StatusOr<std::string>, GetKeyValue,
              (const char* key, int64_t key_size), (override));
  MOCK_METHOD(StatusOr<std::string>, GetKeyValue,
              (const std::string& key, absl::Duration timeout), (override));
  MOCK_METHOD(std::shared_ptr<CallOptions>, GetKeyValueAsync,
              (const std::string& key, StatusOrValueCallback done), (override));
  MOCK_METHOD(StatusOr<std::string>, TryGetKeyValue, (const std::string& key),
              (override));
  MOCK_METHOD(StatusOr<std::vector<KeyValueEntry>>, GetKeyValueDir,
              (const std::string& key), (override));
  MOCK_METHOD(void, GetKeyValueDirAsync,
              (const std::string& key, StatusOrValueDirCallback done),
              (override));
  MOCK_METHOD(Status, InsertKeyValue,
              (const std::string& key, const std::string& value), (override));
  MOCK_METHOD(Status, InsertKeyValue,
              (const char* key, int64_t key_size, const char* value,
               int64_t value_size),
              (override));
  MOCK_METHOD(Status, DeleteKeyValue, (const std::string& key), (override));
  MOCK_METHOD(Status, DeleteKeyValue, (const char* key, int64_t key_size),
              (override));
  MOCK_METHOD(Status, UpdateKeyValue,
              (const std::string& key, const std::string& value), (override));
  MOCK_METHOD(Status, StartWatchKey,
              (const std::string& key, ChangedKeyValuesCallback on_change),
              (override));
  MOCK_METHOD(Status, StopWatchKey, (const std::string& key), (override));
  MOCK_METHOD(void, WaitAtBarrierAsync,
              (const std::string& barrier_id, absl::Duration timeout,
               const std::vector<CoordinatedTask>& tasks, StatusCallback done),
              (override));
  MOCK_METHOD(void, CancelBarrierAsync,
              (const std::string& barrier_id, StatusCallback done), (override));
  MOCK_METHOD(StatusOr<Env*>, GetEnv, (), (override));
  MOCK_METHOD(void, SetError, (const Status& error), (override));
  MOCK_METHOD(Status, ActivateWatch,
              (const std::string& key,
               (const std::map<std::string, std::string>&)),
              (override));
};

constexpr auto kTestKey = "test_key";
constexpr auto kTestTimeout = absl::Seconds(1);
const int kThreadPoolSize = 32;

void TestBarrierProxyWait(
    int num_tasks, int num_threads_planned, int num_threads_entered,
    int expected_ok_count, std::optional<Status> agent_wait_status,
    std::optional<Status> expected_same_exit_status_for_all_threads) {
  auto agent = std::make_unique<MockCoordinationServiceAgent>();
  const std::vector<CoordinatedTask> tasks(num_tasks);
  BarrierProxy barrier(agent.get(), tasks, num_threads_planned, kTestKey,
                       kTestTimeout);
  std::atomic<int> last_exit_count = 0;
  std::atomic<int> actual_ok_count = 0;

  if (agent_wait_status.has_value()) {
    EXPECT_CALL(*agent, WaitAtBarrier(kTestKey, kTestTimeout, _))
        .WillOnce(Return(agent_wait_status.value()));
  } else {
    EXPECT_CALL(*agent, WaitAtBarrier(kTestKey, kTestTimeout, _)).Times(0);
  }

  {
    thread::ThreadPool pool(Env::Default(), /*name=*/"TestPool",
                            kThreadPoolSize);
    for (int i = 0; i < num_threads_entered; ++i) {
      pool.Schedule([&]() {
        auto [status, last_exit] = barrier.Wait();
        if (expected_same_exit_status_for_all_threads.has_value()) {
          ASSERT_EQ(status, expected_same_exit_status_for_all_threads.value());
        }
        actual_ok_count += status.ok();
        last_exit_count += last_exit;
      });
    }
  }
  ASSERT_EQ(actual_ok_count, expected_ok_count);
  ASSERT_EQ(last_exit_count, 1);
}

TEST(BarrierProxyTest, AllThreadsExitBarrier) {
  TestBarrierProxyWait(
      /*num_tasks=*/2,
      /*num_threads_planned=*/8,
      /*num_threads_entered=*/8,
      /*expected_ok_count=*/8,
      /*agent_wait_status=*/OkStatus(),
      /*expected_same_exit_status_for_all_threads=*/OkStatus());
}

TEST(BarrierProxyTest, AgentErrorBroadcastedToAllThreads) {
  TestBarrierProxyWait(
      /*num_tasks=*/2,
      /*num_threads_planned=*/8,
      /*num_threads_entered=*/8,
      /*expected_ok_count=*/0,
      /*agent_wait_status=*/errors::Internal(""),
      /*expected_same_exit_status_for_all_threads=*/errors::Internal(""));
}

TEST(BarrierProxyTest, AgentIsIgnoredIfThereIsOnlyOneTask) {
  TestBarrierProxyWait(
      /*num_tasks=*/1,
      /*num_threads_planned=*/8,
      /*num_threads_entered=*/8,
      /*expected_ok_count=*/8,
      /*agent_wait_status=*/{},
      /*expected_same_exit_status_for_all_threads=*/OkStatus());
}

TEST(BarrierProxyTest, TimeoutIfNotEnoughThreadEntered) {
  TestBarrierProxyWait(
      /*num_tasks=*/2,
      /*num_threads_planned=*/8,
      /*num_threads_entered=*/7,
      /*expected_ok_count=*/0,
      /*agent_wait_status=*/{},
      /*expected_same_exit_status_for_all_threads=*/
      errors::DeadlineExceeded("BarrierProxy timeout: key=", kTestKey));
}

TEST(BarrierProxyTest, ExtraThreadsEnteringTheBarrierGetErrors) {
  TestBarrierProxyWait(
      /*num_tasks=*/2,
      /*num_threads_planned=*/8,
      /*num_threads_entered=*/10,
      /*expected_ok_count=*/8,
      /*agent_wait_status=*/OkStatus(),
      /*expected_same_exit_status_for_all_threads=*/{});
}

void TestBarrierProxyManagerWaitSingleKey(
    int num_threads_planned, int num_threads_entered,
    std::optional<Status> agent_wait_status, int expected_ok_count) {
  auto agent = std::make_unique<MockCoordinationServiceAgent>();
  const std::vector<CoordinatedTask> tasks;
  BarrierProxyManager mgr;
  std::atomic<int> actual_ok_count = 0;

  if (agent_wait_status.has_value()) {
    EXPECT_CALL(*agent, WaitAtBarrier(kTestKey, kTestTimeout, _))
        .WillOnce(Return(agent_wait_status.value()));
  }
  {
    thread::ThreadPool pool(Env::Default(), /*name=*/"TestPool",
                            num_threads_planned);
    for (int i = 0; i < num_threads_entered; ++i) {
      pool.Schedule([&]() {
        actual_ok_count += mgr.Wait(agent.get(), tasks, num_threads_planned,
                                    kTestKey, kTestTimeout)
                               .ok();
      });
    }
  }
  ASSERT_EQ(actual_ok_count, expected_ok_count);
  // The BarrierProxy will be cleared.
  ASSERT_EQ(mgr.size(), 0);
}

TEST(BarrierProxyManagerTest, AllThreadExited) {
  TestBarrierProxyManagerWaitSingleKey(
      /*num_threads_planned=*/8,
      /*num_threads_entered=*/8,
      /*agent_wait_status=*/OkStatus(),
      /*expected_ok_count=*/8);
}

TEST(BarrierProxyManagerTest, AllThreadTimedOut) {
  TestBarrierProxyManagerWaitSingleKey(
      /*num_threads_planned=*/8,
      /*num_threads_entered=*/7,
      /*agent_wait_status=*/{},
      /*expected_ok_count=*/0);
}

TEST(BarrierProxyManagerTest, CoordinationServiceError) {
  TestBarrierProxyManagerWaitSingleKey(
      /*num_threads_planned=*/8,
      /*num_threads_entered=*/8,
      /*agent_wait_status=*/errors::Internal(""),
      /*expected_ok_count=*/0);
}

TEST(BarrierProxyManagerTest, ExtraThreadsEnteringTheSameKeyGetErrors) {
  TestBarrierProxyManagerWaitSingleKey(
      /*num_threads_planned=*/8,
      /*num_threads_entered=*/10,
      /*agent_wait_status=*/OkStatus(),
      /*expected_ok_count=*/8);
}

TEST(BarrierProxyManagerTest, DifferentKeysDoNotInterfereWithEachOther) {
  constexpr int kNumThreads = 8;
  auto agent = std::make_unique<MockCoordinationServiceAgent>();
  const std::vector<CoordinatedTask> tasks;
  BarrierProxyManager mgr;

  EXPECT_CALL(*agent, WaitAtBarrier("key0", kTestTimeout, _))
      .WillOnce(Return(OkStatus()));
  EXPECT_CALL(*agent, WaitAtBarrier("key1", kTestTimeout, _))
      .WillOnce(Return(OkStatus()));
  {
    thread::ThreadPool pool(Env::Default(), /*name=*/"TestPool",
                            kThreadPoolSize);
    for (int i = 0; i < kNumThreads * 2; ++i) {
      pool.Schedule([&, key = absl::StrCat("key", i % 2)]() {
        ASSERT_EQ(mgr.Wait(agent.get(), tasks, kNumThreads, key, kTestTimeout),
                  OkStatus());
      });
    }
  }
}

}  // namespace
}  // namespace tensorflow
