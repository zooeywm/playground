#include <stdint.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>

class ShmFrame {
public:
  explicit ShmFrame(size_t size) {
    m_shmId = shmget(IPC_PRIVATE, size, IPC_CREAT | 0666);
    if (m_shmId >= 0) {
      void *ptr = shmat(m_shmId, nullptr, 0);
      if (ptr != MAP_FAILED) {
        m_data = static_cast<uint8_t *>(ptr);
        m_isShm = true;
      } else {
        shmctl(m_shmId, IPC_RMID, nullptr);
        m_data = new uint8_t[size];
        m_shmId = -1;
        m_isShm = false;
      }
    } else {
      m_data = new uint8_t[size];
      m_shmId = -1;
      m_isShm = false;
    }
  }

  ~ShmFrame() {
    if (m_isShm) {
      shmdt(m_data);
      shmctl(m_shmId, IPC_RMID, nullptr);
    } else {
      delete[] m_data;
    }
  }

  ShmFrame(const ShmFrame &) = delete;
  ShmFrame &operator=(const ShmFrame &) = delete;

  uint8_t *getData() const { return m_data; }

private:
  uint8_t *m_data = nullptr;
  int m_shmId = -1;
  bool m_isShm = false;
};
