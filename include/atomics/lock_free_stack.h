//
// Created by Mark.Sweeney on 1/17/2025.
//

#ifndef LOCK_FREE_STACK_HPP
#define LOCK_FREE_STACK_HPP

#include <atomic>
#include <memory>

namespace totally_atomic {

  template<typename T>
  class lock_free_stack {
  private:
    struct node {
      std::shared_ptr<T> data;
      node* next{nullptr};
      explicit node(T const& data_) : data(std::make_shared<T>(data_)) {}
    };
    std::atomic<node*> head{nullptr};

  public:
    void push(T const& data) {
      auto new_node  = new node(data);
      new_node->next = head.load(std::memory_order_relaxed);
      while (!head.compare_exchange_weak(new_node->next, new_node,
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {}
    }

    std::shared_ptr<T> pop() {
      ++threads_in_pop;

      node* old_head = head.load(std::memory_order_relaxed);

      while (old_head &&
             !head.compare_exchange_weak(old_head, old_head->next,
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {}

      std::shared_ptr<T> res;
      if (old_head) {
        res.swap(old_head->data);
      }

      try_reclaim(old_head);
      return res;
    }

  private:
    std::atomic<unsigned int> threads_in_pop{0};
    std::atomic<node*> to_be_deleted{nullptr};

    static void delete_nodes(node* nodes) {
      while (nodes) {
        auto next = nodes->next;
        delete nodes;
        nodes = next;
      }
    }

    void try_reclaim(node* old_head) {
      if (threads_in_pop == 1) {
        node* nodes_to_delete = to_be_deleted.exchange(nullptr);
        if (!--threads_in_pop) {
          lock_free_stack::delete_nodes(nodes_to_delete);
        } else if (nodes_to_delete) {
          chain_pending_node(nodes_to_delete);
        }
        delete old_head;
      } else {
        chain_pending_node(old_head);
        --threads_in_pop;
      }
    }

    void chain_pending_nodes(node* nodes) {
      auto last = nodes;
      while (node* const next = last->next) {
        last = next;
      }
      chain_pending_nodes(nodes, last);
    }

    void chain_pending_nodes(node* first, node* last) {
      if (!last) return;

      last->next = to_be_deleted;
      while (!to_be_deleted.compare_exchange_weak(last->next, first)) {}
    }

    void chain_pending_node(node* n) { chain_pending_nodes(n, n); }
  };

  // A simple lock-free stack
  template<typename T>
  class LockFreeStack {
  private:
    struct Node {
      T data;
      Node* next;

      Node(T value) : data(value), next(nullptr) {}
    };

    std::atomic<Node*> head;

  public:
    LockFreeStack() : head(nullptr) {}

    ~LockFreeStack() {
      while (Node* node = head.load(std::memory_order_relaxed)) {
        head.store(node->next, std::memory_order_relaxed);
        delete node;
      }
    }

    void push(T value) {
      Node* newNode = new Node(value);
      newNode->next = head.load(std::memory_order_relaxed);

      // Use compare_exchange_weak in a loop to atomically update the head
      while (!head.compare_exchange_weak(newNode->next, newNode,
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {
        // retry
      }
    }

    bool pop(T& result) {
      Node* top = head.load(std::memory_order_relaxed);

      // Use compare_exchange_weak to atomically remove the head
      while (top && !head.compare_exchange_weak(top, top->next,
                                                std::memory_order_acquire,
                                                std::memory_order_relaxed)) {
        // retry
      }

      if (top) {
        result = top->data;
        delete top;
        return true;
      }
      return false;// stack was empty
    }

    bool isEmpty() const {
      return head.load(std::memory_order_relaxed) == nullptr;
    }
  };

}// namespace totally_atomic

#endif//LOCK_FREE_STACK_HPP
