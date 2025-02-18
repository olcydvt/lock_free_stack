#include <atomic>
#include <optional>

template <typename T>
struct lock_free_stack {
private:
    struct node {
        explicit node(const T& v) : val(v), next(nullptr) {} // Properly initialize val
        T val;
        node* next;
    };

    std::atomic<node*> head;

public:
    lock_free_stack() : head(nullptr) {} // Initialize head

    void push(const T& val) {
        node* new_node = new node(val);
        do {
            new_node->next = head.load(std::memory_order_relaxed);
        } while (!head.compare_exchange_weak(new_node->next, new_node, 
                                             std::memory_order_release, 
                                             std::memory_order_relaxed));
    }

    std::optional<T> pop() {
        node* old_head;
        do {
            old_head = head.load(std::memory_order_acquire); // at his time we use acquire to get updated value from push
            if (old_head == nullptr) {
                return std::nullopt;
            }
        } while (!head.compare_exchange_weak(old_head, old_head->next, 
                                             std::memory_order_release, 
                                             std::memory_order_relaxed));

        T return_value = old_head->val;
        delete old_head; // Correct deallocation
        return return_value;
    }
};
