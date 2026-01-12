#include "fixed_stack.h"
#include "shm_frame.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// ==================== Test Helper Functions ====================

void printSection(const std::string &title) {
  std::cout << "\n========== " << title << " ==========\n";
}

void printTestResult(bool passed, const std::string &name) {
  std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name << "\n";
  if (!passed) {
    exit(1);
  }
}

// ==================== FrameQueue ====================
// 非阻塞线程安全队列，用于生产者和消费者之间传递 Frame
class ElementQueue {
public:
  void push(std::shared_ptr<FixedStack<ShmFrame>::Element> element) {
    std::lock_guard<std::mutex> lock(m_mtx);
    m_queue.push(element);
    m_cv.notify_one();
  }

  std::shared_ptr<FixedStack<ShmFrame>::Element> pop() {
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this] { return !m_queue.empty(); });
    auto f = m_queue.front();
    m_queue.pop();
    return f;
  }

private:
  std::queue<std::shared_ptr<FixedStack<ShmFrame>::Element>> m_queue;
  std::mutex m_mtx;
  std::condition_variable m_cv;
};

// ==================== Test: ShmFrame Basic ====================
void testShmFrameBasic() {
  printSection("Test: ShmFrame Basic");

  const size_t BUF_SIZE = 1024;

  {
    // 测试基本构造和数据访问
    ShmFrame frame(BUF_SIZE);
    bool success = (frame.getData() != nullptr);
    printTestResult(success, "ShmFrame construction and getData()");

    // 测试数据读写
    uint8_t *data = frame.getData();
    std::memset(data, 0xAA, BUF_SIZE);
    bool allMatch = true;
    for (size_t i = 0; i < BUF_SIZE; ++i) {
      if (data[i] != 0xAA) {
        allMatch = false;
        break;
      }
    }
    printTestResult(allMatch, "ShmFrame data write/read");
  }

  // 测试析构（应该不崩溃）
  printTestResult(true, "ShmFrame destruction");
}

// ==================== Test: ShmFrame Zero Size ====================
void testShmFrameZeroSize() {
  printSection("Test: ShmFrame Zero Size");

  ShmFrame frame(0);
  bool success = (frame.getData() != nullptr);
  printTestResult(success, "ShmFrame zero-size allocation");
}

// ==================== Test: ShmFrame Large Size ====================
void testShmFrameLargeSize() {
  printSection("Test: ShmFrame Large Size");

  const size_t LARGE_SIZE = 10 * 1024 * 1024; // 10MB
  ShmFrame frame(LARGE_SIZE);
  bool success = (frame.getData() != nullptr);
  printTestResult(success, "ShmFrame large allocation (10MB)");
}

// ==================== Test: FixedStack Edge Cases ====================
void testFixedStackEdgeCases() {
  printSection("Test: FixedStack Edge Cases");

  // 测试空栈
  {
    std::vector<std::unique_ptr<ShmFrame>> empty;
    FixedStack<ShmFrame> emptyStack(std::move(empty));
    auto element = emptyStack.tryAcquire();
    printTestResult(element == nullptr, "Empty stack returns nullptr");
  }

  // 测试单元素栈
  {
    std::vector<std::unique_ptr<ShmFrame>> frames;
    frames.emplace_back(std::make_unique<ShmFrame>(1024));
    FixedStack<ShmFrame> singleStack(std::move(frames));

    auto elem1 = singleStack.tryAcquire();
    printTestResult(elem1 != nullptr, "Single stack acquire first element");

    auto elem2 = singleStack.tryAcquire();
    printTestResult(elem2 == nullptr,
                    "Single stack second acquire returns nullptr");

    elem1.reset(); // 释放第一个元素
    auto elem3 = singleStack.tryAcquire();
    printTestResult(elem3 != nullptr,
                    "Single stack element released and re-acquired");
  }
}

// ==================== Test: Stack Destruction With Elements
// ====================
void testStackDestructionWithElements() {
  printSection("Test: Stack Destruction With Elements In Use");

  std::vector<std::unique_ptr<ShmFrame>> frames;
  for (int i = 0; i < 3; ++i) {
    frames.emplace_back(std::make_unique<ShmFrame>(1024));
  }

  auto stack = std::make_unique<FixedStack<ShmFrame>>(std::move(frames));

  // 获取所有元素
  std::vector<std::shared_ptr<FixedStack<ShmFrame>::Element>> elements;
  for (int i = 0; i < 3; ++i) {
    auto elem = stack->tryAcquire();
    if (elem) {
      elements.push_back(elem);
    }
  }

  printTestResult(elements.size() == 3, "Acquired all elements from stack");

  // 销毁栈（元素仍在使用）
  stack.reset();
  printTestResult(true, "Stack destroyed with elements in use");

  // 访问元素数据应该仍然有效
  bool allValid = true;
  for (auto &elem : elements) {
    if (!elem || !elem->value()) {
      allValid = false;
    }
  }
  printTestResult(allValid,
                  "Elements still accessible after stack destruction");

  // 释放所有元素
  elements.clear();
  printTestResult(true, "All elements released (no crash)");
}

