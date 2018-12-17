#ifndef _CITATION_GRAPH_H
#define _CITATION_GRAPH_H

#include <exception>
#include <vector>
#include <utility>
#include <map>
#include <memory>

template <class Publication>
class CitationGraph {
    private:
        using id_type = typename Publication::id_type;

        struct Node {
            private:
                std::shared_ptr<Publication> pointer;
                std::vector<std::shared_ptr<Node>> children;

            public:
                // This function can throw exceptions.
                Node(const id_type &value)
                    : pointer(std::make_shared<Publication>(value)) {
                }

                Node(const Node &) = delete;

                Publication& getPublication() const noexcept {
                    return *pointer;
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
        std::vector<Publication::id_type> get_children(Publication::id_type const &id) const;

        // Zwraca listę identyfikatorów publikacji cytowanych przez publikację o podanym
        // identyfikatorze. Zgłasza wyjątek PublicationNotFound, jeśli dana publikacja
        // nie istnieje.
        std::vector<Publication::id_type> get_parents(Publication::id_type const &id) const;

        // Sprawdza, czy publikacja o podanym identyfikatorze istnieje.
        bool exists(Publication::id_type const &id) const;

        // Zwraca referencję do obiektu reprezentującego publikację o podanym
        // identyfikatorze. Zgłasza wyjątek PublicationNotFound, jeśli żądana publikacja
        // nie istnieje.
        Publication& operator[](Publication::id_type const &id) const;

        // Tworzy węzeł reprezentujący nową publikację o identyfikatorze id cytującą
        // publikacje o podanym identyfikatorze parent_id lub podanych identyfikatorach
        // parent_ids. Zgłasza wyjątek PublicationAlreadyCreated, jeśli publikacja
        // o identyfikatorze id już istnieje. Zgłasza wyjątek PublicationNotFound, jeśli
        // któryś z wyspecyfikowanych poprzedników nie istnieje albo lista poprzedników jest pusta.
        void create(Publication::id_type const &id, Publication::id_type const &parent_id);
        void create(Publication::id_type const &id, std::vector<Publication::id_type> const &parent_ids);

        // Dodaje nową krawędź w grafie cytowań. Zgłasza wyjątek PublicationNotFound,
        // jeśli któraś z podanych publikacji nie istnieje.
        void add_citation(Publication::id_type const &child_id, Publication::id_type const &parent_id);

        // Usuwa publikację o podanym identyfikatorze. Zgłasza wyjątek
        // PublicationNotFound, jeśli żądana publikacja nie istnieje. Zgłasza wyjątek
        // TriedToRemoveRoot przy próbie usunięcia pierwotnej publikacji.
        // W wypadku rozspójnienia grafu, zachowujemy tylko spójną składową zawierającą źródło.
        void remove(Publication::id_type const &id);
};

class PublicationAlreadyCreated : public std::exception {
};

class PublicationNotFound : public std::exception {
};

class TriedToRemoveRoot : public std::exception {
};

#endif // _CITATION_GRAPH_H
