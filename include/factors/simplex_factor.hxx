#ifndef LP_MP_SIMPLEX_FACTOR_HXX
#define LP_MP_SIMPLEX_FACTOR_HXX

#include "LP_MP.h"
#include "memory_allocator.hxx"
#include "vector.hxx"
#ifdef WITH_SAT
#include "sat_interface.hxx"
#endif

// Investigate contingency tables (Knuth) for more general tabular structures.

namespace LP_MP {

// the polytope {x >= 0 : x_1 + ... + x_n = 1 }
// dual problem:
//      max_{z} z 
//      s.t.    z <= repamPot[i]

// do zrobienia: replace std::vector by dynamic vector that is not resizable and gets memory from factor allocator
template<typename T = REAL> // artifact: do zrobienia: remove
class SimplexFactor : public std::vector<REAL>
{
public:
   using VECTOR = std::vector<REAL>;
   SimplexFactor(const std::vector<REAL>& cost)
      : VECTOR(cost.begin(), cost.end()) // better use iterators and allocator at same time
   {}
   SimplexFactor(const INDEX n)
      : VECTOR(n,0.0)
   {}

   ~SimplexFactor()
   {
      std::cout << "do not use anymore\n";
      throw std::runtime_error("do not use anymore");
   }

   void MaximizePotentialAndComputePrimal(typename PrimalSolutionStorage::Element primal) const
   {
      INDEX min_element;
      REAL min_value = std::numeric_limits<REAL>::infinity();
      for(INDEX i=0; i<this->size(); ++i) {
         if(primal[i] == true) {
            return;
         } else if(primal[i] == false) {
            // do nothing
         } else {
            assert(primal[i] == unknownState);
            primal[i] = false;
            if(min_value >= (*this)[i]) {
               min_value = (*this)[i];
               min_element = i;
            }
         }
      }
      assert(min_element < this->size());
      primal[min_element] = true;
   };

   REAL LowerBound() const {
      const REAL lb = *std::min_element(this->begin(), this->end());
      assert(!std::isnan(lb) && std::isfinite(lb));
      return lb;
   }

   // if there is exactly one unknownIndex (rest false), make it true
   // if there is already one true index, make all other false
   void PropagatePrimal(PrimalSolutionStorage::Element primal)
   {
      INDEX noTrue = 0;
      INDEX noUnknown = 0;
      INDEX unknownIndex;
      for(INDEX i=0; i<this->size(); ++i) {
         if(primal[i] == true) { 
            ++noTrue;
         }
         if(primal[i] == unknownState) {
            ++noUnknown;
            unknownIndex = i;
         }
      }
      if(noTrue == 0 && noUnknown == 1) {
         primal[unknownIndex] = true;
      } else if(noTrue == 1 && noUnknown > 0) {
         for(INDEX i=0; i<this->size(); ++i) {
            if(primal[i] == unknownState) {
               primal[i] = false;
            }
         }
      }
   }

   REAL EvaluatePrimal() const
   {
      assert(false);
         return std::numeric_limits<REAL>::infinity();
   }

  //INDEX GetNumberOfAuxVariables() const { return 0; }

  /*
  void ReduceLp(LpInterfaceAdapter* lp) const {
    REAL lb = LowerBound();
    REAL epsi = lp->GetEpsilon();
    for(INDEX i=0;i<this->size();i++){
      if((*this)[i] >= lb + epsi){
        lp->SetVariableBound(lp->GetVariable(i),0.0,0.0,false);
      }
    }
  }
  
  void CreateConstraints(LpInterfaceAdapter* lp) const { 
    LinExpr lhs = lp->CreateLinExpr();
    for(INDEX i=0;i<lp->GetFactorSize();i++){
      lhs += lp->GetVariable(i);
    }
    LinExpr rhs = lp->CreateLinExpr();
    rhs += 1;
    lp->addLinearEquality(lhs,rhs);
  }
  */
  
};

   
class UnarySimplexFactor : public vector<REAL> {
public:
   UnarySimplexFactor(const std::vector<REAL>& cost) : vector<REAL>(cost.begin(), cost.end()) {}
   UnarySimplexFactor(const INDEX n) : vector<REAL>(n, 0.0) {}

