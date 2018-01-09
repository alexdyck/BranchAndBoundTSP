#ifndef BRANCHANDBOUNDTSP_TSP_HPP
#define BRANCHANDBOUNDTSP_TSP_HPP

#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <string>
#include <climits>
#include <vector>
#include <sstream>
#include <queue>
#include <numeric>
#include "util.hpp"



// TODO: Split header and cpp methods
namespace TSP {
    using size_type = std::size_t;
    using NodeId = size_type;
    using EdgeId = size_type;

    EdgeId to_EdgeId(NodeId i , NodeId j, size_type N) {
        if ( i == j)
            throw std::runtime_error("Loops are not contained in this instace");
        if (i>j)
            std::swap(i,j);
        return (i)*(N) +j;
    }

    void to_NodeId(EdgeId e , NodeId & i, NodeId & j, size_type N) {
        j = e % (N);
        i = (e - j)/(N) ;
    }

    template <class dist_type>
    class BranchingNode;

    template <class coord_type, class dist_type>
    class Instance;

    template <class coord_type, class dist_type>
    void compute_minimal_1_tree(std::vector<EdgeId> & tree,
                                std::vector<NodeId> & tree_deg,
                                const std::vector<dist_type> lambda,
                                const Instance<double, dist_type> & tsp,
                                const BranchingNode<dist_type> & bn){
        Union_Find uf(tsp.size());
        NodeId i = 0,j = 0 ;
        std::vector<EdgeId> sorted(tsp.num_edges());
        for (size_type index = 0 ; index < tsp.num_edges(); index++)
            sorted[index] = index;

        for (auto const & el : bn.get_required()) {
            tree.push_back(el);
            to_NodeId(el,i,j,tsp.size());
            uf._union(i,j);
            tree_deg[i]++, tree_deg[j]++;
        }

        std::vector<dist_type> mod_weights;
        mod_weights.reserve(tsp.num_edges());
        for (size_t lauf = 0; lauf < tsp.weights().size(); lauf++) {
            NodeId index1= 0, index2 = 0;
            to_NodeId(lauf, index1 , index2 , tsp.size());
            mod_weights.push_back(tsp.weight(lauf) + lambda[index1] + lambda[index2]);
        }
        std::stable_sort(sorted.begin(), sorted.end(),
                         [&](EdgeId ind1, EdgeId ind2) {
                           return (mod_weights[ind1] <
                             mod_weights[ind2]);} );
        std::vector<EdgeId> candidates;
        candidates.reserve(tsp.size());
        for (auto it = sorted.begin(); (it != sorted.end()) && (tree.size() != tsp.size()-1); it++) {
            to_NodeId(*it, i, j, tsp.size());
            if ((i == 0) && (0 != j))
                candidates.push_back(*it);
            if ((i != j) && (i != 0) && (0 != j)) {
                if (uf._find(i) != uf._find(j)) {
                    tree.push_back(*it);
                    uf._union(i, j);
                }
            }
        }
        tree.push_back(candidates[0]);
        to_NodeId(candidates[0], i , j , tsp.size());
        tree_deg[i]++, tree_deg[j]++;

        tree.push_back(candidates[1]);
        to_NodeId(candidates[1], i , j , tsp.size());
        tree_deg[i]++, tree_deg[j]++;
        return;
    }

    template <class dist_type>
    dist_type Held_Karp(const TSP::Instance<double,dist_type> & tsp,
                        std::vector<dist_type> & lambda,
                        std::vector<EdgeId> & tree,
                        const BranchingNode<dist_type> & bn) {
        size_type n = tsp.size();
        std::vector<dist_type> sol_vector;
        for (size_t i = 0; i < std::ceil(n * n / 50) + n + 15; i++) {
            std::vector<NodeId> tree_deg(n);
            compute_minimal_1_tree<double, dist_type>(tree, tree_deg ,lambda, tsp, bn);
            for (size_t j = 0; j < lambda.size(); j++) {
                lambda[j] += 1*(tree_deg[j] - 2 );  // TODO: replace 1 by t_i
            }
            dist_type sum = 0, sum2 = 0 ;
            for (size_t k1 = 0; k1<tree.size(); k1++)
                sum += tsp.weight(tree[k1]);
            for (size_t k2 = 0; k2 < tree_deg.size(); k2++)
                sum2 += (tree_deg[k2] - 2)*lambda[k2];
            sol_vector.push_back(sum + sum2);
        }
        return *std::max_element(sol_vector.begin(), sol_vector.end());
    }



