#include <stdexcept>
#include <iostream>
#include <vector>

using namespace std;

// Factor máximo de carga aceptado: datacount / bucketcount
const float maxFillFactor = 0.75;
// Límite inferior de factor de carga para empezar a hacer merge
const float lowerBound = 0.4;

// Cada bucket es una lista enlazada de nodos LinearHashNode
// TK = tipo de la clave (key), TV = tipo del valor (value)
template <typename TK, typename TV>
struct LinearHashNode {
	TK key; TV value;   // string token y struct Sesion
	LinearHashNode* next;	// chaining
	LinearHashNode() = default;
	LinearHashNode(TK key, TV value): key(key), value(value), next(nullptr) {}
	LinearHashNode(TK key, TV value, LinearHashNode* next): key(key), value(value), next(next) {}
};

// Permite recorrer la lista enlazada de un bucket como si fuera un contenedor
template<typename TK, typename TV>
class LinearHashListIterator {
	LinearHashNode<TK, TV>* current;
public:
	LinearHashListIterator(LinearHashNode<TK, TV>* current): current(current) {}
	// ++it: avanzar al siguiente nodo
	LinearHashListIterator& operator++() {current = current->next; return *this;}
	// Comparación de iteradores (distinto)
	bool operator !=(const LinearHashListIterator& other) {return current != other.current;}
	// Comparación de iteradores (igual)
	bool operator==(const LinearHashListIterator& other) {return current == other.current;}
	// Desreferenciar iterador: devuelve una referencia al nodo actual
	LinearHashNode<TK, TV>& operator*() {
		if (current == nullptr) throw std::runtime_error("Iterator does not have an associated node");
		return *current;
	}
};


template<typename TK, typename TV>
class LinearHash {
	// Alias internos para simplificar código
	typedef LinearHashNode<TK, TV> Node;
	typedef LinearHashListIterator<TK, TV> Iterator;

	Node** array;   // Arreglo de punteros a lista de nodos: los buckets físicos
	int* bucket_sizes;   // Arreglo con la cantidad de elementos en cada bucket
	int visited;   // Contador de nodos visitados (para estadísticas)
	// Parámetros y estado del Linear Hashing:
	// M0: cantidad base de buckets (tamaño inicial)
	// p:  índice del próximo bucket lógico a dividir (split pointer)
	// i:  nivel de expansión (indica cuántas veces hemos duplicado la tabla)
	// datacount: cantidad total de claves almacenadas
	// bucketcount: número de buckets lógicos activos
	// capacity: capacidad física del array (puede ser > bucketcount por splits/merges)
	int M0, p, i, datacount, bucketcount, capacity;
	double fillFactor(){
		// Diferente a una tabla hash tradicional: aquí usamos bucketcount, no capacity
		return double(datacount) / bucketcount;
	}
public:
	//  M0: cantidad de buckets iniciales.
	//  Se inicializa el array de buckets y el arreglo de tamaños en 0
	// Inicializar todos los buckets apuntando a nullptr y tamaños en 0
	LinearHash(int M0=4): M0(M0), array(new Node*[M0]()), bucket_sizes(new int[M0]()),
	bucketcount(M0), p(0), i(0), datacount(0), capacity(M0), visited(0) {
		for (int i=0; i<bucketcount; ++i) {array[i] = nullptr; bucket_sizes[i] = 0;}
	}
private:

	// Devuelve el índice de bucket donde debe ir una clave "key"
	// Aplica la lógica de:
	//  - hash base
	//  - módulo con M0 * 2^i
	//  - si el índice cae en un bucket ya dividido (currindex < p), se usa la versión extendida (M0 * 2^(i+1))
	size_t hash_index(TK key) {
		std::hash<TK> ptr_hash;
		size_t base_hash = ptr_hash(key);
		size_t currindex = base_hash % ((1<<i)*M0);
		size_t extindex = base_hash % ((1<<(i+1))*M0);
		if (currindex < p) return extindex; return currindex;
	}
	// Versión siempre extendida del hash (para usar en split)
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
		// 1. Calcular el índice físico donde debería caer la clave
		size_t index = hash_index(key);
		// 2. Buscar si la clave ya existe en la lista del bucket
		Node* current = array[index];
		while(current != nullptr){
			++visited;
			// Si existe, solo actualizamos el valor y salimos
			if(current->key == key) {current->value = value; return;}
			current = current->next;
		}
		// 3. Si la clave no existe, creamos un nuevo nodo y lo insertamos al inicio de la lista
		Node* newNode = new Node(key, value);
		newNode->next = array[index];
		++visited;	// visitamos la posición de inserción
		array[index] = newNode;
		// Actualizar contadores globales
		datacount++;
		bucket_sizes[index]++;
		// 4. Si el factor de carga supera el máximo permitido, se hace un split
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