   REAL LowerBound() const { 
      const REAL lb = *std::min_element(this->begin(), this->end()); 
      assert(std::isfinite(lb));
      return lb;
   }

   REAL EvaluatePrimal() const 
   { 
      if(primal_ >= size()) {
         return std::numeric_limits<REAL>::infinity();
      }
      return (*this)[primal_]; 
   }
   void MaximizePotentialAndComputePrimal() 
   {
      if(primal_ >= size()) {
         primal_ = std::min_element(this->begin(), this->end()) - this->begin();
         assert(primal_ < size());
      }
   }

   // load/store function for the primal value
   template<class ARCHIVE> void serialize_primal(ARCHIVE& ar) { ar(primal_); }
   template<class ARCHIVE> void serialize_dual(ARCHIVE& ar) { ar( *static_cast<vector<REAL>*>(this) ); }

   void init_primal() { primal_ = size(); }
   INDEX primal() const { return primal_; }
   INDEX& primal() { return primal_; }
   void primal(const INDEX p) { primal_ = p; }


#ifdef WITH_SAT
   template<typename SAT_SOLVER>
   void construct_sat_clauses(SAT_SOLVER& s) const
   {
      auto vars = create_sat_variables(s, size());
      add_simplex_constraint_sat(s, vars.begin(), vars.end());
   }

   template<typename VEC>
   void reduce_sat(VEC& assumptions, const REAL th, sat_var begin) const
   {
      const REAL lb = LowerBound();
      for(INDEX i=0; i<this->size(); ++i) {
         if((*this)[i] > lb + th) { 
            assumptions.push_back(-to_literal(begin+i));
         }
      } 
   }

   template<typename SAT_SOLVER>
   void convert_primal(SAT_SOLVER& s, sat_var first)
   {
      for(INDEX i=first; i<first+this->size(); ++i) {
         if(lglderef(s,to_literal(i)) == 1) {
            primal_ = i-first;
         }
      }
   }
#endif

private:
   INDEX primal_;
};

// do zrobienia: if pairwise was supplied to us (e.g. external factor, then reflect this in constructor and only allocate space for messages.
// When tightening, we can simply replace pairwise pointer to external factor with an explicit copy. Reallocate left_msg_ and right_msg_ to make memory contiguous? Not sure, depends whether we use block_allocator, which will not acually release the memory
// when factor is copied, then pairwise_ must only be copied if it is actually modified. This depends on whether we execute SMRP or MPLP style message passing. Templatize for this possibility
class PairwiseSimplexFactor : public matrix_expression<REAL, PairwiseSimplexFactor> {
public:
   PairwiseSimplexFactor(const INDEX dim1, const INDEX dim2) : dim1_(dim1), dim2_(dim2)
   {
      const INDEX size = dim1_*dim2_ + dim1_ + dim2_;
      pairwise_ = global_real_block_allocator.allocate(size);
      //pairwise_ = new REAL[size]; // possibly use block allocator!
      assert(pairwise_ != nullptr);
      std::fill(pairwise_, pairwise_ + size, 0.0);
      left_msg_ = pairwise_ + dim1_*dim2_;
      right_msg_ = left_msg_ + dim1_;
   }
   ~PairwiseSimplexFactor() {
      global_real_block_allocator.deallocate(pairwise_,1);
   }
   PairwiseSimplexFactor(const PairwiseSimplexFactor& o) : dim1_(o.dim1_), dim2_(o.dim2_) {
      const INDEX size = dim1_*dim2_ + dim1_ + dim2_;
      pairwise_ = global_real_block_allocator.allocate(size);
      //pairwise_ = new REAL[dim1_*dim2_ + dim1_ + dim2_]; // possibly use block allocator!
      assert(pairwise_ != nullptr);
      left_msg_ = pairwise_ + dim1_*dim2_;
      right_msg_ = left_msg_ + dim1_;
      for(INDEX i=0; i<dim1_*dim2_; ++i) { pairwise_[i] = o.pairwise_[i]; }
      for(INDEX i=0; i<dim1_; ++i) { left_msg_[i] = o.left_msg_[i]; }
      for(INDEX i=0; i<dim2_; ++i) { right_msg_[i] = o.right_msg_[i]; }
   }
   void operator=(const PairwiseSimplexFactor& o) {
      assert(dim1_ == o.dim1_ && dim2_ == o.dim2_);
      for(INDEX i=0; i<dim1_*dim2_; ++i) { pairwise_[i] = o.pairwise_[i]; }
      for(INDEX i=0; i<dim1_; ++i) { left_msg_[i] = o.left_msg_[i]; }
      for(INDEX i=0; i<dim2_; ++i) { right_msg_[i] = o.right_msg_[i]; }
   }