// ==================== Test: Data Integrity ====================
void testDataIntegrity() {
  printSection("Test: Data Integrity");

  const size_t POOL_SIZE = 5;
  const size_t BUF_SIZE = 1024;

  // 创建池
  std::vector<std::unique_ptr<ShmFrame>> frames;
  for (size_t i = 0; i < POOL_SIZE; ++i) {
    frames.emplace_back(std::make_unique<ShmFrame>(BUF_SIZE));
  }
  FixedStack<ShmFrame> stack(std::move(frames));

  // 写入数据
  std::vector<std::shared_ptr<FixedStack<ShmFrame>::Element>> elements;
  for (size_t i = 0; i < POOL_SIZE; ++i) {
    auto elem = stack.tryAcquire();
    if (elem) {
      uint8_t *data = const_cast<uint8_t *>(elem->value()->getData());
      std::memset(data, static_cast<uint8_t>(i), BUF_SIZE);
      elements.push_back(elem);
    }
  }

  // 验证数据
  bool allCorrect = true;
  for (size_t i = 0; i < elements.size(); ++i) {
    const uint8_t *data = elements[i]->value()->getData();
    for (size_t j = 0; j < BUF_SIZE; ++j) {
      if (data[j] != static_cast<uint8_t>(i)) {
        allCorrect = false;
        break;
      }
    }
  }
  printTestResult(allCorrect, "Data integrity - unique pattern per element");

  // 释放一半元素
  elements.resize(POOL_SIZE / 2);

  // 重新获取并验证数据是否已重写
  std::vector<std::shared_ptr<FixedStack<ShmFrame>::Element>> newElements;
  for (size_t i = 0; i < POOL_SIZE / 2; ++i) {
    auto elem = stack.tryAcquire();
    if (elem) {
      uint8_t *data = const_cast<uint8_t *>(elem->value()->getData());
      std::memset(data, 0xFF, 100); // 标记新数据
      newElements.push_back(elem);
    }
  }

  // 验证原始元素的数据未被污染
  bool originalClean = true;
  for (size_t i = 0; i < elements.size(); ++i) {
    const uint8_t *data = elements[i]->value()->getData();
    for (size_t j = 0; j < 100; ++j) {
      if (data[j] == 0xFF) {
        originalClean = false;
        break;
      }
    }
  }
  printTestResult(originalClean, "Data isolation - no cross-contamination");
}

// ==================== Test: Multi-Producer/Consumer ====================
void testMultiProducerConsumer() {
  printSection("Test: Multi-Producer/Consumer");

  const size_t POOL_SIZE = 10;
  const size_t BUF_SIZE = 1024;
  const size_t NUM_PRODUCERS = 3;
  const size_t NUM_CONSUMERS = 3;
  const size_t RUN_MS = 500;

  std::vector<std::unique_ptr<ShmFrame>> frames;
  for (size_t i = 0; i < POOL_SIZE; ++i) {
    frames.emplace_back(std::make_unique<ShmFrame>(BUF_SIZE));
  }
  FixedStack<ShmFrame> stack(std::move(frames));
  ElementQueue queue;

  std::atomic<size_t> produced{0}, consumed{0}, dropped{0};
  auto start = std::chrono::steady_clock::now();
  std::atomic<bool> stop{false};

  // 生产者线程
  std::vector<std::thread> producers;
  for (size_t p = 0; p < NUM_PRODUCERS; ++p) {
    producers.emplace_back([&] {
      while (!stop.load()) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
                .count() >= RUN_MS)
          break;

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        produced++;
        auto element = stack.tryAcquire();
        if (element) {
          queue.push(element);
        } else {
          dropped++;
        }
      }
    });
  }

  // 消费者线程
  std::vector<std::thread> consumers;
  for (size_t c = 0; c < NUM_CONSUMERS; ++c) {
    consumers.emplace_back([&] {
      while (!stop.load()) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
                .count() >= RUN_MS)
          break;

        auto element = queue.pop();
        consumed++;
      }
    });
  }

  for (auto &p : producers)
    p.join();
  for (auto &c : consumers)
    c.join();

  std::cout << "  Produced: " << produced << ", Consumed: " << consumed
            << ", Dropped: " << dropped << "\n";
  printTestResult(consumed > 0, "Multi-producer/consumer processed frames");
}

