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
#include <cassert>
#include "util.hpp"
#include "tree.hpp"

#define EPS 1. - 10e-5


// TODO: Split header and cpp methods
namespace TSP {

using size_type = std::size_t;
using NodeId = size_type;
using EdgeId = size_type;



template<class coord_type, class dist_type>
class BranchingNode;

template<class coord_type, class dist_type>
class Instance;

template<class coord_type, class dist_type>
void compute_minimal_1_tree(OneTree &tree,
                            const std::vector<double> &lambda,
                            const Instance<coord_type, dist_type> &tsp,
                            const BranchingNode<coord_type, dist_type> &BNode) {
    // Initializing union find structure, i and j nodeIDs and a sorted vector
    // which will contain the edges in an sorted order
    Union_Find uf(tsp.size());
    NodeId i = 0, j = 0;
    std::vector<EdgeId> sorted(tsp.num_edges());
    for (size_type index = 0; index < tsp.num_edges(); index++)
        sorted.at(index) = index;

    //compute modified weights c_\lambda and set the weight of required edges
    // to -inf and for forbidden edges to +inf
    std::vector<dist_type> mod_weights(tsp.num_edges(), 0);
    for (const auto &el : BNode.get_forbidden())
        mod_weights.at(el) = std::numeric_limits<dist_type>::max();
    for (const auto &el : BNode.get_required())
        mod_weights.at(el) = -std::numeric_limits<dist_type>::max();


    for (size_t edge = 0; edge < tsp.weights().size(); edge++) {
        if (mod_weights.at(edge) == 0) {//double comparison with == is bad, but let's make it a TODO
            NodeId v = 0, w = 0;
            to_NodeId(edge, v, w, tsp.size());
            mod_weights.at(edge) = tsp.weight(edge) + lambda[v] + lambda[w];
        }
    }
    //Sort the array to compute an 1-tree via iterating over the edges
    std::stable_sort(sorted.begin(), sorted.end(),
                     [&](EdgeId ind1, EdgeId ind2) {
                       return (mod_weights.at(ind1) <
                           mod_weights.at(ind2));
                     });

    std::vector<NodeId> candidates;
    candidates.reserve(tsp.size());
    for (auto it = sorted.begin(); it != sorted.end(); it++) {
        to_NodeId(*it, i, j, tsp.size());
        if ((i == 0) && (0 != j) && (candidates.size() < 2))
            candidates.push_back(j);
        if ((i != j) && (i != 0) && (0 != j) && (tree.get_num_edges() < tsp.size()-1 )) {
            if (uf._find(i) != uf._find(j)) {
                EdgeId edge = to_EdgeId(i,j,tsp.size());
                assert(!BNode.is_forbidden(edge));
                tree.add_edge(i, j);
                uf._union(i, j);
            }
        }
    }
    // This should be okay since we forbid all edges where deg >= 3 could happen
    assert(!BNode.is_forbidden(to_EdgeId(0,candidates.at(0),tsp.size())));
    assert(!BNode.is_forbidden(to_EdgeId(0,candidates.at(1),tsp.size())));
    tree.add_edge(0, candidates.at(0));
    tree.add_edge(0, candidates.at(1));
    return;
}

template<class coord_type, class dist_type>
dist_type Held_Karp(const TSP::Instance<coord_type, dist_type> &tsp,
                    std::vector<double> & lambda,
                    OneTree &tree,
                    const BranchingNode<coord_type, dist_type> &bn,
                    bool root = false) {
    size_type n = tsp.size();
    std::vector<dist_type> sol_vector;
    std::vector<double> lambda_max(lambda.size(),0), lambda_tmp(lambda.size(),0);
    OneTree tree_max(tree), tree_tmp(tree);
    double t_0 = 0, del_0 = 0, deldel = 0;
    size_t max_el = 0;
    size_t N = std::ceil(n / 4) + 5;
    if (root) {
        N =  std::ceil(n*n / 50) + n +  15;
    }

    t_0 = 1./(2. * tsp.size()) * std::accumulate(lambda.begin(), lambda.end(), 0.);

    deldel = t_0 / (N*N - N);
    del_0 = 3 * t_0 / (2 * N);

    for (size_t i = 0; i < N; i++) {
        tree.clear();
        compute_minimal_1_tree<coord_type, dist_type>(tree, lambda_tmp, tsp, bn);

        dist_type sum = 0, sum2 = 0;
        for (const auto &el : tree.get_edges())
            sum += tsp.weight(el);
        for (size_t node = 0; node < n; node++)
            sum2 += (tree.get_node(node).degree() - 2.) * lambda_tmp[node];
        sol_vector.push_back(sum + sum2);

        if (i == 0) {
            tree_max = tree;
            lambda_max = lambda_tmp;
            if(root){
                t_0 = sum/(2.*n);
            }

            for (size_t j = 0; j < lambda_tmp.size(); j++) {
                lambda_tmp[j] += t_0 * (tree.get_node(j).degree() - 2.);
            }
            t_0 = t_0 - del_0;
            del_0 = del_0 - deldel;
            continue;
            tree_tmp  = tree;
        }

        if (i > 0)
            if (sol_vector[max_el] < sol_vector[i]) {
                lambda_max = lambda_tmp;
                max_el = i;
                tree_max = tree;
            }
        for (size_t j = 0; j < lambda_tmp.size(); j++) {
            lambda_tmp[j] += t_0 * (0.6 * (tree.get_node(j).degree() - 2.) + 0.4*(tree_tmp.get_node(j).degree() - 2.));
        }
        t_0 = t_0 - del_0;
        del_0 = del_0 - deldel;
    }
    if(root){
        lambda = lambda_max;
    }
    tree = tree_max;
    return std::ceil(*std::max_element(sol_vector.begin(), sol_vector.end()));
}

template<class coord_type, class dist_type>
class Instance {
 public:
  Instance(const std::string &filename);