    template <class coord_type, class dist_type>
    class Instance {
    public:
        Instance(const std::string &filename) {
            std::ifstream file(filename);

            if (!file.is_open())
                throw std::runtime_error("File " + filename + " could not be opened");

            std::string line = "", option = "";
            bool scan = false;
            do {
                getline(file, line);
                stripColons(line);
                std::stringstream strstr;
                strstr << line;
                strstr >> option;

                if (option == "DIMENSION") {
                    strstr >> this->dimension;
                }

                if (option == "NODE_COORD_SECTION") {
                    scan = true;
                }
            } while (!scan && file.good());

            if (!scan)
                throw std::runtime_error("File not in right format");

            std::vector<coord_type> x, y;
            x.reserve(dimension), y.reserve(dimension);
            //_nodes.reserve(dimension);
            //_weights.reserve(dimension*(dimension-1));
            //size_type id = std::numeric_limits<size_type>::max();
            coord_type coord_x = std::numeric_limits<coord_type>::max() , coord_y = std::numeric_limits<coord_type>::max() ;
            while (file.good()) {
                getline(file, line);
                std::stringstream strstr;
                strstr << line;
                strstr >> option;
                // TODO: Capture if EOF is missing
                if (option != "EOF") {
                    try {
                        strstr >> coord_x >> coord_y;
                        std::cout << option << ' ' << coord_x << ' ' << coord_y << std::endl;
                        _nodes.push_back(std::stoi(option)-1) , x.push_back(coord_x) , y.push_back(coord_y);
                    }
                    catch (int e) {
                        std::cerr << "An exception occurred while reading the file. Exception Nr. " << e << '\n';
                    }
                }
                else break;
            }
            file.close();
            for (size_t i = 0; i < dimension; i++)
                for (size_t j = 0; j < dimension; j++)
                    this->_weights.push_back(
                        distance(x[i], y[i], x[j], y[j])
                        );

        }

        dist_type distance(coord_type x1,coord_type y1,coord_type x2,coord_type y2) {
            return std::lround(std::sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)));
        }

        void compute_optimal_tour()  {
            dist_type upperBound = std::numeric_limits<dist_type>::max(); // we want to do a naive tsp tour!
            auto cmp = [](BranchingNode<dist_type>, BranchingNode<dist_type>) { return true; };
            std::priority_queue<BranchingNode<dist_type>,
                    std::vector<BranchingNode<dist_type> >, decltype(cmp)> Q(cmp);
            Q.push(BranchingNode<dist_type>(std::vector<EdgeId>(), std::vector<EdgeId>(),*this,std::vector<dist_type>(dimension)));

            while (!Q.empty()) {
                BranchingNode<dist_type> current_BN(Q.top());
                Q.pop();
                if (current_BN.HK > upperBound)
                    continue;
                else {

                }
            }
        }

        void print_optimal_length() {
            int length = 0;
            for (int count = 0; count < this->size() - 1; count++) {
                NodeId i = this->_tour.at(count);
                NodeId j = this->_tour.at(count + 1);
                EdgeId edge = i * this->size() + j;
                length += this->_weights.at(edge);
            }
            NodeId i = this->_tour.at(this->size() - 1);
            NodeId j = this->_tour.at(0);
            EdgeId edge = i * this->size() + j;
            length += this->_weights.at(edge);

            std::cout << "The found tour is of length " << length << std::endl;
        }

        void print_optimal_tour(const std::string &filename) {
            std::ofstream file_to_print;
            file_to_print.open(filename, std::ios::out);


            file_to_print << "TYPE : TOUR" << std::endl;
            file_to_print << "DIMENSION : " << this->size() << std::endl;
            file_to_print << "TOUR_SECTION" << std::endl;
            for( int i = 0; i < this->size(); i++){
                file_to_print << this->_tour.at(i) << std::endl;
            }
            file_to_print << "-1" << std::endl;
            file_to_print << "EOF" << std::endl;
        }

        size_type size() const {
            return  dimension;
        }

        size_type num_edges() const {
            return _weights.size();
        }

        dist_type weight(EdgeId id) const {
            return _weights[id];
        }

        const std::vector<dist_type> &  weights() const {
            return _weights;
        }

    private:
        std::vector<NodeId> _nodes;
        std::vector<dist_type> _weights;
        size_type dimension;
        std::vector<NodeId> _tour;
    };

    template <class dist_type>
    class BranchingNode {
    public:
        BranchingNode(const std::vector<EdgeId > & req,
                      const std::vector<EdgeId> & forb,
                      const Instance<double, dist_type> & tsp,
                      const std::vector<dist_type> & lambda
        ) : required(req) , forbidden(forb), lambda(lambda) {

            this->HK = Held_Karp(tsp, this->lambda, this->tree, *this);
        }

        bool check_required(size_type id);
        bool check_forbidden(size_type id);

      const std::vector<EdgeId> & get_required() const {
          return required;
      }
      const std::vector<EdgeId> & get_forbidden() const {
          return forbidden;
      }


    private:
        friend Instance<double,dist_type>;
        std::vector<EdgeId> required;
        std::vector<EdgeId> forbidden;
        std::vector<dist_type> lambda;
        std::vector<EdgeId> tree;
        dist_type HK;
    };

}


#endif // BRANCHANDBOUNDTSP_TSP_HPP