   REAL operator[](const INDEX x) const {
      const INDEX x1 = x/dim2_;
      const INDEX x2 = x%dim2_;
      assert(x1 < dim1_ && x2 < dim2_);
      return pairwise_[x] + left_msg_[x1] + right_msg_[x2];
   }
   // below is not nice: two different values, only differ by const!
   REAL operator()(const INDEX x1, const INDEX x2) const {
      assert(x1 < dim1_ && x2 < dim2_);
      return pairwise_[x1*dim2_ + x2] + left_msg_[x1] + right_msg_[x2];
   }
   REAL& cost(const INDEX x1, const INDEX x2) {
      assert(x1 < dim1_ && x2 < dim2_);
      return pairwise_[x1*dim2_ + x2];
   }
   REAL LowerBound() const {
      REAL lb = std::numeric_limits<REAL>::infinity();
      for(INDEX x1=0; x1<dim1_; ++x1) {
         for(INDEX x2=0; x2<dim2_; ++x2) {
            lb = std::min(lb, (*this)(x1,x2));
         }
      }
      assert(std::isfinite(lb));
      return lb;
   }

   
   INDEX dim1() const { return dim1_; }
   INDEX dim2() const { return dim2_; }
     
   REAL& pairwise(const INDEX x1, const INDEX x2) { return pairwise_[x1*dim2_ + x2]; }
   REAL& left(const INDEX x1) { return left_msg_[x1]; }
   REAL& right(const INDEX x2) { return right_msg_[x2]; }

   INDEX size() const { return dim1_*dim2_; }

   void init_primal() 
   {
      primal_[0] = dim1();
      primal_[1] = dim2();
   }
   REAL EvaluatePrimal() const { 
      if(primal_[0] >= dim1() || primal_[1] >= dim2()) {
         return std::numeric_limits<REAL>::infinity();
      }
      assert(primal_[0] < dim1());
      assert(primal_[1] < dim2());
      const REAL val = (*this)(primal_[0], primal_[1]); 
      //assert(val < std::numeric_limits<REAL>::infinity());
      return val;
   }
   void MaximizePotentialAndComputePrimal() 
   {
      REAL min_val = std::numeric_limits<REAL>::infinity();
      for(INDEX x1=0; x1<dim1(); ++x1) {
         for(INDEX x2=0; x2<dim2(); ++x2) {
            if(min_val >= (*this)(x1,x2)) {
               min_val = (*this)(x1,x2);
               primal_[0] = x1;
               primal_[1] = x2;
            }
         }
      }
   }

   template<class ARCHIVE> void serialize_primal(ARCHIVE& ar) { ar( primal_[0], primal_[1] ); }
   template<class ARCHIVE> void serialize_dual(ARCHIVE& ar) { ar( cereal::binary_data( pairwise_, sizeof(REAL)*(size()+dim1()+dim2()) ) ); }