	// Devuelve true si se eliminó algo, false si la clave no existía
	bool remove(TK key) {
		size_t index = hash_index(key);
		Node* current = array[index];
		// Caso 1: bucket vacío
		if (current == nullptr) return false;
		++visited;
		// Caso 2: el primer nodo contiene la clave
		if (current->key == key) {
			auto temp = array[index];
			array[index] = array[index]->next;
			delete temp; temp = nullptr; --datacount; --bucket_sizes[index];
			// Si el factor de carga está por debajo del límite inferior y la capacidad física es mayor que M0, hacemos merge
			if (fillFactor() < lowerBound && capacity > M0) merge(); return true;
		}

		// Caso 3: la clave está en algún nodo intermedio o al final
		while(current->next != nullptr){
			++visited;
			if (current->next->key == key) {
				auto temp = current->next;
				current->next = current->next->next;
				delete temp; temp = nullptr; --datacount; --bucket_sizes[index];
				if (fillFactor() < lowerBound && capacity > M0) merge(); return true;
			}
			current = current->next;
		}
		return false;
	}

	// trivial
	bool contains(TK key) {
		size_t index = hash_index(key);
		Node* current = array[index];
		while(current != nullptr){
			++visited;
			if(current->key == key) return true;
			current = current->next;
		} return false;
	}

	// Borra todos los nodos de todos los buckets y resetea contadores
	void clear() {
		for (int b = 0; b < bucketcount; ++b) {
			Node* curr = array[b];
			while (curr != nullptr) {
				Node* temp = curr;
				curr = curr->next;
				delete temp;
			}
			array[b] = nullptr;
			bucket_sizes[b] = 0;
		}
		datacount = 0;
		visited = 0;
		// Nota: p, i, bucketcount, capacity se mantienen
	}

	// Devuelve true si encuentra la clave, false si no. En caso de éxito, out_value se llena con el valor correspondiente (struct Sesion)
	bool try_get(TK key, TV &out_value) {
		size_t index = hash_index(key);
		Node* current = array[index];
		while (current != nullptr) {
			++visited;
			if (current->key == key) {
				out_value = current->value;
				return true;
			}
			current = current->next;
		}
		return false;
	}

	// Log tras cada interacción con LinearHashing
	// Muestra en consola la configuración interna de la estructura
	// y todos los buckets con sus claves.
	void debug_print(const char* label = "") {
		cout << "\n========== ESTADO LinearHash " << label << " ==========\n" << flush;
		cout << "M0=" << M0
			 << "  i=" << i
			 << "  p=" << p
			 << "  bucketcount=" << bucketcount
			 << "  capacity=" << capacity
			 << "  datacount=" << datacount
			 << "  fillFactor=" << fillFactor()
			 << "\n" << flush;
		for (int b = 0; b < bucketcount; ++b) {
			cout << "Bucket " << b << " (size=" << bucket_sizes[b] << "): ";
			Node* curr = array[b];
			if (!curr) {
				cout << "[vacio]";
			} else {
				while (curr) {
					cout << curr->key;
					if (curr->next) cout << " -> ";
					curr = curr->next;
				}
			}
			cout << "\n";
		}
		cout << "===========================================\n" << flush;
	}

