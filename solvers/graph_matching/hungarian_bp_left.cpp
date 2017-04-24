
#include "graph_matching.h"
#include "visitors/standard_visitor.hxx"
int main(int argc, char* argv[])

{
MpRoundingSolver<Solver<FMC_HUNGARIAN_BP<PairwiseConstruction::Left>,LP,StandardTighteningVisitor>> solver(argc,argv);
solver.ReadProblem(TorresaniEtAlInput::ParseProblemHungarian<Solver<FMC_HUNGARIAN_BP<PairwiseConstruction::Left>,LP,StandardTighteningVisitor>>);
return solver.Solve();

}