   /*
   void serialize_primal(bit_vector primal_bits) const
   {
      INDEX i=0;
      for(INDEX x1=0; x1<dim1(); ++x1) {
         for(INDEX x2=0; x2<dim2(); ++x2) {
            primal_bits[i] = false;
            if(primal_[0] == x1 && primal_[1] == x2) {
               primal_bits[i] = true;
            }
            ++i;
         } 
      }
   }

   void deserialize_primal(bit_vector primal_bits) 
   {
      INDEX i=0;
      for(INDEX x1=0; x1<dim1(); ++x1) {
         for(INDEX x2=0; x2<dim2(); ++x2) {
            if(primal_bits[i] == true) {
               primal_[0] = x1;
               primal_[1] = x2;
            }
            ++i;
         } 
      }
   }
   */


#ifdef WITH_SAT
   template<typename SAT_SOLVER>
   void construct_sat_clauses(SAT_SOLVER& s) const
   {
      auto vars = create_sat_variables(s, dim1() + dim2() + dim1()*dim2());
      sat_var pairwise_var_begin = vars[dim1() + dim2()];

      std::vector<sat_var> tmp_vars;
      tmp_vars.reserve(std::max(dim1(), dim2()));

      for(INDEX x1=0; x1<dim1(); ++x1) {
         for(INDEX x2=0; x2<dim2(); ++x2) {
            tmp_vars.push_back(pairwise_var_begin + x1*dim2() + x2);
         }
         sat_var c = add_at_most_one_constraint_sat(s, tmp_vars.begin(), tmp_vars.end());
         make_sat_var_equal(s, to_literal(c), to_literal(vars[x1]));
         tmp_vars.clear();
      }
      for(INDEX x2=0; x2<dim2(); ++x2) {
         for(INDEX x1=0; x1<dim1(); ++x1) {
            tmp_vars.push_back(pairwise_var_begin + x1*dim2() + x2);
         }
         sat_var c = add_at_most_one_constraint_sat(s, tmp_vars.begin(), tmp_vars.end());
         make_sat_var_equal(s, to_literal(c), to_literal(vars[dim1() + x2]));
         tmp_vars.clear();
      }

      // is superfluous: summation constraints must be active due to unary simplex factors
      //add_simplex_constraint_sat(sat_var.begin(), sat_var.begin()+dim1());
      //add_simplex_constraint_sat(sat_var.begin()+dim1(), sat_var.begin()+dim1()+dim2());
   }

   template<typename VEC>
   void reduce_sat(VEC& assumptions, const REAL th, sat_var begin) const
   {
      begin += dim1() + dim2();
      const REAL lb = LowerBound();
      for(INDEX x1=0; x1<this->dim1(); ++x1) {
         for(INDEX x2=0; x2<this->dim2(); ++x2) {
            if((*this)(x1,x2) > lb + th) { 
               assumptions.push_back(-to_literal(begin + x1*dim2() + x2));
            }
         }
      } 
   }

   template<typename SAT_SOLVER>
   void convert_primal(SAT_SOLVER& s, sat_var first)
   {
      for(INDEX x1=0; x1<this->dim1(); ++x1) {
         if(lglderef(s,to_literal(first + x1)) == 1) {
            primal_[0] = x1;
         } 
      }
      for(INDEX x2=0; x2<this->dim2(); ++x2) {
         if(lglderef(s,to_literal(first + dim1() + x2)) == 1) {
            primal_[1] = x2;
         } 
      }
   }
#endif


