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
                    : map(map_p),
											map_iterator(map_p->end()),
											pointer(std::make_shared<Publication>(value)) {
                }

                Node(const Node &) = delete;

                void setMapIterator(typename map_type::iterator const &it) {
                    map_iterator = it;
                }

                Publication& getPublication() const noexcept {
                    return *pointer;
                }

                bool exists(std::shared_ptr<Node> node) noexcept {
                    return children.find(node) != children.end();
                }

		        		void addChild(std::shared_ptr<Node> const &node) {
		          			children.insert(node);
		        		}

		        		void addParent(std::weak_ptr<Node> const &node) {
		          			parents.push_back(node);
		        		}

								void removeLastParent() noexcept {
										if (!parents.empty())
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
												if (parent.expired())
														continue;

                        parentsIds.push_back(parent.lock()->getPublication().get_id());
                    }

                    return parentsIds;
		        		}

								void erase(const std::shared_ptr<Node> &son) noexcept {
										children.erase(son);
								}

                void removeNode(const std::shared_ptr<Node> &me) {
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
            const auto it = publications->insert(typename map_type::value_type(stem_id, root));

            root->setMapIterator(it.first);
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

        // Returns list of publications's ids cited by the given publication.
        // Throws PublicationNotFound exception if the given publication does
        // not exist.
        std::vector<typename Publication::id_type> get_parents(typename Publication::id_type const &id) const {
            const auto publication = publications->find(id);

            if (publication == publications->end()) {
                throw PublicationNotFound();
            }

            return publication->second.lock()->getParentsIds();
    		}

        // Check if the given publication exists.
        bool exists(typename Publication::id_type const &id) const {
    			  return publications->find(id) != publications->end();
    	  }

        // Returns refference to a representation of publication of the given id.
        // Throws PublicationNotFound exception if the given publication does
        // not exist.
        Publication& operator[](typename Publication::id_type const &id) const {
      		const auto publication = publications->find(id);

            if (publication == publications->end())
                throw PublicationNotFound();

      			return publication->second.lock()->getPublication();
    		}
        
        // Creates node representing new publication which cites publication 
        // identified by parent_id or publications identified by vector parents_id.
        // Throws exception PublicationAlreadyCreated if publication identified by id 
        // doesen`t exist. Throws exception PublicationNotFound if one of the arguments 
        // doesen`t exist or vector is empty.
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

						try {
					      node->setMapIterator(it.first);
						} catch (...) {
								publications->erase(it.first);
								throw;
						}

						for (auto parent_id : parent_ids) {
								try {
										add_citation(id, parent_id);
								} catch(std::exception &e) {
									  node->removeNode(node);
										throw e;
								}
						}
				}

        void create(typename Publication::id_type const &id, typename Publication::id_type const &parent_id) {
						std::vector<typename Publication::id_type> parents_ids = {parent_id};

						create(id, parents_ids);
				}

        // Adds new edge in the Citation Graph. Throws exception 
        // PublicationNotFound if one of the publications doesen`t exist.
        void add_citation(typename Publication::id_type const &child_id, typename Publication::id_type const &parent_id) {
	        	auto child_publication = publications->find(child_id);
	        	auto parent_publication = publications->find(parent_id);

	        	if (child_publication != publications->end() && parent_publication != publications->end()) {
					if (parent_publication->second.lock()->exists(child_publication->second.lock()) == false) {
					   child_publication->second.lock()->addParent(parent_publication->second);
                    
                       try {
						  parent_publication->second.lock()->addChild(child_publication->second.lock());
					   } catch(std::exception &e) {
						  child_publication->second.lock()->removeLastParent();
						  throw e;
					   }
                    }
	        	} else {
	          		throw PublicationNotFound();
	        	}
      	}

        // Removes publication identified by id. Throws exception PublicationNotFound if the
        // Publication doesen`t exist. Throws exception TriedToremoveRoot while attempting
        // to remove the first publication. In the case of the disconnectedness of the graph
        // we leave only the connected component.
        void remove(typename Publication::id_type const &id) {
	      		auto publication = publications->find(id);

            if (publication == publications->end())
                throw PublicationNotFound();

            if (root->getPublication().get_id() == id)
                throw TriedToRemoveRoot();

            publication->second.lock()->removeNode(publication->second.lock());
    	}
};

#endif // _CITATION_GRAPH_H
