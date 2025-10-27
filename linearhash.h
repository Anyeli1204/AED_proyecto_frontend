#include <stdexcept>
#include <iostream>

using namespace std;

const float maxFillFactor = 0.75;
const float lowerBound = 0.4;

template <typename TK, typename TV>
struct LinearHashNode {
	TK key; TV value;
	LinearHashNode* next;
	LinearHashNode() = default;
	LinearHashNode(TK key, TV value): key(key), value(value), next(nullptr) {}
	LinearHashNode(TK key, TV value, LinearHashNode* next): key(key), value(value), next(next) {}
};

template<typename TK, typename TV>
class LinearHashListIterator {
	LinearHashNode<TK, TV>* current;
public:
	LinearHashListIterator(LinearHashNode<TK, TV>* current): current(current) {}
	LinearHashListIterator& operator++() {current = current->next; return *this;}
	bool operator !=(const LinearHashListIterator& other) {return current != other.current;}
	bool operator==(const LinearHashListIterator& other) {return current == other.current;}
	LinearHashNode<TK, TV>& operator*() {
		if (current == nullptr) throw std::runtime_error("Iterator does not have an associated node");
		return *current;
	}
};


template<typename TK, typename TV>
class LinearHash {
	typedef LinearHashNode<TK, TV> Node;
	typedef LinearHashListIterator<TK, TV> Iterator;
	Node** array;
	int* bucket_sizes; int visited;
	int M0, p, i, datacount, bucketcount, capacity;
	double fillFactor(){
		return double(datacount) / bucketcount; //aquí el cálculo es diferente a una tabla hash
	}
public:
	LinearHash(int M0=4): M0(M0), array(new Node*[M0]()), bucket_sizes(new int[M0]()),
	bucketcount(M0), p(0), i(0), datacount(0), capacity(M0), visited(0) {
		for (int i=0; i<bucketcount; ++i) {array[i] = nullptr; bucket_sizes[i] = 0;}
	}
private:
	size_t hash_index(TK key) {
		std::hash<TK> ptr_hash;
		size_t base_hash = ptr_hash(key);
		size_t currindex = base_hash % ((1<<i)*M0);
		size_t extindex = base_hash % ((1<<(i+1))*M0);
		if (currindex < p) return extindex; return currindex;
	}
	size_t extended_hash_index(TK key) {
		std::hash<TK> ptr_hash;
		size_t base_hash = ptr_hash(key);
		return base_hash % ((1<<(i+1))*M0);
	}
public:
	int visited_buckets() {return visited;}
	int size() {return datacount;}
	int bucket_count() {return bucketcount;}
	int bucket_size(int index) {
		if(index < 0 || index >= bucketcount) throw std::runtime_error("Invalid bucket index");
		return bucket_sizes[index];
	}
	void insert(TK key, TV value) {
		size_t index = hash_index(key);
		//si la key ya existe
		Node* current = array[index];
		while(current != nullptr){
			++visited;
			if(current->key == key) {current->value = value; return;}
			current = current->next;
		}
		//si la key no existe (si existe ya habría retornado)
		Node* newNode = new Node(key, value);
		newNode->next = array[index];
		++visited;
		array[index] = newNode;
		datacount++;
		bucket_sizes[index]++;
		if (fillFactor() > maxFillFactor) split();
	}
	TV operator[](TK key) {
		size_t index = hash_index(key);
		Node* current = array[index];
		while (current != nullptr) {
			++visited;
			if (current->key == key) {return current->value;}
			current = current->next;
		}
		throw std::runtime_error("Key not found in linear hashing");
	}
	bool remove(TK key) {
		size_t index = hash_index(key);
		Node* current = array[index];
		if (current == nullptr) return false;
		++visited;
		if (current->key == key) {
			auto temp = array[index];
			array[index] = array[index]->next;
			delete temp; temp = nullptr; --datacount; --bucket_sizes[index];
			if (fillFactor() < lowerBound && capacity > M0) merge(); return true;
		}
		while(current->next != nullptr){
			++visited;
			if (current->next->key == key) {
				auto temp = current->next;
				current->next = current->next->next;
				delete temp; temp = nullptr; --datacount; --bucket_sizes[index];
				if (fillFactor() < lowerBound && capacity > M0) merge(); return true;
			}
			current = current->next;
		} return false;

	}
	bool contains(TK key) {
		size_t index = hash_index(key);
		Node* current = array[index];
		while(current != nullptr){
			++visited;
			if(current->key == key) return true;
			current = current->next;
		} return false;
	}
private:
	void split() {
		if (p == 0) {
			auto oldcapacity = capacity; capacity *= 2;
			Node** new_array = new Node*[capacity]();
			int* new_bucket_sizes = new int[capacity]();
			for (int i=0; i<oldcapacity; ++i) {
				new_bucket_sizes[i] = bucket_sizes[i];
				new_array[i] = array[i];
			}
			for (int i=oldcapacity; i<capacity; ++i) {
				new_bucket_sizes[i] = 0; new_array[i] = nullptr;
			}
			delete[] array; delete[] bucket_sizes;
			array = new_array; bucket_sizes = new_bucket_sizes;
		}
		++bucketcount;
		Node* currnode = array[p];
		Node* prevnode = nullptr;
		while (currnode != nullptr) {
			++visited;
			Node* nextnode = currnode->next;
			size_t newindex = extended_hash_index(currnode->key);
			if (newindex != p) {
				if (prevnode != nullptr) prevnode->next = nextnode;
				else array[p] = nextnode;
				currnode->next = array[newindex];
				array[newindex] = currnode;
				++bucket_sizes[newindex]; --bucket_sizes[p];
			} else prevnode = currnode;
			currnode = nextnode;
		}
		++p;
		if (p == (M0 * (1 << i))) {++i; p = 0;}
	}

	void merge() {
		if (p == 0) {--i; p = M0 * (1 << i);} else --p;
		auto curr = array[p];
		bucket_sizes[p] += bucket_sizes[bucketcount-1]; bucket_sizes[bucketcount-1] = 0;
		if (curr == nullptr) array[p] = array[bucketcount-1];
		else {
			while (curr->next != nullptr) {++visited; curr = curr->next;}
			curr->next = array[bucketcount-1];
		} array[bucketcount-1] = nullptr;
		--bucketcount;
		if (p == 0) {
			capacity /= 2;
			Node** new_array = new Node*[capacity]();
			int* new_bucket_sizes = new int[capacity]();
			for (int i=0; i<capacity; ++i) {
				new_bucket_sizes[i] = bucket_sizes[i];
				new_array[i] = array[i];
			}
			delete[] array; delete[] bucket_sizes;
			array = new_array; bucket_sizes = new_bucket_sizes;
		}
	}
public:

	Iterator begin(int index) {return Iterator(array[index]);}
	Iterator end(int index) {return Iterator(nullptr);}


	~LinearHash() {
		for (int i=0; i<capacity; ++i) {
			while (array[i] != nullptr) {
				auto temp = array[i];
				array[i] = array[i]->next;
				delete temp;
			}
		}
		delete[] array;
		array = nullptr;
		delete[] bucket_sizes;
		bucket_sizes = nullptr;
	}
};