   //INDEX primal_[0], primal_[1]; // not so nice: make getters and setters!
   std::array<INDEX,2> primal_;
private:
   // those three pointers should lie contiguously in memory.
   REAL* pairwise_;
   REAL* left_msg_;
   REAL* right_msg_;
   const INDEX dim1_, dim2_;

};

// factor assumes that triplet potentials is empty and holds only messages to pairwise factors, i.e. is latently factorizable
class SimpleTighteningTernarySimplexFactor : public tensor3_expression<REAL, SimpleTighteningTernarySimplexFactor> {
public:
   SimpleTighteningTernarySimplexFactor(const INDEX dim1, const INDEX dim2, const INDEX dim3) : dim1_(dim1), dim2_(dim2), dim3_(dim3) 
   {
      const INDEX size = dim1_*dim2_ + dim1_*dim3_ + dim2_*dim3_;
      
      msg12_ = global_real_block_allocator.allocate(size);
      assert(msg12_ != nullptr);
      msg13_ = msg12_ + dim1_*dim2_;
      msg23_ = msg13_ + dim1_*dim3_;
      std::fill(msg12_, msg12_ + size, 0.0);
   }
   ~SimpleTighteningTernarySimplexFactor() {
      global_real_block_allocator.deallocate(msg12_,1);
   }
   SimpleTighteningTernarySimplexFactor(const SimpleTighteningTernarySimplexFactor& o) : dim1_(o.dim1_), dim2_(o.dim2_), dim3_(o.dim3_) {
      const INDEX size = dim1_*dim2_ + dim1_*dim3_ + dim2_*dim3_;
      
      msg12_ = global_real_block_allocator.allocate(size);
      assert(msg12_ != nullptr);
      msg13_ = msg12_ + dim1_*dim2_;
      msg23_ = msg13_ + dim1_*dim3_;
      for(INDEX i=0; i<dim1_*dim2_; ++i) { msg12_[i] = o.msg12_[i]; }
      for(INDEX i=0; i<dim1_*dim3_; ++i) { msg13_[i] = o.msg13_[i]; }
      for(INDEX i=0; i<dim2_*dim3_; ++i) { msg23_[i] = o.msg23_[i]; }
   }
   void operator=(const SimpleTighteningTernarySimplexFactor& o) {
      assert(dim1_ == o.dim1_ && dim2_ == o.dim2_ && dim3_ == o.dim3_);
      for(INDEX i=0; i<dim1_*dim2_; ++i) { msg12_[i] = o.msg12_[i]; }
      for(INDEX i=0; i<dim1_*dim3_; ++i) { msg13_[i] = o.msg13_[i]; }
      for(INDEX i=0; i<dim2_*dim3_; ++i) { msg23_[i] = o.msg23_[i]; }
   }

   REAL LowerBound() const {
      REAL lb = std::numeric_limits<REAL>::infinity();
      for(INDEX x1=0; x1<dim1_; ++x1) {
         for(INDEX x2=0; x2<dim2_; ++x2) {
            for(INDEX x3=0; x3<dim3_; ++x3) {
               lb = std::min(lb, (*this)(x1,x2,x3));
            }
         }
      }
      assert(std::isfinite(lb));
      return lb;
   }

   REAL EvaluatePrimal() const
   {
      if(primal_[0] >= dim1_ || primal_[1] >= dim2_ || primal_[2] >= dim3_) {
         return std::numeric_limits<REAL>::infinity();
      }
      //assert((*this)(primal_[0], primal_[1], primal_[2]) < std::numeric_limits<REAL>::infinity());
      return (*this)(primal_[0], primal_[1], primal_[2]);
   }

   REAL operator()(const INDEX x1, const INDEX x2, const INDEX x3) const {
      return msg12(x1,x2) + msg13(x1,x3) + msg23(x2,x3);
   }

   /*
   REAL operator[](const INDEX x) const {
      const INDEX x1 = x / (dim2_*dim3_);
      const INDEX x2 = ( x % (dim2_*dim3_) ) / dim3_;
      const INDEX x3 = x % dim3_;
      return msg12(x1,x2) + msg13(x1,x3) + msg23(x2,x3);
   }
   */

   REAL min_marginal12(const INDEX x1, const INDEX x2) const {
      REAL marg = (*this)(x1,x2,0);
      for(INDEX x3=1; x3<dim3_; ++x3) {
         marg = std::min(marg, (*this)(x1,x2,x3));
      }
      return marg;
   }
   template<typename MSG>
   void min_marginal12(MSG& msg) const {
      assert(msg.dim1() == dim1_);
      assert(msg.dim2() == dim2_);
      //for(INDEX x1=0; x1<dim1_; ++x1) {
      //   for(INDEX x2=0; x2<dim2_; ++x2) {
      //      msg(x1,x2) = std::numeric_limits<REAL>::infinity();
      //   }
      //}
      for(INDEX x1=0; x1<dim1_; ++x1) {
         for(INDEX x2=0; x2<dim2_; ++x2) {
            msg(x1,x2) = min_marginal12(x1,x2);
         }
      }
   }

