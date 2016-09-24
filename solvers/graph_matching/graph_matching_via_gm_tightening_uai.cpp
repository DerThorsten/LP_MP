#include "graph_matching.h"
#include "visitors/standard_visitor.hxx"
using FMC_INST = FMC_GM_T<PairwiseConstruction::Left>;
LP_MP_CONSTRUCT_SOLVER_WITH_INPUT_AND_VISITOR(FMC_INST, UaiGraphMatchingInput::ParseProblemGM<FMC_INST>, StandardTighteningVisitor);