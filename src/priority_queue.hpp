#ifndef SJTU_PRIORITY_QUEUE_HPP
#define SJTU_PRIORITY_QUEUE_HPP

#include <cstddef>
#include <functional>
#include "exceptions.hpp"

namespace sjtu {
/**
 * @brief a container like std::priority_queue which is a heap internal.
 *
 * Implemented as a leftist heap so that `merge` runs in O(log n).
 *
 * **Exception Safety**: The `Compare` operation might throw exceptions for
 * certain data. In such cases, any ongoing operation is terminated, and the
 * priority queue is restored to its original state before the operation
 * began, then a `runtime_error` is thrown.
 *
 * The internal merging routine performs all structural mutations strictly
 * *after* its recursive calls return. Therefore, if `Compare` throws at any
 * point, the exception propagates before a single pointer of either heap is
 * modified, which gives the strong exception guarantee for free.
 */
template<typename T, class Compare = std::less<T>>
class priority_queue {
private:
	struct Node {
		T data;
		Node *left;
		Node *right;
		int npl; // null path length: shortest distance to an empty subtree
		Node(const T &d) : data(d), left(nullptr), right(nullptr), npl(1) {}
	};

	Node *root;
	size_t sz;
	Compare comp;

	static int nplOf(const Node *p) { return p ? p->npl : 0; }

	/**
	 * Merge two leftist heaps and return the new root.
	 * Runs in O(log n). If `comp` throws, neither heap is modified,
	 * because every mutation below happens after the recursive call
	 * has successfully returned.
	 */
	Node *mergeNodes(Node *a, Node *b) {
		if (!a) return b;
		if (!b) return a;
		if (comp(a->data, b->data)) { // max-heap: keep the larger on top
			Node *t = a; a = b; b = t;
		}
		a->right = mergeNodes(a->right, b);
		if (nplOf(a->left) < nplOf(a->right)) {
			Node *t = a->left; a->left = a->right; a->right = t;
		}
		a->npl = nplOf(a->right) + 1;
		return a;
	}

	/**
	 * Iteratively free a whole subtree in O(n) time and O(1) extra space
	 * (rotation-based, avoids recursion depth issues on degenerate trees).
	 */
	static void destroy(Node *p) {
		while (p) {
			if (p->left) { // rotate the left child up
				Node *t = p->left;
				p->left = t->right;
				t->right = p;
				p = t;
			} else { // no left child: safe to delete, continue with right
				Node *t = p->right;
				delete p;
				p = t;
			}
		}
	}

	/**
	 * Iteratively deep-copy a subtree of exactly n nodes.
	 * If any allocation/copy throws, the partially built copy is freed.
	 */
	static Node *copyTree(const Node *srcRoot, size_t n) {
		if (!srcRoot) return nullptr;
		const Node **sstk = new const Node*[n];
		Node **dstk = new Node*[n];
		size_t top = 0;
		Node *newRoot = nullptr;
		try {
			newRoot = new Node(srcRoot->data);
			newRoot->npl = srcRoot->npl;
			sstk[top] = srcRoot; dstk[top] = newRoot; ++top;
			while (top) {
				--top;
				const Node *s = sstk[top];
				Node *d = dstk[top];
				if (s->left) {
					Node *c = new Node(s->left->data);
					c->npl = s->left->npl;
					d->left = c;
					sstk[top] = s->left; dstk[top] = c; ++top;
				}
				if (s->right) {
					Node *c = new Node(s->right->data);
					c->npl = s->right->npl;
					d->right = c;
					sstk[top] = s->right; dstk[top] = c; ++top;
				}
			}
		} catch (...) {
			if (newRoot) destroy(newRoot);
			delete[] sstk;
			delete[] dstk;
			throw;
		}
		delete[] sstk;
		delete[] dstk;
		return newRoot;
	}

public:
	/**
	 * @brief default constructor
	 */
	priority_queue() : root(nullptr), sz(0) {}

	/**
	 * @brief copy constructor
	 * @param other the priority_queue to be copied
	 */
	priority_queue(const priority_queue &other)
		: root(copyTree(other.root, other.sz)), sz(other.sz) {}

	/**
	 * @brief deconstructor
	 */
	~priority_queue() { destroy(root); }

	/**
	 * @brief Assignment operator
	 * @param other the priority_queue to be assigned from
	 * @return a reference to this priority_queue after assignment
	 */
	priority_queue &operator=(const priority_queue &other) {
		if (this == &other) return *this;
		Node *newRoot = copyTree(other.root, other.sz); // may throw, *this stays intact
		destroy(root);
		root = newRoot;
		sz = other.sz;
		return *this;
	}

	/**
	 * @brief get the top element of the priority queue.
	 * @return a reference of the top element.
	 * @throws container_is_empty if empty() returns true
	 */
	const T & top() const {
		if (!root) throw container_is_empty();
		return root->data;
	}

	/**
	 * @brief push new element to the priority queue.
	 * @param e the element to be pushed
	 */
	void push(const T &e) {
		Node *n = new Node(e);
		try {
			root = mergeNodes(root, n);
		} catch (...) {
			// the heap was not modified; just drop the new node
			delete n;
			throw runtime_error();
		}
		++sz;
	}

	/**
	 * @brief delete the top element from the priority queue.
	 * @throws container_is_empty if empty() returns true
	 */
	void pop() {
		if (!root) throw container_is_empty();
		Node *old = root;
		Node *merged;
		try {
			merged = mergeNodes(old->left, old->right);
		} catch (...) {
			// merging failed before any mutation; the heap is unchanged
			throw runtime_error();
		}
		root = merged;
		delete old;
		--sz;
	}

	/**
	 * @brief return the number of elements in the priority queue.
	 * @return the number of elements.
	 */
	size_t size() const { return sz; }

	/**
	 * @brief check if the container is empty.
	 * @return true if it is empty, false otherwise.
	 */
	bool empty() const { return sz == 0; }

	/**
	 * @brief merge another priority_queue into this one.
	 * The other priority_queue will be cleared after merging.
	 * The complexity is at most O(logn).
	 * @param other the priority_queue to be merged.
	 */
	void merge(priority_queue &other) {
		if (this == &other) return;
		Node *merged;
		try {
			merged = mergeNodes(root, other.root);
		} catch (...) {
			// both heaps are unchanged
			throw runtime_error();
		}
		root = merged;
		sz += other.sz;
		other.root = nullptr;
		other.sz = 0;
	}
};

}

#endif
