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
      auto new_node = new node(data);
      new_node->next = head.load();
      while (!head.compare_exchange_weak(new_node->next, new_node)) {}
    }
    std::shared_ptr<T> pop() {
      ++threads_in_pop;
      node* old_head = head.load();
      while (old_head && !head.compare_exchange_weak(old_head, old_head->next) ) {}

      std::shared_ptr<T> res;
      if (old_head) {res.swap(old_head->data); }

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
    }

    void chain_pending_nodes(node* first, node* last) {
      if (!last) return;

      last->next = to_be_deleted;
      while (!to_be_deleted.compare_exchange_weak( last->next, first)) {}
    }

    void chain_pending_node(node* n) {
      chain_pending_nodes(n, n);
    }
  };

}// namespace totally_atomic

#endif//LOCK_FREE_STACK_HPP
