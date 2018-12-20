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
                std::set<std::shared_ptr<Node>,
												 std::owner_less<std::shared_ptr<Node>>> children;
								std::vector<std::weak_ptr<Node>> parents;

            public:
                Node(map_type *map_p, const id_type &value)
                    : map(map_p),
											map_iterator(map_p->end()),
											pointer(std::make_shared<Publication>(value)) {
                }

                Node(const Node &) = delete;

                void set_map_iterator(typename map_type::iterator const &it) {
                    map_iterator = it;
                }

                Publication& get_publication() const noexcept {
                    return *pointer;
                }

                bool exists(std::shared_ptr<Node> node) noexcept {
                    return children.find(node) != children.end();
                }

		        		void add_child(std::shared_ptr<Node> const &node) {
		          			children.insert(node);
		        		}

		        		void add_parent(std::weak_ptr<Node> const &node) {
		          			parents.push_back(node);
		        		}

								void remove_last_parent() noexcept {
										if (!parents.empty())
												parents.pop_back();
								}

		        		std::vector<id_type> get_children_ids() const {
			        			std::vector<id_type> children_ids;

                    for (const auto &child : children) {
                        children_ids.push_back(child->get_publication().get_id());
                    }

                    return children_ids;
		        		}

		        		std::vector<id_type> get_parents_ids() const {
	                	std::vector<id_type> parents_ids;

                    for (const auto &parent : parents) {
												if (parent.expired())
														continue;

                        parents_ids.push_back(parent.lock()->get_publication().get_id());
                    }

                    return parents_ids;
		        		}

								void erase(const std::shared_ptr<Node> &son) noexcept {
										children.erase(son);
								}

                void remove_node(const std::shared_ptr<Node> &me) {
                    for (const auto &parent : parents) {
                        	parent.lock()->erase(me);
                    }
                }

                ~Node() noexcept {
										if (map_iterator != map->end())
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
            const auto it =
								publications->insert(typename map_type::value_type(stem_id, root));

            root->set_map_iterator(it.first);
        }

        // Move constructor.
        CitationGraph(CitationGraph<Publication> &&other) noexcept
            : publications(std::move(other.publications)),
							root(std::move(other.root)) {
        }

        // Move assignment operator.
        CitationGraph<Publication>& operator=(CitationGraph<Publication> &&other) noexcept {
            root = std::move(other.root);
            publications = std::move(other.publications);

            return *this;
        }

        // Returns id of the root.
        id_type get_root_id() const noexcept(noexcept(std::declval<Publication>().get_id())) {
            return root->get_publication().get_id();
        }

        // Returns list of publications's ids citing the given publication.
        // Throws PublicationNotFound exception if the given publication does
        // not exist.
        std::vector<id_type> get_children(id_type const &id) const {
            const auto publication = publications->find(id);

            if (publication == publications->end()) {
                throw PublicationNotFound();
            }

            return publication->second.lock()->get_children_ids();
    		}

        // Returns list of publications's ids cited by the given publication.
        // Throws PublicationNotFound exception if the given publication does
        // not exist.
        std::vector<id_type> get_parents(id_type const &id) const {
            const auto publication = publications->find(id);

            if (publication == publications->end()) {
                throw PublicationNotFound();
            }

            return publication->second.lock()->get_parents_ids();
    		}

        // Check if the given publication exists.
        bool exists(id_type const &id) const {
    			  return publications->find(id) != publications->end();
    	  }

        // Returns refference to a representation of publication of the given id.
        // Throws PublicationNotFound exception if the given publication does
        // not exist.
        Publication& operator[](id_type const &id) const {
      		const auto publication = publications->find(id);

            if (publication == publications->end())
                throw PublicationNotFound();

      			return publication->second.lock()->get_publication();
    		}

        // Creates node representing new publication which cites publication
        // identified by parent_id or publications identified by vector parents_id.
        // Throws exception PublicationAlreadyCreated if publication identified by id
        // doesn`t exist. Throws exception PublicationNotFound if one of the arguments
        // doesn`t exist or vector is empty.
				void create(id_type const &id, typename std::vector<id_type> const &parent_ids) {
						const auto child_node = publications->find(id);
						if (child_node != publications->end())
								throw PublicationAlreadyCreated();

						for (const auto &parent_id : parent_ids) {
								const auto parent_node = publications->find(parent_id);
								if (parent_node == publications->end())
										throw PublicationNotFound();
						}

						if (parent_ids.empty())
								throw PublicationNotFound();

						const auto node = std::make_shared<Node>(publications.get(), id);
						const auto it = publications->insert(typename map_type::value_type(id, node));

						try {
					      node->set_map_iterator(it.first);
						} catch (...) {
								publications->erase(it.first);
								throw;
						}

						for (const auto &parent_id : parent_ids) {
								try {
										add_citation(id, parent_id);
								} catch(...) {
									  node->remove_node(node);
										throw;
								}
						}
				}

        void create(id_type const &id, id_type const &parent_id) {
						std::vector<id_type> parents_ids = {parent_id};

						create(id, parents_ids);
				}

        // Adds new edge in the Citation Graph. Throws exception
        // PublicationNotFound if one of the publications doesn`t exist.
        void add_citation(id_type const &child_id, id_type const &parent_id) {
	        	auto child_publication = publications->find(child_id);
	        	auto parent_publication = publications->find(parent_id);

	        	if (child_publication != publications->end() && parent_publication != publications->end()) {
								if (parent_publication->second.lock()->exists(child_publication->second.lock()) == false) {
								   child_publication->second.lock()->add_parent(parent_publication->second);

			             try {
										  parent_publication->second.lock()->add_child(child_publication->second.lock());
								   } catch(...) {
										  child_publication->second.lock()->remove_last_parent();
										  throw;
								   }
			          }
	        	} else {
	          		throw PublicationNotFound();
	        	}
      	}

        // Removes publication identified by id. Throws exception PublicationNotFound if the
        // publication doesn`t exist. Throws exception TriedToremoveRoot while attempting
        // to remove the first publication. In the case of the disconnectedness of the graph
        // we leave only the component connected to the root.
        void remove(id_type const &id) {
	      		auto publication = publications->find(id);

            if (publication == publications->end())
                throw PublicationNotFound();

            if (root->get_publication().get_id() == id)
                throw TriedToRemoveRoot();

            publication->second.lock()->remove_node(publication->second.lock());
    	}
};

#endif // _CITATION_GRAPH_H