   template<typename MSG>
   void min_marginal13(MSG& msg) const {
      assert(msg.dim1() == dim1_);
      assert(msg.dim2() == dim3_);
      for(INDEX x1=0; x1<dim1_; ++x1) {
         for(INDEX x3=0; x3<dim3_; ++x3) {
            msg(x1,x3) = std::numeric_limits<REAL>::infinity();
         }
      }
      for(INDEX x1=0; x1<dim1_; ++x1) {
         for(INDEX x2=0; x2<dim2_; ++x2) {
            for(INDEX x3=0; x3<dim3_; ++x3) {
               msg(x1,x3) = std::min(msg(x1,x3), (*this)(x1,x2,x3));
            }
         }
      }
   }
   template<typename MSG>
   void min_marginal23(MSG& msg) const {
      assert(msg.dim1() == dim2_);
      assert(msg.dim2() == dim3_);
      for(INDEX x2=0; x2<dim2_; ++x2) {
         for(INDEX x3=0; x3<dim3_; ++x3) {
            msg(x2,x3) = std::numeric_limits<REAL>::infinity();
         }
      }
      for(INDEX x1=0; x1<dim1_; ++x1) {
         for(INDEX x2=0; x2<dim2_; ++x2) {
            for(INDEX x3=0; x3<dim3_; ++x3) {
               msg(x2,x3) = std::min(msg(x2,x3), (*this)(x1,x2,x3));
            }
         }
      }
   }

   INDEX size() const { return dim1()*dim2()*dim3(); }

   INDEX dim1() const { return dim1_; }
   INDEX dim2() const { return dim2_; }
   INDEX dim3() const { return dim3_; }

   REAL msg12(const INDEX x1, const INDEX x2) const { return msg12_[x1*dim2_ + x2]; }
   REAL msg13(const INDEX x1, const INDEX x3) const { return msg13_[x1*dim3_ + x3]; }
   REAL msg23(const INDEX x2, const INDEX x3) const { return msg23_[x2*dim3_ + x3]; }

   REAL& msg12(const INDEX x1, const INDEX x2) { return msg12_[x1*dim2_ + x2]; }
   REAL& msg13(const INDEX x1, const INDEX x3) { return msg13_[x1*dim3_ + x3]; }
   REAL& msg23(const INDEX x2, const INDEX x3) { return msg23_[x2*dim3_ + x3]; }

