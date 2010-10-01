
#ifndef _CHILDREN_H_
#define _CHILDREN_H_

#include "thread.h"

template <class Node> class Children {
	static const int LOCK = 1; //needs to be cast to (Node *) at usage point

	uint32_t _num; //can be smaller, but will be padded anyway.
	Node *   _children;
public:
	typedef Node * iterator;
	Children()      : _num(0), _children(NULL) { }
	Children(int n) : _num(0), _children(NULL) { alloc(n); }
	~Children() { assert_empty(); }

	void assert_consistent() const { assert((_num == 0) || (_children > (Node *) LOCK)); }
	void assert_empty()      const { assert((_num == 0) && (_children == NULL)); }

	bool lock()   { return CAS(_children, (Node *) NULL, (Node *) LOCK); }
	bool unlock() { return CAS(_children, (Node *) LOCK, (Node *) NULL); }

	void atomic_set(Children & o){
		assert(CAS(_children, (Node *) LOCK, o._children)); //undoes the lock
		assert(CAS(_num, 0, o._num)); //keeps consistency
	}

	unsigned int alloc(unsigned int n){
		assert(_num == 0);
		assert(_children == NULL);
		assert(n > 0);

		_num = n;
		_children = new Node[_num];

		return _num;
	}
	void neuter(){
		_children = 0;
		_num = 0;
	}
	unsigned int dealloc(){
		assert_consistent();

		int n = _num;
		if(_children){
			Node * temp = _children; //CAS!
			neuter();
			delete[] temp;
		}
		return n;
	}
	void swap(Children & other){
		Children temp = other;
		other = *this;
		*this = temp;
		temp.neuter(); //to avoid problems with the destructor of temp
	}
	unsigned int num() const {
		assert_consistent();
		return _num;
	}
	bool empty() const {
		return num() == 0;
	}
	Node & operator[](unsigned int offset){
		assert(_children > (Node *) LOCK);
		assert(offset >= 0 && offset < _num);
		return _children[offset];
	}
	Node * begin() const {
		assert_consistent();
		return _children;
	}
	Node * end() const {
		assert_consistent();
		return _children + _num;
	}
};

#endif

