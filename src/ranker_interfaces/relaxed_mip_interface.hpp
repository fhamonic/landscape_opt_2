#ifndef LSCP_MIP_INTERFACE_HPP
#define LSCP_MIP_INTERFACE_HPP

#include <sstream>

#include <boost/program_options.hpp>

#include "landscape_opt/rankers/mip.hpp"

#include "instance.hpp"
#include "ranker_interfaces/abstract_ranker.hpp"

namespace fhamonic {

class RelaxedMIPInterface : public AbstractRanker {
private:
    landscape_opt::rankers::MIP ranker;
    boost::program_options::options_description desc;

public:
    MIPInterface() : desc(name() + " options") {
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

        ranker.verbose = vm.count("verbose") > 0;
        ranker.parallel = vm.count("parallel") > 0;
    }

    typename Instance::OptionPotentialMap rank_options(
        const Instance & instance) const {
        return ranker.rank_options(instance);
    };

    std::string name() const { return "mip"; }
    std::string description() const {
        return "Mixed Integer Program from 'Optimizing the ecological "
               "connectivity of landscapes with generalized flow models and "
               "preprocessing', François Hamonic, Cécile Albert, Basile "
               "Couëtoux, Yann Vaxès";
    }
    std::string options_description() const {
        std::ostringstream s;
        s << desc;
        return s.str();
    }
    std::string string() const { return "mip"; }
};

}  // namespace fhamonic

#endif  // LSCP_MIP_INTERFACE_HPP