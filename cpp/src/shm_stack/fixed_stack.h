// clang-format off
// Compile & Run: g++ -std=c++17 -pthread fixed_stack.cpp -o /tmp/fixed_stack.out && /tmp/fixed_stack.out
// clang-format on
#include <algorithm>
#include <atomic>
#include <cassert>
#include <memory>
#include <vector>

/**
 * @brief 固定大小的共享内存池
 *
 * FixedStack 是一个线程安全的对象池，管理固定数量的对象实例。
 * 它使用 CAS (Compare-And-Swap) 原子操作实现无锁的对象获取和释放。
 *
 * @tparam T 池中存储的对象类型
 */
template <typename T> class FixedStack {
  /**
   * @brief 元素状态枚举
   *
   * 元素在其生命周期中会经历以下状态转换：
   * Available -> Acquired -> Available (正常使用流程)
   * Acquired -> Destroyed (栈被销毁时，元素正在使用)
   */
  enum class ElementState {
    Available, // 元素可用，可以被获取
    Acquired,  // 元素已被获取，正在使用中
    Destroyed, // 栈已被销毁，元素需要自行清理
  };

public:
  /**
   * @brief 池元素的包装类
   *
   * Element 封装了实际的对象 T，并使用原子状态来管理其生命周期。
   * 用户通过 shared_ptr<Element> 持有元素，当引用计数归零时自动释放。
   */
  class Element {
  public:
    /**
     * @brief 获取元素值的指针
     * @return 指向内部值的常量指针
     */
    inline const T *value() const { return m_value.get(); }

  private:
    /**
     * @brief 构造函数
     * @param value 要管理的对象，通过移动语义转移所有权
     */
    explicit Element(std::unique_ptr<T> value)
        : m_state{ElementState::Available}, m_value(std::move(value)) {}

  private:
    // 禁止拷贝构造和拷贝赋值
    Element(const Element &) = delete;
    Element &operator=(const Element &) = delete;

    std::atomic<ElementState> m_state; // 元素的原子状态
    const std::unique_ptr<T> m_value;  // 实际存储的对象
    friend class FixedStack<T>;        // 允许 FixedStack 访问私有成员
  };

public:
  /**
   * @brief 构造函数
   * @param values 要放入池中的对象集合，通过右值引用转移所有权
   *
   * 将所有对象包装成 Element 并存储在 m_elements 向量中。
   */
  explicit FixedStack(std::vector<std::unique_ptr<T>> &&values) {
    for (auto &value : values) {
      m_elements.emplace_back(new Element(std::move(value)));
    }
  }

  /**
   * @brief 析构函数
   *
   * 遍历所有元素：
   * - 如果元素状态是 Acquired（正在被使用），则尝试将状态改为 Destroyed
   *   这表明使用者需要在栈销毁后自行清理该元素
   * - 如果元素状态是 Available，则直接删除元素
   *
   * 这种设计确保了在栈销毁时，正在被使用的元素不会立即被删除，
   * 而是由使用者持有并负责清理。
   */
  ~FixedStack() {
    for (Element *element : m_elements) {
      ElementState expected = ElementState::Acquired;
      if (!element->m_state.compare_exchange_strong(
              expected, ElementState::Destroyed, std::memory_order_acq_rel)) {
        // 元素未被获取（状态不是 Acquired），直接删除
        delete element;
      }
      // 否则，状态改为 Destroyed，元素会在 shared_ptr deleter 中被删除
    }
    m_elements.clear();
  }

  /**
   * @brief 尝试从池中获取一个可用元素
   * @return shared_ptr<Element> 获取成功返回元素的智能指针，失败返回 nullptr
   *
   * 工作原理：
   * 1. 遍历所有元素，寻找状态为 Available 的元素
   * 2. 使用 CAS 原子操作将状态从 Available 改为 Acquired
   * 3. 如果成功，返回一个带有自定义 deleter 的 shared_ptr
   * 4. 如果失败（元素不可用），继续尝试下一个元素
   *
   * 自定义 deleter：
   * - 当 shared_ptr 的引用计数归零时调用
   * - 尝试将元素状态从 Acquired 改回 Available
   * - 如果当前状态是 Destroyed（栈已被销毁），则删除元素
   * - 如果 CAS 失败（其他线程修改了状态），也删除元素（异常情况）
   */
  std::shared_ptr<Element> tryAcquire() {
    for (Element *element : m_elements) {
      ElementState expected = ElementState::Available;
      if (element->m_state.compare_exchange_strong(
              expected, ElementState::Acquired, std::memory_order_acq_rel)) {
        // 成功获取元素，创建带有自定义 deleter 的 shared_ptr
        auto deleter = [](Element *element) {
          ElementState expected = ElementState::Acquired;
          if (!element->m_state.compare_exchange_strong(
                  expected, ElementState::Available,
                  std::memory_order_acq_rel)) {
            // 状态不是 Acquired（可能是 Destroyed），删除元素
            delete element;
          }
        };
        return std::shared_ptr<Element>(element, deleter);
      }
    }
    // 所有元素都不可用
    return nullptr;
  }

private:
  // 禁止拷贝构造和拷贝赋值
  FixedStack(const FixedStack &) = delete;
  FixedStack &operator=(const FixedStack &) = delete;

  std::vector<Element *> m_elements; // 元素指针数组
};
