#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
namespace logging = boost::log;

// #include "landscape_opt/solvers/mip_xue.hpp"
// #include "landscape_opt/solvers/mip_eca.hpp"
// #include "landscape_opt/solvers/mip_eca_preprocessed.hpp"
// #include "landscape_opt/solvers/randomized_rounding.hpp"

#include "instance.hpp"
#include "parse_instance.hpp"

#include "solver_interfaces/abstract_solver.hpp"
#include "solver_interfaces/greedy_decremental_interface.hpp"
#include "solver_interfaces/greedy_incremental_interface.hpp"
#include "solver_interfaces/mip_interface.hpp"
#include "solver_interfaces/static_decremental_interface.hpp"
#include "solver_interfaces/static_incremental_interface.hpp"

using namespace fhamonic;

void init_logging() {
    logging::core::get()->set_filter(logging::trivial::severity >=
                                     logging::trivial::warning);
}

void print_paragraph(std::ostream & os, std::size_t offset,
                     std::size_t column_width, const std::string & str) {
    std::size_t line_start = 0;

    while(str.size() - line_start > column_width) {
        std::size_t n = str.rfind(' ', line_start + column_width);
        if(n <= line_start) {
            os << str.substr(line_start, column_width - 1) << '-' << '\n'
               << std::string(offset, ' ');
            line_start += column_width - 1;
        } else {
            os << str.substr(line_start, n - line_start) << '\n'
               << std::string(offset, ' ');
            line_start = n + 1;
        }
    }

    os << str.substr(line_start) << '\n';
}

static bool process_command_line(
    const std::vector<std::string> & args,
    std::shared_ptr<AbstractSolver> & solver,
    std::filesystem::path & instances_description_json_file, double & budget,
    bool & output_in_file, std::filesystem::path & output_csv_file) {
    std::vector<std::shared_ptr<AbstractSolver>> solver_interfaces{
        std::make_unique<StaticIncrementalInterface>(),
        std::make_unique<StaticDecrementalInterface>(),
        std::make_unique<GreedyIncrementalInterface>(),
        std::make_unique<GreedyDecrementalInterface>(),
        std::make_unique<MIPInterface>()};

    auto print_soft_name = []() { std::cout << "LSCP 0.1\n\n"; };
    auto print_usage = []() {
        std::cout << R"(Usage:
  lcsp_solve --help
  lcsp_solve --list-algorithms
  lcsp_solve <algorithm> --list-params
  lcsp_solve <algorithm> <instance> <budget> [<params> ...]

)";
    };

    auto print_available_algorithms = [&solver_interfaces]() {
        std::cout << "Available solving algorithms:\n";
        const std::size_t algorithm_name_max_length = std::ranges::max(
            std::ranges::views::transform(solver_interfaces, [&](auto && s) {
                return s->name().size();
            }));

        const std::size_t offset =
            std::max(algorithm_name_max_length + 1, std::size_t(24));

        for(auto & s : solver_interfaces) {
            std::cout << "  " << s->name()
                      << std::string(offset - s->name().size() - 2, ' ');
            print_paragraph(std::cout, offset, 80 - offset, s->description());
        }
        std::cout << std::endl;
        return false;
    };

    std::string solver_name;

    try {
        po::options_description desc("Allowed options");
        desc.add_options()("help,h", "Display this help message")(
            "list-algorithms", "List the available algorithms")(
            "list-params", "List the parameters of the chosen algorithm")(
            "algorithm,a", po::value<std::string>(&solver_name)->required(),
            "Algorithm to use")(
            "instance,i",
            po::value<std::filesystem::path>(&instances_description_json_file)
                ->required(),
            "Instance JSON file")(
            "budget,B", po::value<double>(&budget)->required(), "Budget value")(
            "output,o", po::value<std::filesystem::path>(&output_csv_file),
            "Output solution in CSV file");

        po::positional_options_description p;
        p.add("algorithm", 1);
        p.add("instance", 1);
        p.add("budget", 1);
        po::variables_map vm;
        po::parsed_options parsed = po::basic_command_line_parser(args)
                                        .options(desc)
                                        .positional(p)
                                        .allow_unregistered()
                                        .run();
        po::store(parsed, vm);

        if(vm.count("help")) {
            print_soft_name();
            print_usage();

            std::cout << desc << std::endl;
            return false;
        }

        if(vm.count("list-algorithms")) {
            print_soft_name();
            print_available_algorithms();
            return false;
        }

        if(vm.count("algorithm")) {
            solver_name = vm["algorithm"].as<std::string>();
            bool found = false;
            for(auto & s : solver_interfaces) {
                if(s->name() == solver_name) {
                    solver = std::move(s);
                    found = true;
                    break;
                }
            }
            if(!found) {
                std::cout << "Error: the argument ('" << solver_name
                          << "') for option '--algorithm' is invalid\n\n";
                print_available_algorithms();
                return false;
            }
        }

        if(vm.count("list-params")) {
            if(!vm.count("algorithm")) {
                throw std::logic_error(std::string(
                    "Option '--list-params' requires option '--algorithm'."));
            }
            print_soft_name();
            std::cout << solver->options_description();
            std::cout << std::endl;
            return false;
        }

        po::notify(vm);
        std::vector<std::string> opts =
            po::collect_unrecognized(parsed.options, po::exclude_positional);

        solver->parse(opts);
        output_in_file = vm.count("output");
    } catch(std::exception & e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
    return true;
}

int main(int argc, const char * argv[]) {
    std::shared_ptr<AbstractSolver> solver;
    std::filesystem::path instances_description_json;
    double budget;
    bool output_in_file;
    std::filesystem::path output_csv;

    std::vector<std::string> args(argv + 1, argv + argc);
    bool valid_command =
        process_command_line(args, solver, instances_description_json, budget,
                             output_in_file, output_csv);
    if(!valid_command) return EXIT_FAILURE;
    init_logging();

    Instance instance = parse_instance(instances_description_json);

    Instance::Solution solution = solver->solve(instance, budget);

    if(!output_in_file) {
        std::cout << "Solution:" << std::endl;
        const std::size_t option_name_max_length = std::ranges::max(
            std::ranges::views::transform(instance.options(), [&](auto && o) {
                return instance.option_name(o).size();
            }));
        for(auto && o : instance.options()) {
            std::cout << "  " << instance.option_name(o)
                      << std::string(option_name_max_length + 1 -
                                         instance.option_name(o).size(),
                                     ' ')
                      << solution[o] << '\n';
        }
        std::cout << std::endl;

    } else {
        std::ofstream output_file(output_csv);
        output_file << "option_id,value\n";
        for(auto && o : instance.options()) {
            output_file << instance.option_name(o) << ',' << solution[o]
                        << '\n';
        }
    }

    return EXIT_SUCCESS;
}
