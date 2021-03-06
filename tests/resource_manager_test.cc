/*
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <chrono>

#include "gtest/gtest.h"
#include "lullaby/util/resource_manager.h"

namespace lull {
namespace {

struct TestResource {
 public:
  TestResource() : value(0) {}
  explicit TestResource(int value) : value(value) {}

  int value;
};

TEST(ResourceManagerTest, Create) {
  ResourceManager<TestResource> manager;
  auto res = manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });
  EXPECT_EQ(456, res->value);
}

TEST(ResourceManagerTest, Find) {
  ResourceManager<TestResource> manager;
  manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });

  auto res = manager.Find(123);
  EXPECT_EQ(456, res->value);
}

TEST(ResourceManagerTest, NoFind) {
  ResourceManager<TestResource> manager;
  manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });

  auto res = manager.Find(456);
  EXPECT_EQ(nullptr, res);
}

TEST(ResourceManagerTest, Release) {
  ResourceManager<TestResource> manager;
  manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });

  auto res = manager.Find(123);
  EXPECT_EQ(456, res->value);

  manager.Release(123);
  res.reset();

  res = manager.Find(123);
  EXPECT_EQ(nullptr, res);
}

TEST(ResourceManagerTest, GroupAttachRelease) {
  ResourceManager<TestResource> manager;
  manager.PushNewResourceGroup();
  manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });

  {
    auto res = manager.Find(123);
    EXPECT_EQ(456, res->value);
  }

  auto* group = manager.PopResourceGroup();
  manager.ReleaseResourceGroup(group);

  {
    auto res = manager.Find(123);
    EXPECT_EQ(nullptr, res);
  }
}

TEST(ResourceManagerTest, GroupAttachDetachRelease) {
  ResourceManager<TestResource> manager;
  manager.PushNewResourceGroup();
  manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });
  auto* group = manager.PopResourceGroup();
  manager.Create(456, []() {
    return std::shared_ptr<TestResource>(new TestResource(789));
  });

  manager.ReleaseResourceGroup(group);

  {
    auto res = manager.Find(123);
    EXPECT_EQ(nullptr, res);
  }

  {
    auto res = manager.Find(456);
    EXPECT_EQ(789, res->value);
  }
}

TEST(ResourceManagerTest, ReleaseAlive) {
  ResourceManager<TestResource> manager;
  manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });

  auto res = manager.Find(123);
  EXPECT_EQ(456, res->value);

  manager.Release(123);

  auto res2 = manager.Find(123);
  EXPECT_EQ(res, res2);
}

TEST(ResourceManagerTest, Recreate) {
  ResourceManager<TestResource> manager;
  manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });
  manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(789));
  });

  auto res = manager.Find(123);
  EXPECT_EQ(456, res->value);
}

TEST(ResourceManagerTest, RecreateAlive) {
  ResourceManager<TestResource> manager;
  manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });
  auto res = manager.Find(123);
  manager.Release(123);

  manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(789));
  });

  auto res2 = manager.Find(123);
  EXPECT_EQ(res, res2);
}

TEST(ResourceManagerTest, ExplicitCache) {
  ResourceManager<TestResource> manager(
      ResourceManager<TestResource>::kCacheExplicitly);

  auto obj1 = manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });
  EXPECT_EQ(456, obj1->value);

  auto obj2 = manager.Find(123);
  EXPECT_EQ(nullptr, obj2);

  manager.Register(123, obj1);

  obj2 = manager.Find(123);
  EXPECT_EQ(obj1.get(), obj2.get());
}

TEST(ResourceManagerTest, WeakCache) {
  ResourceManager<TestResource> manager(
      ResourceManager<TestResource>::kWeakCachingOnly);

  auto obj1 = manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });
  EXPECT_EQ(456, obj1->value);

  auto obj2 = manager.Find(123);
  EXPECT_EQ(obj1.get(), obj2.get());

  obj1.reset();

  obj2 = manager.Find(123);
  EXPECT_EQ(456, obj2->value);

  obj2.reset();

  obj2 = manager.Find(123);
  EXPECT_EQ(nullptr, obj2);
}

TEST(ResourceManagerTest, StrongCache) {
  ResourceManager<TestResource> manager(
      ResourceManager<TestResource>::kCacheFullyOnCreate);

  auto obj1 = manager.Create(123, []() {
    return std::shared_ptr<TestResource>(new TestResource(456));
  });
  EXPECT_EQ(456, obj1->value);

  auto obj2 = manager.Find(123);
  EXPECT_EQ(obj1.get(), obj2.get());

  obj1.reset();

  obj2 = manager.Find(123);
  EXPECT_EQ(456, obj2->value);

  obj2.reset();

  obj2 = manager.Find(123);
  EXPECT_EQ(456, obj2->value);
}

TEST(ResourceManagerTest, TrackNewInstances) {
  constexpr HashValue key = 123;
  ResourceManager<TestResource> manager;

  for (int i = 0; i < 10; ++i) {
    auto obj = manager.Create(
        key, []() { return std::shared_ptr<TestResource>(new TestResource); });
    EXPECT_EQ(manager.Find(key), obj);
    manager.Release(key);
    EXPECT_EQ(manager.Find(key), obj);
    obj.reset();
    EXPECT_EQ(manager.Find(key), nullptr);
  }
}

}  // namespace
}  // namespace lull
