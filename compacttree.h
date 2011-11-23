
#pragma once

#include <new>
#include <cstring> //for memmove
#include <stdint.h>
#include <cassert>
#include "thread.h"

/* CompactTree is a Tree of Nodes. It malloc's one chunk at a time, and has a very efficient allocation strategy.
 * It maintains a freelist of empty segments, but never assigns a segment to a smaller amount of memory,
 * completely avoiding fragmentation, but potentially having empty space in sizes that are no longer popular.
 * Since it maintains forward and backward pointers within the tree structure, it can move nodes around,
 * compacting the empty space and freeing it back to the OS. It can scan memory since it is a contiguous block
 * of memory with no fragmentation.
 * Your tree Node should include an instance of CompactTree<Node>::Children named 'children'
 */
template <class Node> class CompactTree {
	static const unsigned int CHUNK_SIZE = 16*1024*1024;
	static const unsigned int MAX_NUM = 300; //maximum amount of Node's to allocate at once, needed for size of freelist

	//Hold a list of children within the compact tree
	struct Data {
		const static uint32_t oldcount = 4; //how many generations it needs to be empty before it's considered old
		uint32_t    header;   //sanity check value, <= oldcount means it's empty
		uint16_t    capacity; //number of Node's worth of memory to follow
		uint16_t    used;     //number of children to follow that are actually used, num <= capacity
		//sizes are chosen such that they add to a multiple of word size on 32bit and 64bit machines.

		union {
			Data ** parent;  //pointer to the Data* in the parent Node that references this Data instance
			Data *  nextfree; //next free Data block of this size when in the free list
		};
		Node        children[0]; //array of Nodes, runs past the end of the data block

		Data(unsigned int n, Data ** p) : capacity(n), used(n), parent(p) {
			header = (((unsigned long)this >> 2) & 0xFFFF) | (0xBEEF << 16);
			if(empty()) header += 0xABCD;

			for(Node * i = begin(), * e = end(); i != e; ++i)
				new(i) Node(); //call the constructors
		}

		~Data(){
			for(Node * i = begin(), * e = end(); i != e; ++i)
				i->~Node();
			header = 0;
		}

		//how big is this structure in bytes by capacity or used
		size_t memsize() const { return sizeof(Data) + sizeof(Node)*capacity; }
		size_t memused() const { return sizeof(Data) + sizeof(Node)*used; }

		bool empty() const { return (header <= oldcount); }
		bool old()   const { return (header == oldcount); }

		Node * begin(){
			return children;
		}

		Node * end(){
			return children + used;
		}

		//not thread safe, so only use in child creation or garbage collection
		//useful if you allocated too much memory, then realized you didn't need it all, or
		//to reduce the size once you realize some nodes are no longer needed, like if they're proven a loss and can be ignored
		//shrink will simply remove the last few Nodes, so make sure to reorder the nodes before calling shrink
		//allows compact() to move the Data segment to a location where capacity matches used again to save space
		int shrink(int n){
			assert(n > 0 && n <= capacity && n <= used);
			for(Node * i = children + n, * e = children + used; i != e; ++i)
				i->~Node();
			int diff = used - n;
			used = n;
			return diff;
		}

		//make sure the parent points back to the same place
		bool parent_consistent() const {
			return (header == (*parent)->header);
		}

		//called after moving the memory to update the parent pointers for this node and its children
		void move(Data * s){
			assert(!empty()); //don't move an empty Data segment
			assert(*parent == s); //my parent points to my old location

			//update my parent with my new location
			*parent = this;

			//make sure the parent points back to the same place
			assert(parent_consistent());

			//update my children
			for(Node * i = begin(), * e = end(); i != e; ++i){
				if(i->children.data){
					i->children.data->parent = &(i->children.data);
					assert(i->children.data->parent_consistent());
				}
			}
		}
	};

public:
	//Sits in Node to manage the children, which are actually stored in a Data struct
	class Children {
		static const int LOCK = 1; //must be cast to (Data *) at usage point
		Data * data;
		friend class Data;

	public:
		typedef Node * iterator;
		Children() : data(NULL) { }
		~Children() { assert(data == NULL); }

		//lock the children, so only one thread creates them at a time, returns false if another thread already has the lock
		bool lock()   { return CAS(data, (Data *) NULL, (Data *) LOCK); }
		bool unlock() { return CAS(data, (Data *) LOCK, (Data *) NULL); }

		//allocate n nodes, likely best used in a temporary node and swapped in
		unsigned int alloc(unsigned int n, CompactTree & ct){
			assert(data == NULL);
			data = ct.alloc(n, &data);
			return n;
		}

		//deallocate the children
		unsigned int dealloc(CompactTree & ct){
			Data * t = data;
			int n = 0;
			if(t && CAS(data, t, (Data*)NULL)){
				n = t->used;
				ct.dealloc(t);
			}
			return n;
		}
		//swap children with the other node, used for threadsafe child creation
		void swap(Children & other){
			//swap data pointer
			Data * temp;
			temp = data;
			data = other.data;
			other.data = temp;
			temp = NULL;

			//update parent pointer
			if(data > (Data*)LOCK)
				data->parent = &data;
			if(other.data > (Data*)LOCK)
				other.data->parent = &(other.data);
		}
		//keep only the first n children, used if too many children were allocated
		int shrink(int n){
			return data->shrink(n);
		}
		//how many children are there?
		unsigned int num() const {
			return (data > (Data *) LOCK ? data->used : 0);
		}
		//does this node have any children?
		bool empty() const {
			return num() == 0;
		}
		//access a child at a specific offset
		Node & operator[](unsigned int offset){
			assert(data > (Data *) LOCK);
			assert(offset >= 0 && offset < data->used);
			return data->children[offset];
		}
		//iterator through the children
		Node * begin() const {
			if(data > (Data *) LOCK)
				return data->begin();
			return NULL;
		}
		//end of the iterator through the children
		Node * end() const {
			if(data > (Data *) LOCK)
				return data->end();
			return NULL;
		}
		//find a node associated with a move
		Node * find(const Move & m) const {
			for(Node * c = begin(), * cend = end(); c != cend; c++)
				if(c->move == m)
					return c;
			return NULL;
		}
	};

private:
	//Holds the large chunks of memory requested from the OS
	struct Chunk {
		Chunk *  next; //linked list of chunks
		uint32_t id;   //number of chunks before this one
		uint32_t capacity; //in bytes
		uint32_t used;     //in bytes
		char *   mem;  //actual memory

		Chunk()               : next(NULL), id(0), capacity(0), used(0), mem(NULL) { }
		Chunk(unsigned int c) : next(NULL), id(0), capacity(0), used(0), mem(NULL) { alloc(c); }
		~Chunk() { assert_empty(); }

		void alloc(unsigned int c){
			assert_empty();
			capacity = c;
			used = 0;
			mem = (char*) new uint64_t[capacity / sizeof(uint64_t)]; //use uint64_t instead of char to guarantee alignment
		}
		void dealloc(bool deallocnext = false){
			assert(capacity > 0 && mem != NULL);
			if(deallocnext && next != NULL){
				next->dealloc(deallocnext);
				delete next;
				next = NULL;
			}
			assert(next == NULL);
			capacity = 0;
			used = 0;
			delete[] (uint64_t *)mem;
			mem = NULL;
		}
		void assert_empty(){ assert(capacity == 0 && used == 0 && mem == NULL && next == NULL); }
		void clear_unused(){
			for(char * i = mem + used, * iend = mem + capacity ; i != iend; i++)
				*i = 0;
		}
	};

	class Freelist {
		Data * list[MAX_NUM];
		SpinLock lock;
	public:
		Freelist(){
			clear();
		}
		void clear(){
			for(unsigned int i = 0; i < MAX_NUM; i++)
				list[i] = NULL;
		}

		void push_nolock(Data * d){
			unsigned int num = d->capacity;
			d->nextfree = list[num];
			list[num] = d;
		}
		void push(Data * d){
			assert(d->empty());
			lock.lock();
			push_nolock(d);
			lock.unlock();
		}

		Data * pop_nolock(unsigned int num){
			Data * t = list[num];
			if(t)
				list[num] = t->nextfree;
			return t;
		}
		Data * pop(unsigned int num){
			lock.lock();
			Data * t = pop_nolock(num);
			lock.unlock();
			return t;
		}
	};


	Chunk * head,    //start of the chunk list
	      * current, //where memory is currently being allocated
	      * last;    //last chunk that isn't empty
	unsigned int numchunks;
	Freelist freelist;
	uint64_t memused;

public:

	CompactTree() {
		//allocate the first chunk
		head = current = last = new Chunk(CHUNK_SIZE);
		numchunks = 1;
		memused = 0;
	}
	~CompactTree(){
		head->dealloc(true);
		delete head;
		head = current = last = NULL;
		numchunks = 0;
	}

	//how much memory is malloced and available for use
	uint64_t memarena() const {
		Chunk * c = current;
		while(c->next)
			c = c->next;
		return ((uint64_t)(c->id+1))*((uint64_t)CHUNK_SIZE);
	}

	//how much memory is in use or in a freelist, a good approximation of real memory usage from the OS perspective
	uint64_t memalloced() const {
		return ((uint64_t)(last->id))*((uint64_t)CHUNK_SIZE) + current->used;
	}

	//how much memory is actually in use by nodes in the tree, plus the overhead of the Data struct
	//uses capacity, so may be inacurate for data segments that were shrunk but haven't been compacted yet
	//Data segments that are in the freelist are not included here
	uint64_t meminuse() const {
		return memused;
	}

	Data * alloc(unsigned int num, Data ** parent){
		assert(num > 0 && num < MAX_NUM);

		unsigned int size = sizeof(Data) + sizeof(Node)*num;
		PLUS(memused, size);

	//check freelist
		if(Data * t = freelist.pop(num)){
			assert(t->empty() && t->capacity == num);
			return new(t) Data(num, parent);
		}

	//allocate new memory
		while(1){
			Chunk * c = current;
			uint32_t used = c->used;
			if(used + size <= c->capacity){ //if there is room, try to use it
				if(CAS(c->used, used, used+size))
					return new((Data *)(c->mem + used)) Data(num, parent);
				else
					continue;
			}else if(c->next != NULL){ //if there is a next chunk, advance to it and try again
				CAS(current, c, c->next); //CAS to avoid skipping a chunk
				CAS(last, c, c->next); //most last forward too
				continue;
			}else{ //need to allocate a new chunk
				Chunk * next = new Chunk(CHUNK_SIZE);

				while(1){
					while(c->next != NULL) //advance to the end
						c = c->next;

					next->id = c->id+1;
					if(CAS(c->next, (Chunk *)NULL, next)){ //put it in place
						INCR(numchunks);
						//note that this doesn't move current forward since this may not be the next chunk
						// if there is a race condition where two threads allocate chunks at the same time
						break;
					}
				}
				continue;
			}
		}
		assert(false && "How'd CompactTree::alloc get here?");
		return NULL;
	}
	void dealloc(Data * d){
		assert(!d->empty() && d->capacity > 0 && d->capacity < MAX_NUM);

		unsigned int size = d->memsize();
		PLUS(memused, -size);

		//call the destructor
		d->~Data();
		d->used = 0;

		//add to the freelist
		freelist.push(d);
	}

	//assume this is the only thread running
	//arenasize is how much memory to keep around, as a fraction of current usage
	//  0 frees all extra memory, 1 keeps enough memory allocated to avoid having to call malloc to get back to this level
	//generationsize is how far to go through the list only adding to the freelist, not compacting. This avoids moving memory
	//  at the potential cost of not freeing any space at the end. Values around 1.0 are a waste of time, but 0.2 - 0.6 is good
	void compact(float arenasize = 0, float generationsize = 0){
		assert(arenasize >= 0 && arenasize <= 1);
		assert(generationsize >= 0 && generationsize <= 1);

		memused = 0;

		if(head->used == 0)
			return;

		//clear the freelist
		freelist.clear();

		Chunk      * schunk = head; //source chunk
		unsigned int soff   = 0;    //source offset
		Chunk      * dchunk = schunk; //destination chunk
		unsigned int doff   = soff;   //destination offset

		unsigned int generationid = (unsigned int)(generationsize * current->id);
		bool compactthischunk = (generationid == 0);

		//iterate over each chunk, moving data blocks to the left to fill empty space
		while(schunk != NULL){
			//iterate over each Data block
			Data * s = (Data *)(schunk->mem + soff);
			assert(s->capacity > 0 && s->capacity < MAX_NUM);

			int ssize = s->memsize(); //how much to move the source pointer

			//move from -> to, update parent pointer
			if(s->empty()){
				if(!compactthischunk){
					if(s->old()){//this empty segment is an unpopular size, lets compact this chunk to clean up this segment
						compactthischunk = true;
						dchunk = schunk;
						doff = soff;
					}else{ //freed recently, add to the free list
						freelist.push_nolock(s);
						s->header++; //empty, but a generation old
					}
				}//else this position will be overwritten by the next full chunk
			}else{
				if(!compactthischunk){
					memused += ssize;
				}else{
					assert(s->used > 0 && s->used <= s->capacity);
					int dsize = s->memused(); //how much to move the dest pointer
					Data * d = NULL;

					//where to move
					while(1){
						if((d = freelist.pop_nolock(s->used))){ //allocate off the freelist if possible
							break;
						}else if(doff + dsize <= dchunk->capacity){ //if space, allocate from this chunk
							assert(schunk->id > dchunk->id || (schunk == dchunk && soff >= doff)); //make sure I'm moving left
							d = (Data *)(dchunk->mem + doff);
							doff += dsize;
							break;
						}else{ //otherwise finish this chunk and prepare the next
							dchunk->used = doff;
							dchunk->clear_unused();

							dchunk = dchunk->next;
							doff = 0;
						}
					}

					//move!
					s->capacity = s->used;
					if(s != d){
						memmove(d, s, dsize);
						d->move(s);
					}
					memused += dsize;
				}
			}

			//update source
			soff += ssize;
			while(schunk && schunk->used <= soff){ //move forward, skip empty chunks
				//if the last chunk was being compacted out of order, finish it off
				if(schunk->id < generationid && compactthischunk){
					dchunk->used = doff;
					dchunk->clear_unused();
				}
				//go to the next source chunk
				schunk = schunk->next;
				soff = 0;
				//if we're done the static generation, initialize the destination chunk
				if(schunk){
					if(schunk->id == generationid){
						dchunk = schunk;
						doff = soff;
					}
					compactthischunk = (schunk->id >= generationid);
				}
			}
		}

		//finish the last used chunk
		dchunk->used = doff;
		dchunk->clear_unused();

		//free unused chunks
		Chunk * del = dchunk;
		while(del->next && del->id < arenasize*current->id){
			del = del->next;
			del->used = 0;
		}

		if(del->next != NULL){
			del->next->dealloc(true);
			delete del->next;
			del->next = NULL;
			numchunks = del->id + 1;
		}

		//set current to head in case some chunks aren't filled completely due to generations
		current = head;
		last = dchunk;
	}
};