	// Recorre todos los elementos de la tabla y aplica una función callback
	// La función callback recibe: (TK key, TV& value) -> bool
	// Si retorna true, el elemento se elimina; si retorna false, se mantiene
	template<typename Func>
	int for_each_remove_if(Func callback) {
		int eliminados = 0;
		std::vector<TK> tokens_a_eliminar;
		
		// Primera pasada: identificar tokens a eliminar
		for (int b = 0; b < bucketcount; ++b) {
			Node* curr = array[b];
			while (curr != nullptr) {
				++visited;
				if (callback(curr->key, curr->value)) {
					tokens_a_eliminar.push_back(curr->key);
				}
				curr = curr->next;
			}
		}
		
		// Segunda pasada: eliminar los tokens identificados
		for (const TK& token : tokens_a_eliminar) {
			if (remove(token)) {
				++eliminados;
			}
		}
		
		return eliminados;
	}

private:
	// Se llama cuando el factor de carga supera maxFillFactor.
	// Puede duplicar la capacidad física del array
	// Reubica elementos del bucket p hacia el nuevo bucket según el hash extendido.
	void split() {
		// Si p == 0 significa que vamos a pasar a un nuevo "ciclo" de splits
		// y necesitamos duplicar la capacidad física
		if (p == 0) {
			// Crear nuevos arreglos con la nueva capacidad
			auto oldcapacity = capacity; capacity *= 2;
			Node** new_array = new Node*[capacity]();
			int* new_bucket_sizes = new int[capacity]();
			// Copiar el contenido de los buckets existentes
			for (int i=0; i<oldcapacity; ++i) {
				new_bucket_sizes[i] = bucket_sizes[i];
				new_array[i] = array[i];
			}
			// Inicializar la nueva mitad
			for (int i=oldcapacity; i<capacity; ++i) {
				new_bucket_sizes[i] = 0; new_array[i] = nullptr;
			}
			// Liberar los arreglos antiguos
			delete[] array; delete[] bucket_sizes;
			array = new_array; bucket_sizes = new_bucket_sizes;
		}
		// Aumentamos la cantidad de buckets lógicos (uno más se activa)
		++bucketcount;
		// Reubicamos nodos del bucket p usando el hash extendido
		Node* currnode = array[p];
		Node* prevnode = nullptr;
		while (currnode != nullptr) {
			++visited;
			Node* nextnode = currnode->next;
			// Índice nuevo con la tabla extendida
			size_t newindex = extended_hash_index(currnode->key);
			if (newindex != p) {
				// El nodo debe moverse al nuevo bucket "newindex"
				if (prevnode != nullptr) prevnode->next = nextnode;
				else array[p] = nextnode;
				// Insertar nodo movido al inicio del bucket newindex
				currnode->next = array[newindex];
				array[newindex] = currnode;
				++bucket_sizes[newindex]; --bucket_sizes[p];
			} else {
				// El nodo se queda en el bucket p
				prevnode = currnode;
			}
			currnode = nextnode;
		}
		// Avanzar el puntero de split
		++p;
		// Si ya hemos dividido todos los buckets del rango actual,
		// incrementamos i (nuevo nivel) y reiniciamos p a 0
		if (p == (M0 * (1 << i))) {
			++i; p = 0;
		}
	}


	// Se llama cuando el factor de carga baja de lowerBound
	// Junta el último bucket con el bucket p-1 (en orden lógico inverso).
	// Puede reducir la capacidad física a la mitad
	void merge() {
		// Ajustamos p hacia atrás:
		// Si p == 0, retrocedemos nivel (i--) y ponemos p al último bucket del nivel
		// Si p > 0, simplemente decrementamos p.
		if (p == 0) {
			--i; p = M0 * (1 << i);
		} else --p;
		// Curr apunta al bucket p donde vamos a fusionar
		auto curr = array[p];
		// Agregar al tamaño del bucket p los elementos del último bucket lógico
		bucket_sizes[p] += bucket_sizes[bucketcount-1]; bucket_sizes[bucketcount-1] = 0;
		// Si el bucket p estaba vacío, simplemente apuntamos al último bucket
		if (curr == nullptr) array[p] = array[bucketcount-1];
		else {
			// Si no está vacío, vamos al final de la lista para "concatenar"
			while (curr->next != nullptr) {++visited; curr = curr->next;}
			curr->next = array[bucketcount-1];
		}
		// El último bucket lógico queda vacío
		array[bucketcount-1] = nullptr;
		// Disminuimos la cantidad de buckets lógicos
		--bucketcount;
		// Si p vuelve a ser 0, quiere decir que hemos "bajado" a un nivel anterior
		// y podemos reducir la capacidad física a la mitad
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

	// Libera toda la memoria de los nodos y de los arreglos.
	~LinearHash() {
		// Borrar todos los nodos de todos los buckets físicos
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