#ifndef _CITATION_GRAPH_H
#define _CITATION_GRAPH_H

#include <exception>
#include <vector>
#include <utility>
#include <map>
#include <memory>

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
        using id_type = typename Publication::id_type;

        struct Node {
            private:
                std::shared_ptr<Publication> pointer;
                std::vector<std::shared_ptr<Node>> children;
				std::vector<std::shared_ptr<Node>> parents;

            public:
                // This function can throw exceptions.
                Node(const id_type &value)
                    : pointer(std::make_shared<Publication>(value)) {
                }

                Node(const Node &) = delete;

                Publication& getPublication() const noexcept {
                    return *pointer;
                }
				
				void addChild(std::shared_ptr<Node> const node) {
					children.push_back(node);
				}
			
				void addParent(std::shared_ptr<Node> const node) {
					parents.push_back(node);
				}

				std::vector<std::shared_ptr<Node>>& getChildren() noexcept {
					return children;
				}
				
				std::vector<std::shared_ptr<Node>>& getParents() noexcept {
					return parents;
				}
        };

        using map_type = typename std::map<id_type, std::shared_ptr<Node>>;

		std::shared_ptr<Node> root;
        std::unique_ptr<map_type> publications;

    public:
        // Creates the graph.
        // It also creates root with publication, which id is equal to stem_id.
        CitationGraph(id_type const &stem_id)
            : root(std::make_shared<Node>(stem_id)),
              publications(std::make_unique<map_type>()) {
            publications->insert(typename map_type::value_type(stem_id, root));
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

        // Zwraca listę identyfikatorów publikacji cytujących publikację o podanym
        // identyfikatorze. Zgłasza wyjątek PublicationNotFound, jeśli dana publikacja
        // nie istnieje.
        std::vector<typename Publication::id_type> get_children(typename Publication::id_type const &id) const {
			auto publication = publications->find(id);
			
			if (publication != publications->end()) {
				auto children_nodes = publication->second->getChildren();
				std::unique_ptr<std::vector<typename Publication::id_type>> children_ids;				
				
				for (auto child : children_nodes)
					children_ids->push_back(child->getPublication().get_id());

				return *children_ids;
			} else {
				throw PublicationNotFound();			
			}
		}

        // Zwraca listę identyfikatorów publikacji cytowanych przez publikację o podanym
        // identyfikatorze. Zgłasza wyjątek PublicationNotFound, jeśli dana publikacja
        // nie istnieje.
        std::vector<typename Publication::id_type> get_parents(typename Publication::id_type const &id) const {
			auto publication = publications->find(id);
			
			if (publication != publications->end()) {
				auto parents_nodes = publication->second->getParents();
				std::unique_ptr<std::vector<typename Publication::id_type>> parents_ids;				
				
				for (auto parent : parents_nodes)
					parents_ids->push_back(parent->getPublication().get_id());

				return *parents_ids;
			} else {
				throw PublicationNotFound();			
			}		
		}

        // Sprawdza, czy publikacja o podanym identyfikatorze istnieje.
        bool exists(typename Publication::id_type const &id) const {
			return publications->find(id) != publications->end();		
		}

        // Zwraca referencję do obiektu reprezentującego publikację o podanym
        // identyfikatorze. Zgłasza wyjątek PublicationNotFound, jeśli żądana publikacja
        // nie istnieje.
        Publication& operator[](typename Publication::id_type const &id) const {
			auto publication = publications->find(id);
		
			if (publication != publications->end()) {
				return publication->second->getPublication();		
			} else {
				throw PublicationNotFound();			
			}		
		}

        // Tworzy węzeł reprezentujący nową publikację o identyfikatorze id cytującą
        // publikacje o podanym identyfikatorze parent_id lub podanych identyfikatorach
        // parent_ids. Zgłasza wyjątek PublicationAlreadyCreated, jeśli publikacja
        // o identyfikatorze id już istnieje. Zgłasza wyjątek PublicationNotFound, jeśli
        // któryś z wyspecyfikowanych poprzedników nie istnieje albo lista poprzedników jest pusta.
        void create(typename Publication::id_type const &id, typename Publication::id_type const &parent_id);
        void create(typename Publication::id_type const &id, typename std::vector<typename Publication::id_type> const &parent_ids);

        // Dodaje nową krawędź w grafie cytowań. Zgłasza wyjątek PublicationNotFound,
        // jeśli któraś z podanych publikacji nie istnieje.
        void add_citation(typename Publication::id_type const &child_id, typename Publication::id_type const &parent_id) {
			auto child_publication = publications->find(child_id);
			auto parent_publication = publications->find(parent_id);

			if (child_publication != publications->end() && parent_publication != publications->end()) {
				parent_publication->second->addChild(child_publication->second);
				child_publication->second->addParent(parent_publication->second);								
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
			
			if (publication != publications->end()) {
				if (publication->second != root) { //TODO pewnie zle porownanie
					publications->erase(publication);
				} else {
					throw TriedToRemoveRoot();				
				}
			} else {
				throw PublicationNotFound();
			}
		}
};

#endif // _CITATION_GRAPH_H
