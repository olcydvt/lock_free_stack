#include <atomic>
#include <optional>
#include <vector>
#include <thread>

template <typename T>
struct lock_free_stack {
private:
    struct node {
        explicit node(const T& v) : val(v), next(nullptr) {}
        T val;
        node* next;
    };

    struct tagged_pointer {
        node* ptr;
        std::uintptr_t tag;
    };

    std::atomic<tagged_pointer> head;

    // Hazard pointer array (simplified, ideally use a thread-local hazard pointer system)
    static constexpr int MAX_THREADS = 8;  // Adjust based on expected concurrency
    std::atomic<node*> hazard_pointers[MAX_THREADS];

public:
    lock_free_stack() {
        head.store({nullptr, 0});
        for (auto& hp : hazard_pointers) {
            hp.store(nullptr);
        }
    }

    void push(const T& val) {
        node* new_node = new node(val);
        tagged_pointer old_head;
        do {
            old_head = head.load(std::memory_order_relaxed);
            new_node->next = old_head.ptr;
        } while (!head.compare_exchange_weak(old_head, {new_node, old_head.tag + 1},
                                             std::memory_order_release,
                                             std::memory_order_relaxed));
    }

    std::optional<T> pop() {
        int thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id()) % MAX_THREADS;
        node* old_node;

        tagged_pointer old_head;
        do {
            old_head = head.load(std::memory_order_acquire);
            if (old_head.ptr == nullptr) {
                return std::nullopt;
            }
            hazard_pointers[thread_id].store(old_head.ptr, std::memory_order_seq_cst);
        } while (!head.compare_exchange_weak(old_head, {old_head.ptr->next, old_head.tag + 1},
                                             std::memory_order_release,
                                             std::memory_order_relaxed));

        hazard_pointers[thread_id].store(nullptr, std::memory_order_seq_cst);

        // **DELAYED DELETION**
        bool can_delete = true;
        for (int i = 0; i < MAX_THREADS; ++i) {
            if (hazard_pointers[i].load(std::memory_order_seq_cst) == old_head.ptr) {
                can_delete = false;
                break;
            }
        }

        if (can_delete) {
            delete old_head.ptr;  // Safe to delete now
        }

        return old_head.ptr->val;
    }
};
