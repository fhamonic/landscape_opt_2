#ifndef LSCP_GREEDY_INCREMENTAL_INTERFACE_HPP
#define LSCP_GREEDY_INCREMENTAL_INTERFACE_HPP

#include <sstream>

#include <boost/program_options.hpp>

#include "landscape_opt/solvers/greedy_incremental.hpp"

#include "instance.hpp"
#include "solver_interfaces/abstract_solver.hpp"

namespace fhamonic {

class GreedyIncrementalInterface : public AbstractSolver {
private:
    landscape_opt::solvers::GreedyIncremental solver;
    boost::program_options::options_description desc;

public:
    GreedyIncrementalInterface() : desc(name() + " options") {
        desc.add_options()("verbose,v", "Log the algorithm steps")(
            "parallel,p", "Use multithreaded version");
    }

    void parse(const std::vector<std::string> & args) {
        boost::program_options::variables_map vm;
        boost::program_options::store(
            boost::program_options::command_line_parser(args)
                .options(desc)
                .run(),
            vm);
        po::notify(vm);

        solver.verbose = vm.count("verbose") > 0;
        solver.parallel = vm.count("parallel") > 0;
    }

    typename Instance::Solution solve(const Instance & instance,
                                      const double B) const {
        return solver.solve(instance, B);
    };

    std::string name() const { return "greedy_incremental"; }
    std::string description() const {
        return "From the base landscape, iteratively add the option with the "
               "best gain/cost ratio.";
    }
    std::string options_description() const {
        std::ostringstream s;
        s << desc;
        return s.str();
    }
    std::string string() const { return "greedy_incremental"; }
};

}  // namespace fhamonic

#endif  // LSCP_GREEDY_INCREMENTAL_INTERFACE_HPP