   void init_primal() {
      primal_[0] = dim1_;
      primal_[1] = dim2_;
      primal_[2] = dim3_;
   }
   template<class ARCHIVE> void serialize_primal(ARCHIVE& ar) { ar(primal_); }
   template<class ARCHIVE> void serialize_dual(ARCHIVE& ar) 
   { 
      ar( cereal::binary_data( msg12_, sizeof(REAL)*(dim1()*dim2()) ) );
      ar( cereal::binary_data( msg13_, sizeof(REAL)*(dim1()*dim3()) ) );
      ar( cereal::binary_data( msg23_, sizeof(REAL)*(dim2()*dim3()) ) );
   }


#ifdef WITH_SAT
   template<typename SAT_SOLVER>
   void construct_sat_clauses(SAT_SOLVER& s) const
   {
      //auto vars = create_sat_variables(s, dim1()*dim2() + dim1()*dim3() + dim2()*dim3() + dim1()*dim2()*dim3());
      auto vars_12 = create_sat_variables(s, dim1()*dim2());
      auto vars_13 = create_sat_variables(s, dim1()*dim3());
      auto vars_23 = create_sat_variables(s, dim2()*dim3());
      auto vars_123 = create_sat_variables(s, dim1()*dim2()*dim3());
      //auto triplet_var_begin = vars[dim1()*dim2() + dim1()*dim3() + dim2()*dim3()];

      std::vector<sat_var> tmp_vars;
      tmp_vars.reserve(std::max({dim1()*dim2(), dim1()*dim3(), dim2()*dim3()}));

      for(INDEX x1=0; x1<dim1(); ++x1) {
         for(INDEX x2=0; x2<dim2(); ++x2) {
            for(INDEX x3=0; x3<dim3(); ++x3) {
               tmp_vars.push_back(vars_123[x1*dim2()*dim3() + x2*dim3() + x3]);
            }
            auto c = add_at_most_one_constraint_sat(s, tmp_vars.begin(), tmp_vars.end());
            make_sat_var_equal(s, to_literal(c), to_literal(vars_12[x1*dim2() + x2]));
            tmp_vars.clear();
         }
      }

      for(INDEX x1=0; x1<dim1(); ++x1) {
         for(INDEX x3=0; x3<dim3(); ++x3) {
            for(INDEX x2=0; x2<dim2(); ++x2) {
               tmp_vars.push_back(vars_123[x1*dim2()*dim3() + x2*dim3() + x3]);
            }
            auto c = add_at_most_one_constraint_sat(s, tmp_vars.begin(), tmp_vars.end());
            make_sat_var_equal(s, to_literal(c), to_literal(vars_13[x1*dim3() + x3]));
            tmp_vars.clear();
         }
      }

      for(INDEX x2=0; x2<dim2(); ++x2) {
         for(INDEX x3=0; x3<dim3(); ++x3) {
            for(INDEX x1=0; x1<dim1(); ++x1) {
               tmp_vars.push_back(vars_123[x1*dim2()*dim3() + x2*dim3() + x3]);
            }
            auto c = add_at_most_one_constraint_sat(s, tmp_vars.begin(), tmp_vars.end());
            make_sat_var_equal(s, to_literal(c), to_literal(vars_23[x2*dim3() + x3]));
            tmp_vars.clear();
         }
      }

      // summation constraints over triplet variables are superfluous: they are enforced via messages
   }

   template<typename VEC>
   void reduce_sat(VEC& assumptions, const REAL th, sat_var begin) const
   {
      begin += dim1()*dim2() + dim1()*dim3() + dim2()*dim3();
      const REAL lb = LowerBound();
      for(INDEX x1=0; x1<this->dim1(); ++x1) {
         for(INDEX x2=0; x2<this->dim2(); ++x2) {
            for(INDEX x3=0; x3<this->dim3(); ++x3) {
               if((*this)(x1,x2,x3) > lb + th) { 
                  assumptions.push_back(-to_literal(begin + x1*dim2()*dim3() + x2*dim3() + x3));
               }
            }
         }
      } 
   }

   template<typename SAT_SOLVER>
   void convert_primal(SAT_SOLVER& s, sat_var first)
   {
      for(INDEX x1=0; x1<this->dim1(); ++x1) {
         for(INDEX x2=0; x2<this->dim2(); ++x2) {
            if(lglderef(s,to_literal(first + x1*dim2() + x2)) == 1) {
               primal_[0] = x1;
               primal_[1] = x2;
            }
         } 
      }

      const INDEX pairwise_var_begin_13 = first + dim1()*dim2(); 
      for(INDEX x1=0; x1<dim1(); ++x1) {
         for(INDEX x3=0; x3<dim3(); ++x3) {
            if(lglderef(s,to_literal(pairwise_var_begin_13 + x1*dim3() + x3)) == 1) {
               assert(primal_[0] == x1);
               primal_[2] = x3;
            }
         }
      }
   }
#endif


