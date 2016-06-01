#include "multicut.h"
#include "visitors/standard_visitor.hxx"
using namespace LP_MP;
using FMC = FMC_LIFTED_MULTICUT_ODD_CYCLE;
//using FMC = FMC_MULTICUT<MessageSending::SRMP>;
LP_MP_CONSTRUCT_SOLVER_WITH_INPUT_AND_VISITOR(FMC, MulticutTextInput::ParseLiftedProblem<FMC>, StandardTighteningVisitor<ProblemDecomposition<FMC>>);

