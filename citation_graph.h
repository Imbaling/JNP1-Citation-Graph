#ifndef _CITATION_GRAPH_H
#define _CITATION_GRAPH_H

#include <exception>
#include <vector>
#include <utility>
#include <map>
#include <set>
#include <memory>
#include <exception>

class PublicationAlreadyCreated : public std::exception {
	const char *what() const noexcept {
        return "PublicationAlreadyCreated";
    }
};

class PublicationNotFound : public std::exception {
	const char *what() const noexcept {
        return "PublicationNotFound";
    }
};

class TriedToRemoveRoot : public std::exception {
	const char *what() const noexcept {
        return "TriedToRemoveRoot";
    }
};

template <class Publication>
class CitationGraph {
    private:
        struct Node;

        using id_type = typename Publication::id_type;
        using map_type = typename std::map<id_type, std::weak_ptr<Node>>;

        struct Node {
            private:
                map_type *map;
                typename map_type::iterator map_iterator;

                std::shared_ptr<Publication> pointer;
                std::set<std::shared_ptr<Node>, std::owner_less<std::shared_ptr<Node>>> children;
				std::vector<std::weak_ptr<Node>> parents;

            public:
                Node(map_type *map_p, const id_type &value)
                    : map(map_p), pointer(std::make_shared<Publication>(value)) {
                }

                Node(const Node &) = delete;

                void setMapIterator(typename map_type::iterator const &it) {
                    map_iterator = it;
                }

                Publication& getPublication() const noexcept {
                    return *pointer;
                }

        		void addChild(std::shared_ptr<Node> const node) {
          			children.insert(node);
        		}
				
        		void addParent(std::shared_ptr<Node> const node) {
          			parents.push_back(node);
        		}
				
				void removeLastParent() noexcept { //TODO pop_back jest noexcept jesli wektor ma rozmiar wiekszy od 0, nie wiem czy to sprawdzac?
					parents.pop_back();
				}

        		std::vector<id_type> getChildrenIds() const {
        			std::vector<id_type> childrenIds;

                    for (const auto &child : children) {
                        childrenIds.push_back(child->getPublication().get_id());
                    }

                    return childrenIds;
        		}

        		std::vector<id_type> getParentsIds() const {
                	std::vector<id_type> parentsIds;

                    for (const auto &parent : parents) {
                        parentsIds.push_back(parent.lock()->getPublication().get_id());
                    }

                    return parentsIds;
        		}

                // TODO trzeba się upewnić, że for-each nie będzie rzucał wyjątków (iteratory)
                void removeNode() {
                    std::shared_ptr me{this};

                    for (const auto &parent : parents) {
                        //	parent.lock()->erase(me);
                    }
                }

                ~Node() noexcept {
                    map->erase(map_iterator);
                }
        };

        std::unique_ptr<map_type> publications;
    	std::shared_ptr<Node> root;

    public:
        // Creates the graph.
        // It also creates root with publication, which id is equal to stem_id.
        CitationGraph(id_type const &stem_id)
            : publications(std::make_unique<map_type>()),
              root(std::make_shared<Node>(publications.get(), stem_id)) {
            const auto it = publications->insert(typename map_type::value_type(stem_id, root));

            root->setMapIterator(it.first);
        }

        // Move constructor.
        CitationGraph(CitationGraph<Publication> &&other) noexcept
            : root(std::move(other.root)),
              publications(std::move(other.publications)) {
        }

        // Move assignment operator.
        CitationGraph<Publication>& operator=(CitationGraph<Publication> &&other) noexcept {
            root = std::move(other.root);
            publications = std::move(other.publications);

            return *this;
        }

        // Returns id of the root.
        id_type get_root_id() const noexcept(noexcept(std::declval<Publication>().get_id())) {
            return root->getPublication().get_id();
        }

        // Returns list of publications's ids citing the given publication.
        // Throws PublicationNotFound exception if the given publication does
        // not exist.
        std::vector<typename Publication::id_type> get_children(typename Publication::id_type const &id) const {
            const auto publication = publications->find(id);

            if (publication == publications->end()) {
                throw PublicationNotFound();
            }

            return publication->second.lock()->getChildrenIds();
    		}

        // Zwraca listę identyfikatorów publikacji cytowanych przez publikację o podanym
        // identyfikatorze. Zgłasza wyjątek PublicationNotFound, jeśli dana publikacja
        // nie istnieje.
        std::vector<typename Publication::id_type> get_parents(typename Publication::id_type const &id) const {
            const auto publication = publications->find(id);

            if (publication == publications->end()) {
                throw PublicationNotFound();
            }

            return publication->second.lock()->getParentsIds();
    		}

