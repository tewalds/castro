


#ifndef _COMPACTTREE_H_
#define _COMPACTTREE_H_

#include <new>
#include <cstring> //for memmove
#include <stdint.h>
#include "thread.h"

/* CompactTree is a Tree of Nodes. It malloc's one chunk at a time, and has a very efficient allocation strategy
 * It maintains a freelist of empty segments, but never assigns a segment to a smaller amount of memory,
 * completely avoiding fragmentation, but potentially being having empty space in sizes that are no longer popular
 * Since it maintains forward and backward pointers within the tree structure, it can move nodes around,
 * compacting the empty space and freeing it back to the OS. It can scan memory since it is a contiguous block
 * of memory with no fragmentation.
 */
template <class Node> class CompactTree {
	static const unsigned int CHUNK_SIZE = 16*1024*1024;
	static const unsigned int MAX_CHUNKS = 10240;
	static const unsigned int MAX_NUM = 300; //maximum amount of Node's to allocate at once

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
			assert(header == 0);

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

		void move(Data * s){
			assert(header > 0);
			assert(*parent == s);

			//update my parent
			*parent = this;

			//make sure the parent points back to the same place
			assert(header == (*parent)->header);

			//update my children
			for(unsigned int i = 0; i < num; i++){
				if(children[i].children.data){
					children[i].children.data->parent = &(children[i].children.data);
					assert(children[i].children.data->header == (*(children[i].children.data->parent))->header);
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
		uint32_t capacity; //in bytes
		uint32_t used;     //in bytes
		char *   mem;

		Chunk()               : capacity(0), used(0), mem(NULL) { }
		Chunk(unsigned int c) : capacity(0), used(0), mem(NULL) { alloc(c); }
		~Chunk() { assert_empty(); }

		void alloc(unsigned int c){
			assert_empty();
			capacity = c;
			used = 0;
			mem = (char*) new uint64_t[capacity / sizeof(uint64_t)]; //use uint64_t instead of char to guarantee alignment
		}
		void dealloc(){
			assert(capacity > 0 && mem != NULL);
			capacity = 0;
			used = 0;
			delete[] (uint64_t *)mem;
			mem = NULL;
		}
		void assert_empty(){ assert(capacity == 0 && used == 0 && mem == NULL); }
	};

	Chunk * chunks[MAX_CHUNKS];
	unsigned int numchunks;
	Mutex lock; //lock around malloc calls to add new chunks
	Data * freelist[MAX_NUM];

public:

	CompactTree() : numchunks(0) {
		for(unsigned int i = 0; i < MAX_CHUNKS; i++)
			chunks[i] = NULL;
		for(unsigned int i = 0; i < MAX_NUM; i++)
			freelist[i] = NULL;

		//allocate the first chunk
		chunks[numchunks++] = new Chunk(CHUNK_SIZE);
	}
	~CompactTree(){
		for(unsigned int i = 0; i < numchunks; i++){
			chunks[i]->dealloc();
			delete chunks[i];
		}
		numchunks = 0;
	}

	uint64_t memused(){
		if(numchunks == 0)
			return 0;

		return ((uint64_t)(numchunks - 1))*((uint64_t)CHUNK_SIZE) + chunks[numchunks - 1]->used;
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
			Chunk * c = chunks[numchunks-1];
			uint32_t used = c->used;
			if(used + size <= c->capacity){
				if(CAS(c->used, used, used+size))
					return new((Data *)(c->mem + used)) Data(num, parent);
				else
					continue;
			}else{
				lock.lock();

				if(c == chunks[numchunks-1]) //add a new chunk if this is still the last chunk
					chunks[numchunks++] = new Chunk(CHUNK_SIZE);

				lock.unlock();
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
	void compact(){
//		fprintf(stderr, "Compact\n");
		unsigned int dchunk = 0; //destination chunk
		unsigned int doff = 0;   //destination offset
		unsigned int schunk = 0; //source chunk
		unsigned int soff = 0;   //source offset
		unsigned int lastchunk = numchunks-1;
		//iterate over each chunk
		while(schunk < lastchunk || (schunk == lastchunk && soff < chunks[lastchunk]->used)){
			assert(schunk > dchunk || (schunk == dchunk && soff >= doff));

			//iterate over each Data block
			Data * s = (Data *)(chunks[schunk]->mem + soff);
//			fprintf(stderr, "%u, %u, %p, %u, %u, %p -> %u, %u", schunk, soff, s, s->header, s->num, s->parent, dchunk, doff);
			assert(s->num > 0 && s->num < MAX_NUM);
			int size = sizeof(Data) + sizeof(Node)*s->num;

			//move from -> to, update parent pointer
			if(s->header > 0){
				Data * d = NULL;

				//where to move
				while(1){
					Chunk * c = chunks[dchunk];
					if(doff + size <= c->capacity){ //if space, allocate from this chunk
						d = (Data *)(c->mem + doff);
						doff += size;
						break;
					}else{ //otherwise finish this chunk and prepare the next
						c->used = doff;

						//zero out the remainder of the chunk
						for(char * i = c->mem + c->used, * iend = c->mem + c->capacity ; i != iend; i++)
							*i = 0;

						dchunk++;
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
			if(soff >= chunks[schunk]->used){
				schunk++;
				soff = 0;
			}

//			fprintf(stderr, "\n");
		}
		//free unused chunks
		for(unsigned int i = dchunk+1; i < numchunks; i++)
			chunks[i]->dealloc();

		//save used last position
		chunks[dchunk]->used = doff;
		numchunks = dchunk + 1;

		//zero out the remainder of the chunk
		Chunk * ch = chunks[dchunk];
		for(char * i = ch->mem + ch->used, * iend = ch->mem + ch->capacity ; i != iend; i++)
			*i = 0;

		//clear the freelist
		for(unsigned int i = 0; i < MAX_NUM; i++)
			freelist[i] = NULL;

//		fprintf(stderr, "Compact done\n");
	}
};

#endif

