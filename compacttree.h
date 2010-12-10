


#ifndef _COMPACTTREE_H_
#define _COMPACTTREE_H_

#include <new>
#include <cstring> //for memmove
#include <stdint.h>
#include "thread.h"

/* CompactTree is a Tree of Nodes. It malloc's one chunk at a time, and has a very efficient allocation strategy.
 * It maintains a freelist of empty segments, but never assigns a segment to a smaller amount of memory,
 * completely avoiding fragmentation, but potentially being having empty space in sizes that are no longer popular
 * Since it maintains forward and backward pointers within the tree structure, it can move nodes around,
 * compacting the empty space and freeing it back to the OS. It can scan memory since it is a contiguous block
 * of memory with no fragmentation.
 */
template <class Node> class CompactTree {
	static const unsigned int CHUNK_SIZE = 16*1024*1024;
	static const unsigned int MAX_NUM = 300; //maximum amount of Node's to allocate at once, needed for size of freelist

	struct Data {
		uint32_t    header; //sanity check value, 0 means it's unused
		uint32_t    num;    //number of children to follow
		//use 32bit values instead of 16bit values to ensure sizeof(Data) is a multiple of word size on 64bit machines
		//if more fields are needed later, the values above can be reduced such that their sum is a multiple of 8 bytes
		//could also switch to half-word sized values so their size sum to 32bit on 32bit machines and 64bit on 64bit machines
		union {
			Data ** parent;  //pointer to the Data* in the parent Node that references this Data instance
			Data *  nextfree; //next free Data block of this size when in the free list
		};
		Node        children[0]; //array of Nodes, runs past the end of the data block

		Data(unsigned int n, Data ** p) : num(n), parent(p) {
			header = ((unsigned long)this >> 2) & 0xFFFFFF;
			if(header == 0) header = 0xABCDF3;

			for(Node * i = begin(), * e = end(); i != e; ++i)
				new(i) Node(); //call the constructors
		}

		~Data(){
			for(Node * i = begin(), * e = end(); i != e; ++i)
				i->~Node();
			header = 0;
		}

		Node * begin(){
			return children;
		}

		Node * end(){
			return children + num;
		}

		//make sure the parent points back to the same place
		bool parent_consistent(){
			return (header == (*parent)->header);
		}

		void move(Data * s){
			assert(header > 0);
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
	class Children {
		static const int LOCK = 1; //must be cast to (Data *) at usage point
		Data * data;
		friend class Data;

	public:
		typedef Node * iterator;
		Children() : data(NULL) { }
		~Children() { assert(data == NULL); }

		bool lock()   { return CAS(data, (Data *) NULL, (Data *) LOCK); }
		bool unlock() { return CAS(data, (Data *) LOCK, (Data *) NULL); }

		unsigned int alloc(unsigned int n, CompactTree & ct){
			assert(data == NULL);
			data = ct.alloc(n, &data);
			return n;
		}
		void neuter(){
			data = NULL;
		}
		unsigned int dealloc(CompactTree & ct){
			Data * t = data;
			int n = 0;
			if(t && CAS(data, t, (Data*)NULL)){
				n = t->num;
				ct.dealloc(t);
			}
			return n;
		}
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
		unsigned int num() const {
			return (data > (Data *) LOCK ? data->num : 0);
		}
		bool empty() const {
			return num() == 0;
		}
		Node & operator[](unsigned int offset){
			assert(data > (Data *) LOCK);
			assert(offset >= 0 && offset < data->num);
			return data->children[offset];
		}
		Node * begin() const {
			if(data > (Data *) LOCK)
				return data->begin();
			return NULL;
		}
		Node * end() const {
			if(data > (Data *) LOCK)
				return data->end();
			return NULL;
		}
	};

private:
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
	};

	Chunk * head, * current;
	unsigned int numchunks;
	Data * freelist[MAX_NUM];

public:

	CompactTree() {
		for(unsigned int i = 0; i < MAX_NUM; i++)
			freelist[i] = NULL;

		//allocate the first chunk
		head = current = new Chunk(CHUNK_SIZE);
		numchunks = 1;
	}
	~CompactTree(){
		head->dealloc(true);
		delete head;
		head = current = NULL;
		numchunks = 0;
	}

	uint64_t memused(){
		return ((uint64_t)(current->id))*((uint64_t)CHUNK_SIZE) + current->used;
	}

	Data * alloc(unsigned int num, Data ** parent){
		assert(num > 0 && num < MAX_NUM);

//		fprintf(stderr, "+%u ", num);

	//check freelist
		while(Data * t = freelist[num]){
			if(CAS(freelist[num], t, t->nextfree))
				return new(t) Data(num, parent);
		}

	//allocate new memory
		unsigned int size = sizeof(Data) + sizeof(Node)*num;
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
				continue;
			}else{ //need to allocate a new chunk
				Chunk * next = new Chunk(CHUNK_SIZE);

				while(1){
					while(c->next != NULL) //advance to the end
						c = c->next;

					next->id = c->id+1;
					if(CAS(c->next, NULL, next)){ //put it in place
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
//		fprintf(stderr, "-%u ", d->num);
		assert(d->num > 0 && d->num < MAX_NUM);
		assert(d->header > 0);

		//call the destructor
		d->~Data();

		//add to the freelist
		while(1){
			Data * t = freelist[d->num];
			d->nextfree = t;
			if(CAS(freelist[d->num], t, d))
				break;
		}
	}

	//assume this is the only thread running
	void compact(float arenasize = 0){
		if(head->used == 0)
			return;

//		fprintf(stderr, "Compact\n");
		Chunk      * dchunk = head; //destination chunk
		unsigned int doff = 0;      //destination offset
		Chunk      * schunk = head; //source chunk
		unsigned int soff = 0;      //source offset

		//iterate over each chunk
		while(schunk != NULL){
			assert(schunk->id > dchunk->id || (schunk == dchunk && soff >= doff));

			//iterate over each Data block
			Data * s = (Data *)(schunk->mem + soff);
//			fprintf(stderr, "%u, %u, %p, %u, %u, %p -> %u, %u", schunk, soff, s, s->header, s->num, s->parent, dchunk, doff);
			assert(s->num > 0 && s->num < MAX_NUM);
			int size = sizeof(Data) + sizeof(Node)*s->num;

			//move from -> to, update parent pointer
			if(s->header > 0){
				Data * d = NULL;

				//where to move
				while(1){
					if(doff + size <= dchunk->capacity){ //if space, allocate from this chunk
						d = (Data *)(dchunk->mem + doff);
						doff += size;
						break;
					}else{ //otherwise finish this chunk and prepare the next
						dchunk->used = doff;

						//zero out the remainder of the chunk
						for(char * i = dchunk->mem + dchunk->used, * iend = dchunk->mem + dchunk->capacity ; i != iend; i++)
							*i = 0;

						dchunk = dchunk->next;
						doff = 0;
					}
				}

				//move!
				if(s != d){
					memmove(d, s, size);
					d->move(s);
				}
//				fprintf(stderr, ", %p, %u, %u, %p", d, d->header, d->num, d->parent);
			}

			//update source
			soff += size;
			while(schunk && schunk->used <= soff){ //move forward, skip empty chunks
				schunk = schunk->next;
				soff = 0;
			}

//			fprintf(stderr, "\n");
		}
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

		//save used last position
		dchunk->used = doff;
		current = dchunk;

		//zero out the remainder of the chunk
		for(char * i = dchunk->mem + dchunk->used, * iend = dchunk->mem + dchunk->capacity ; i != iend; i++)
			*i = 0;

		//clear the freelist
		for(unsigned int i = 0; i < MAX_NUM; i++)
			freelist[i] = NULL;

//		fprintf(stderr, "Compact done\n");
	}
};

#endif