  dist_type distance(coord_type x1, coord_type y1, coord_type x2, coord_type y2) {
      return std::lround(std::sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)));
  }

  void compute_optimal_tour();

  void print_optimal_length();

  void print_optimal_tour(const std::string &filename);

  size_type size() const {
      return dimension;
  }

  size_type num_edges() const {
      return _weights.size();
  }

  dist_type weight(EdgeId id) const {
      return _weights.at(id);
  }

  const std::vector<dist_type> &weights() const {
      return _weights;
  }

 private:
  std::vector<NodeId> _nodes;
  std::vector<dist_type> _weights;
  size_type dimension;
  std::vector<NodeId> _tour;
};

template<class coord_type, class dist_type>
class BranchingNode {
 public:
  BranchingNode(const Instance<coord_type, dist_type> &tsp
  ) : required(), forbidden(), lambda(size, 0), tree(0, size), size(tsp.size()) {
      this->HK = Held_Karp(tsp, this->lambda, this->tree, *this, true);
  }

  BranchingNode(const BranchingNode<coord_type, dist_type> &BNode,
                const Instance<coord_type, dist_type> &tsp,
                EdgeId e1
  ) : required(BNode.get_required()),
      forbidden(BNode.get_forbidden()),
      lambda(BNode.get_lambda()),
      tree(0, tsp.size()),
      size(tsp.size()) {

      this->push_forbidden(e1, BNode);
      this->HK = Held_Karp(tsp, this->lambda, this->tree, *this);
  }

  BranchingNode(const BranchingNode<coord_type, dist_type> &BNode,
                const Instance<coord_type, dist_type> &tsp,
                EdgeId e1,
                EdgeId e2
  ) : size(tsp.size()),
      required(BNode.get_required()),
      forbidden(BNode.get_forbidden()),
      lambda(BNode.get_lambda()),
      tree(0, tsp.size())
       {
      push_required(e1, BNode);
      push_forbidden(e2, BNode);
      this->HK = Held_Karp(tsp, this->lambda, this->tree, *this);
  }

  BranchingNode(const BranchingNode<coord_type, dist_type> &BNode,
                const Instance<coord_type, dist_type> &tsp,
                EdgeId e1,
                EdgeId e2,
                bool both_req
  ) : size(tsp.size()),
      required(BNode.get_required()),
      forbidden(BNode.get_forbidden()),
      lambda(BNode.get_lambda()),
      tree(0, tsp.size()) {

      if (both_req) {
          push_required(e1, BNode);
          push_required(e2, BNode);
      }
      this->HK = Held_Karp(tsp, this->lambda, this->tree, *this);
  }

  const EdgeId reverse_edge(EdgeId e, size_type n) const {
      NodeId i = 0, j = 0;
      to_NodeId(e, i , j , n);
      return j* n + i;
  }

  bool is_required(EdgeId id) const {
      assert((std::find(required.begin(), required.end(), id) == required.end() )
          == ( std::find(required.begin(), required.end(), reverse_edge(id,size)) == required.end()) );
      return (std::find(required.begin(), required.end(), id) != required.end()
      && std::find(required.begin(), required.end(), reverse_edge(id,size)) != required.end())
          ;
  }

  bool is_forbidden(EdgeId id) const {
      return (std::find(forbidden.begin(), forbidden.end(), id) != forbidden.end()
          && std::find(forbidden.begin(), forbidden.end(), reverse_edge(id,size)) != forbidden.end());
  }



