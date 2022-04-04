#ifndef LSCP_STATIC_DECREMENTAL_INTERFACE_HPP
#define LSCP_STATIC_DECREMENTAL_INTERFACE_HPP

#include <execution>

#include <boost/program_options.hpp>

#include "landscape_opt/concepts/instance.hpp"
#include "landscape_opt/solvers/static_decremental.hpp"

#include "instance.hpp"
#include "solver_interfaces/abstract_solver.hpp"

namespace fhamonic {

class StaticDecrementalInterface : public AbstractSolver {
private:
    landscape_opt::solvers::StaticDecremental solver;

public:
    StaticDecrementalInterface() = default;

    void parse(const std::vector<std::string> & args) {
        boost::program_options::options_description desc("Allowed options");
        desc.add_options()("help,h", "Display this help message")(
            "verbose,v", "Timeout in seconds")("parallel,p",
                                               "Use multithreaded version");

        boost::program_options::variables_map vm;
        boost::program_options::store(
            boost::program_options::command_line_parser(args)
                .options(desc)
                .run(),
            vm);

        if(vm.count("help")) {
            std::cout << desc << "\n";
            return;
        }

        solver.verbose = vm.count("verbose") > 0;
        solver.parallel = vm.count("parallel") > 0;
    }

    typename Instance::Solution solve(const Instance & instance,
                                      const double B) const {
        return solver.solve(instance, B);
    };

    std::string name() const { return "static_decremental"; }
    std::string description() const { return ""; }
    std::string params_lists() const { return ""; }
    std::string string() const { return "static_decremental"; }
};

}  // namespace fhamonic

#endif  // LSCP_STATIC_DECREMENTAL_INTERFACE_HPP