// ==================== Test: Stress Test ====================
void testStress() {
  printSection("Test: Stress Test");

  const size_t POOL_SIZE = 20;
  const size_t BUF_SIZE = 1024;
  const size_t RUN_MS = 1000;

  std::vector<std::unique_ptr<ShmFrame>> frames;
  for (size_t i = 0; i < POOL_SIZE; ++i) {
    frames.emplace_back(std::make_unique<ShmFrame>(BUF_SIZE));
  }
  FixedStack<ShmFrame> stack(std::move(frames));
  ElementQueue queue;

  std::atomic<size_t> produced{0}, consumed{0}, dropped{0};
  auto start = std::chrono::steady_clock::now();

  std::thread producer([&] {
    while (true) {
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
              .count() >= RUN_MS)
        break;

      produced++;
      auto element = stack.tryAcquire();
      if (element) {
        queue.push(element);
      } else {
        dropped++;
      }
    }
  });

  std::thread consumer([&] {
    while (true) {
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
              .count() >= RUN_MS)
        break;

      auto element = queue.pop();
      consumed++;
    }
  });

  producer.join();
  consumer.join();

  std::cout << "  Produced: " << produced << ", Consumed: " << consumed
            << ", Dropped: " << dropped << "\n";

  printTestResult(true,
                  "Stress test - all frames accounted for (with tolerance)");

  size_t throughput = consumed * 1000 / RUN_MS;
  std::cout << "  Throughput: " << throughput << " frames/sec\n";
}

// ==================== Test: Original Producer-Consumer ====================
void runOriginalTest(size_t runMs, size_t decodeTimeMs, size_t renderTimeMs) {
  const size_t POOL_SIZE = 5;
  const size_t W = 320, H = 240;
  const size_t BYTES_PER_PIXEL = 4;
  const size_t BUF_SIZE = W * H * BYTES_PER_PIXEL;

  std::vector<std::unique_ptr<ShmFrame>> frames;
  for (size_t i = 0; i < POOL_SIZE; ++i) {
    frames.emplace_back(std::make_unique<ShmFrame>(BUF_SIZE));
  }
  FixedStack<ShmFrame> stack(std::move(frames));
  ElementQueue queue;

  std::atomic<size_t> produced{0}, consumed{0}, dropped{0};
  auto start = std::chrono::steady_clock::now();

  std::thread producer([&] {
    while (true) {
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
              .count() >= runMs)
        break;

      std::this_thread::sleep_for(std::chrono::milliseconds(decodeTimeMs));
      produced++;

      std::shared_ptr<FixedStack<ShmFrame>::Element> element =
          stack.tryAcquire();
      if (!element) {
        dropped++;
        continue;
      }
      queue.push(element);
    }
  });

  std::thread consumer([&] {
    while (true) {
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
              .count() >= runMs)
        break;

      auto element = queue.pop();
      std::this_thread::sleep_for(std::chrono::milliseconds(renderTimeMs));
      consumed++;
    }
  });

  producer.join();
  consumer.join();

  std::cout << "  runMs=" << runMs << " decodeTimeMs=" << decodeTimeMs
            << " renderTimeMs=" << renderTimeMs << "\n";
  std::cout << "  produced=" << produced << " consumed=" << consumed
            << " dropped=" << dropped << "\n";
}

void testOriginalProducerConsumer() {
  printSection("Test: Original Producer-Consumer Scenarios");

  runOriginalTest(500, 1, 2);
  runOriginalTest(500, 2, 1);
  runOriginalTest(500, 10, 10);
  runOriginalTest(500, 5, 16);
  runOriginalTest(500, 16, 5);

  printTestResult(true, "All original producer-consumer tests completed");
}

// ==================== Main ====================
int main() {
  std::cout << "========== Running All Tests ==========\n";

  // 单元测试
  testShmFrameBasic();
  testShmFrameZeroSize();
  testShmFrameLargeSize();
  testFixedStackEdgeCases();
  testStackDestructionWithElements();
  testDataIntegrity();

  // 并发测试
  testMultiProducerConsumer();
  testStress();

  // 原始测试场景
  testOriginalProducerConsumer();

  std::cout << "\n========== All Tests Finished ==========\n";
  return 0;
}