  void forbid(NodeId idx, EdgeId e1, EdgeId e2) {
//      EdgeId edge_start = to_EdgeId(idx, 0, size);
      for (NodeId k = 0; k < size; k++) {
          if (idx != k) {
              EdgeId edge = to_EdgeId(idx, k, size);
              if (edge != e1 && edge != e2) {
                  push_forbidden(edge);
              }

          }
      }
  }

  void admit(NodeId idx, EdgeId e1, EdgeId e2) {
//      EdgeId edge_start = to_EdgeId(idx, 0, size);
      for (NodeId k = 0; k < size; k++) {
          if (idx != k) {
              EdgeId edge = to_EdgeId(idx, k, size);
              if (edge != e1 && edge != e2)
                  push_required(edge);
          }
      }
  }


  void alt_forbid() {

  }

  void push_required(EdgeId e, const BranchingNode<coord_type, dist_type> &BNode) {
      if (is_required(e))
          return;

//      assert(e != 1122);
//      if (!is_forbidden(e)) {
//          int count = 0;
//          NodeId i = 0, j = 0;
//          to_NodeId(e,i,j,size);
//          for (NodeId k = 0; i < size; i++){
//              if(is_required(to_EdgeId(i, k, size))){
//                  count++;
//              }
//          }
//          if(count == 2){
//
//          }
//
//
//      }

      if (!is_forbidden(e)) {

          int count = 0;
          NodeId i = 0, j = 0;
          to_NodeId(e,i,j,size);
          for (NodeId k = 0; i < size; i++){
              if(is_required(to_EdgeId(i, k, size))){
                  count++;
              }
          }
          if(count == 2){
            push_forbidden(e);
              return;
          }
          count = 0;
          for (NodeId k = 0; i < size; i++){
              if(is_required(to_EdgeId(j, k, size))){
                  count++;
              }
          }
          if(count == 2){
              push_forbidden(e);
              return;
          }



          to_NodeId(e, i, j, size);
          for (const auto & el : required) {
              NodeId idx1 = 0, idx2 = 0;
              to_NodeId(el,idx1,idx2,size);
              if (i == idx1) {
                  forbid(i, e, el);
              }
              if (i == idx2) {
                  forbid(i, e, el);
              }
              if (j == idx1) {
                  forbid(j, e, el);
              }
              if (j == idx2) {
                  forbid(j, e, el);
              }
          }
          this->push_required(e);

      }
  }

  void push_required(EdgeId e) {
      if (is_required(e))
          return;
      if (!is_forbidden(e)) {
          this->required.push_back(e);
          this->required.push_back(reverse_edge(e,size));
      }
  }

    void push_forbidden(EdgeId e, const BranchingNode<coord_type, dist_type> &BNode) {
        if (is_forbidden(e))
            return;
//        if (!is_required(e)) {
//            NodeId i = 0, j = 0;
//            to_NodeId(e, i, j, size);
//            for (const auto & el : forbidden) {
//                NodeId idx1 = 0, idx2 = 0;
//                to_NodeId(el,idx1,idx2,size);
//                if (i == idx1) {
//                    admit(i, e, el);
//                }
//                if (i == idx2) {
//                    admit(i, e, el);
//                }
//                if (j == idx1) {
//                    admit(j, e, el);
//                }
//                if (j == idx2) {
//                    admit(j, e, el);
//                }
//            }
        if (!is_required(e)) {
            this->push_forbidden(e);
        }
//        }
    }
    void push_forbidden(EdgeId e) {
        if (is_forbidden(e))
            return;
        if (!is_required(e)) {
            forbidden.push_back(e);
            forbidden.push_back(reverse_edge(e, size));
        }
    }

  const std::vector<EdgeId> &get_required() const {
      return required;
  }
  const std::vector<EdgeId> &get_forbidden() const {
      return forbidden;
  }

  const std::vector<dist_type> &get_lambda() const {
      return lambda;
  }
  std::vector<dist_type> &get_lambda() {
      return lambda;
  }

  const OneTree &get_tree() const {
      return tree;
  }
  OneTree &get_tree() {
      return tree;
  }

  void set_HK(dist_type c) {
      this->HK = c;
  }
  const dist_type get_HK() const {
      return this->HK;
  }

  bool tworegular() {
      for (const auto &el : tree.get_nodes())
          if (el.degree() != 2)
              return false;
      return true;
  }

 private:
//  friend class TSP::Instance<coord_type, dist_type>;
  size_type size;
  std::vector<EdgeId> required;
  std::vector<EdgeId> forbidden;
  std::vector<double> lambda;
  OneTree tree;

  dist_type HK;
};
}

#include "tsp_impl.hpp"

#endif // BRANCHANDBOUNDTSP_TSP_HPP