   std::array<INDEX,3> primal_;
protected:
   const INDEX dim1_, dim2_, dim3_; // do zrobienia: possibly use 32 bit, for primal as well
   REAL *msg12_, *msg13_, *msg23_;
};


/*
class TighteningTernarySimplexFactor {
public:
   TighteningTernarySimplexFactor(const INDEX dim1, const INDEX dim2, const INDEX dim3) : dim1_(dim1), dim2_(dim2), dim3_(dim3) 
   {
      const INDEX size = dim1_*dim2_ + dim1_*dim3_ + dim2_*dim3_;
      
      msg12_ = global_real_block_allocator.allocate(size + std::ceil((1.0*sizeof(INDEX))/(1.0*sizeof(REAL))*size));
      assert(msg12_ != nullptr);
      msg13_ = msg12_ + dim1_*dim2_;
      msg23_ = msg13_ + dim1_*dim3_;
      std::fill(msg12_, msg12_ + size, 0.0);

      msg12_sorted_ = reinterpret_cast<INDEX*>(msg12_ + size);
      msg13_sorted_ = msg12_sorted_ + dim1_*dim2_;
      msg23_sorted_ = msg13_sorted_ + dim1_*dim3_;
      std::iota(msg12_sorted_, msg12_sorted_ + dim1_*dim2_, 0);
      std::iota(msg13_sorted_, msg13_sorted_ + dim1_*dim3_, 0);
      std::iota(msg23_sorted_, msg23_sorted_ + dim1_*dim2_, 0);
   }

   // slightly extended algorithms as used by Caetano et al for faster message passing
   void sort_indices() const { // do zrobienia: use insertion sort as it is faster on nearly sorted data
      std::sort(msg12_sorted_, msg12_sorted_ + dim1_*dim2_, [&](const INDEX a, const INDEX b) { return msg12_[msg12_sorted_[a]] < msg12_[msg12_sorted_[b]]; });
      std::sort(msg13_sorted_, msg13_sorted_ + dim1_*dim3_, [&](const INDEX a, const INDEX b) { return msg13_[msg13_sorted_[a]] < msg13_[msg13_sorted_[b]]; });
      std::sort(msg23_sorted_, msg23_sorted_ + dim1_*dim2_, [&](const INDEX a, const INDEX b) { return msg23_[msg23_sorted_[a]] < msg23_[msg23_sorted_[b]]; });
   }

   REAL FindBest3(const INDEX x1, const INDEX x2) const {
      REAL min = std::numeric_limits<REAL>::infinity();
      INDEX end13 = msg13_sorted_inverse(x1,msg23_sorted(x2,0));
      INDEX end23 = msgmsg23_sorted_inverse(msg13_sorted(x1,0));
      for(INDEX l3=0; l3<dim3_; ++l3) {
         if(l3 >= std::min(end13, end23)) break;
         {
            const INDEX x3 = msg13_sorted(x1,l3);
            min_val = std::min(min_val, msg_13(x1,x3) + msg_23(x2,x3));
            end12 = std::min(end12,msg23_sorted_inverse(x2, x3) );
         }
         {
            const INDEX x3 = msg23_sorted(x2,l3);
            min_val = std::min(min_val, msg_13(x1,x3) + msg_23(x2,x3));
            end23 = std::min(end23,msg13_sorted_inverse(x2, x3) );
         }
      }
      return min_val;
   }

   REAL LowerBound() const {
      REAL val = std::numeric_limits<REAL>::infinity();
      sort_indices();
      for(INDEX l1=0; l1<dim1_*dim2_; ++l1) { // add early stopping by checking whether val < msg12_sorted(l1)
         const INDEX x1 = msg12_sorted_1(l1);
         const INDEX x2 = msg12_sorted_2(l1);
         // find best index x3 such that
         val = std::min(val, msg12(x1,x2), FindBest3(x1,x2));
      }

      return val;
   }

   REAL& msg12(const INDEX x1, const INDEX x2) { return msg12_[x1*dim2_ + x2]; }
   REAL& msg13(const INDEX x1, const INDEX x3) { return msg13_[x1*dim3_ + x3]; }
   REAL& msg23(const INDEX x2, const INDEX x3) { return msg12_[x2*dim3_ + x3]; }
   INDEX msg12_sorted_1(const INDEX x) const { return msg12_sorted_[x]/dim1_; }
   INDEX msg12_sorted_2(const INDEX x) const { return msg12_sorted_[x]%dim1_; }
private:
   const INDEX dim1_, dim2_, dim3_;
   REAL *msg12_, *msg13_, *msg23_;
   mutable INDEX *msg12_sorted_, *msg13_sorted_, *msg23_sorted_;

public:

};
*/

} // end namespace LP_MP

#endif // LP_MP_SIMPLEX_FACTOR_HXX