        // Sprawdza, czy publikacja o podanym identyfikatorze istnieje.
        bool exists(typename Publication::id_type const &id) const {
    			  return publications->find(id) != publications->end();
    	}

        // Zwraca referencję do obiektu reprezentującego publikację o podanym
        // identyfikatorze. Zgłasza wyjątek PublicationNotFound, jeśli żądana publikacja
        // nie istnieje.
        Publication& operator[](typename Publication::id_type const &id) const {
      		const auto publication = publications->find(id);

            if (publication == publications->end())
                throw PublicationNotFound();

      			return publication->second.lock()->getPublication();
    		}

        // Tworzy węzeł reprezentujący nową publikację o identyfikatorze id cytującą
        // publikacje o podanym identyfikatorze parent_id lub podanych identyfikatorach
        // parent_ids. Zgłasza wyjątek PublicationAlreadyCreated, jeśli publikacja
        // o identyfikatorze id już istnieje. Zgłasza wyjątek PublicationNotFound, jeśli
        // któryś z wyspecyfikowanych poprzedników nie istnieje albo lista poprzedników jest pusta.
        void create(typename Publication::id_type const &id, typename Publication::id_type const &parent_id) {
			const auto child_node = publications->find(id);
			if (child_node != publications->end())
				throw PublicationAlreadyCreated();
			
			const auto parent_node = publications->find(parent_id);
			if (parent_node == publications->end())
				throw PublicationNotFound();

			const auto node = std::make_shared<Node>(publications.get(), id); 
			const auto it = publications->insert(typename map_type::value_type(id, node));
            node->setMapIterator(it.first);
			
			try{
				add_citation(id, parent_id);
			} catch(std::exception &e) {
				//node->removeNode(); //TODO nie wiem czemu sie nie kompiluje, ale chyba w ogole nie jest potrzebne				
				throw e;
			}
		}
        void create(typename Publication::id_type const &id, typename std::vector<typename Publication::id_type> const &parent_ids) {
			const auto child_node = publications->find(id);
			if (child_node != publications->end())
				throw PublicationAlreadyCreated();
			
			for (auto parent_id : parent_ids) {
				const auto parent_node = publications->find(parent_id);
				if (parent_node == publications->end())
					throw PublicationNotFound();
			}
			
			const auto node = std::make_shared<Node>(publications.get(), id); 
			const auto it = publications->insert(typename map_type::value_type(id, node));
            node->setMapIterator(it.first);						

			for (auto parent_id : parent_ids) {
				try {
					const auto parent_node = publications->find(parent_id);
					add_citation(id, parent_id);
				} catch(std::exception &e) {
					//node->removeNode(); //TODO nie wiem czemu to sie nie kompiluje, ale powinno dzialac o ile uda sie skompilowac
					throw e;				
				} 
			}
		}

        // Dodaje nową krawędź w grafie cytowań. Zgłasza wyjątek PublicationNotFound,
        // jeśli któraś z podanych publikacji nie istnieje.
        void add_citation(typename Publication::id_type const &child_id, typename Publication::id_type const &parent_id) {
        	auto child_publication = publications->find(child_id);
        	auto parent_publication = publications->find(parent_id);
            
        	if (child_publication != publications->end() && parent_publication != publications->end()) {
				child_publication->second.lock()->addParent(parent_publication->second.lock());				
				try {
					parent_publication->second.lock()->addChild(child_publication->second.lock());
				} catch(std::exception &e) {
					child_publication->second.lock()->removeLastParent();
					throw e;						
				}				
        	} else {
          		throw PublicationNotFound();
        	}
      	}

        // Usuwa publikację o podanym identyfikatorze. Zgłasza wyjątek
        // PublicationNotFound, jeśli żądana publikacja nie istnieje. Zgłasza wyjątek
        // TriedToRemoveRoot przy próbie usunięcia pierwotnej publikacji.
        // W wypadku rozspójnienia grafu, zachowujemy tylko spójną składową zawierającą źródło.
        void remove(typename Publication::id_type const &id) {
      		auto publication = publications->find(id);

            if (publication == publication->end())
                throw PublicationNotFound();

            if (publication->lock()->getPublication().get_id() == id) //TODO to chyba zawsze jest prawda?
                throw TriedToRemoveRoot();

            publication->lock()->removeNode();
    	}
};

#endif // _CITATION_GRAPH_H
