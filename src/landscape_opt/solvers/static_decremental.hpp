#ifndef LANDSCAPE_OPT_SOLVERS_STATIC_DECREMENTAL_HPP
#define LANDSCAPE_OPT_SOLVERS_STATIC_DECREMENTAL_HPP

#include <execution>

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/zip.hpp>

#include "concepts/instance.hpp"
#include "helper.hpp"
#include "indices/eca.hpp"
#include "utils/chrono.hpp"

namespace fhamonic {
namespace landscape_opt {
namespace solvers {

struct StaticDecremental {
    bool verbose = false;
    bool parallel = false;

    template <concepts::Instance I>
    typename I::Solution solve(const I & instance, const double budget) const {
        using Landscape = typename I::Landscape;
        using QualityMap = typename Landscape::QualityMap;
        using ProbabilityMap = typename Landscape::ProbabilityMap;
        using Option = typename I::Option;
        using Solution = typename I::Solution;

        Chrono chrono;
        Solution solution = instance.create_solution();
        int time_ms = 0;

        const auto nodeOptions = detail::computeOptionsForNodes(instance);
        const auto arcOptions = detail::computeOptionsForArcs(instance);

        QualityMap quality_map = instance.landscape().quality_map();
        ProbabilityMap probability_map = instance.landscape().probability_map();

        QualityMap enhanced_qm = quality_map;
        ProbabilityMap enhanced_pm = probability_map;
        for(auto && option : instance.options()) {
            for(auto && [u, quality_gain] : nodeOptions[option])
                enhanced_qm[u] += quality_gain;
            for(auto && [a, enhanced_prob] : arcOptions[option])
                enhanced_pm[a] = std::max(enhanced_pm[a], enhanced_prob);
        }

        const double enhanced_eca =
            eca(instance.landscape().graph(), enhanced_qm, enhanced_pm);
        if(verbose) {
            std::cout << "ECA with all improvments: " << enhanced_eca
                      << std::endl;
        }

        std::vector<Option> options =
            ranges::to<std::vector>(instance.options());
        std::vector<double> options_ratios(options.size());

        double purchaised = 0.0;
        for(auto && option : instance.options()) {
            purchaised += instance.option_cost(option);
            solution[option] = 1.0;
        }

        auto compute_dec = [&](Option option) {
            QualityMap qm = enhanced_qm;
            ProbabilityMap pm = enhanced_pm;

            for(auto && [u, quality_gain] : nodeOptions[option])
                enhanced_qm[u] -= quality_gain;
            for(auto && [a, enhanced_prob] : arcOptions[option]) {
                enhanced_pm[a] = probability_map[a];
                for(auto && [enhanced_prob, i] :
                    instance.arc_options_map()[a]) {
                    if(option == i) continue;
                    enhanced_pm[a] = std::max(enhanced_pm[a], enhanced_prob);
                }
            }

            const double decreased_eca =
                eca(instance.landscape().graph(), qm, pm);
            const double ratio =
                (enhanced_eca - decreased_eca) / instance.option_cost(option);

            return ratio;
        };

        if(parallel)
            std::transform(std::execution::par_unseq, options.begin(),
                           options.end(), options_ratios.begin(), compute_dec);
        else
            std::transform(std::execution::seq, options.begin(), options.end(),
                           options_ratios.begin(), compute_dec);

        auto zipped_view_dec = ranges::view::zip(options_ratios, options);
        ranges::sort(zipped_view_dec, [](auto && e1, auto && e2) {
            return e1.first < e2.first;
        });

        std::vector<Option> free_options;

        for(Option option : options) {
            if(purchaised <= budget) break;
            const double price = instance.option_cost(option);
            purchaised -= price;
            solution[option] = 0.0;
            free_options.emplace_back(option);
            if(verbose) {
                std::cout << "remove option: " << option
                          << "\n\t costing: " << price
                          << "\n\t total cost: " << purchaised << std::endl;
            }
        }

        typename Landscape::QualityMap current_qm = quality_map;
        typename Landscape::ProbabilityMap current_pm = probability_map;

        for(Option option : options) {
            if(solution[option] == 0.0) continue;
            for(auto && [u, quality_gain] : nodeOptions[option])
                current_qm[u] += quality_gain;
            for(auto && [a, enhanced_prob] : arcOptions[option])
                current_pm[a] = std::max(current_pm[a], enhanced_prob);
        }

        const double current_eca =
            eca(instance.landscape().graph(), current_qm, current_pm);

        auto compute_inc = [&](Option option) {
            typename Landscape::QualityMap qm = current_qm;
            typename Landscape::ProbabilityMap pm = current_pm;

            for(auto && [u, quality_gain] : nodeOptions[option])
                qm[u] += quality_gain;
            for(auto && [a, enhanced_prob] : arcOptions[option])
                pm[a] = std::max(pm[a], enhanced_prob);

            const double increased_eca =
                eca(instance.landscape().graph(), qm, pm);
            const double ratio =
                (increased_eca - current_eca) / instance.option_cost(option);

            return ratio;
        };

        options_ratios.resize(free_options.size());

        if(parallel)
            std::transform(std::execution::par_unseq, free_options.begin(),
                           free_options.end(), options_ratios.begin(),
                           compute_inc);
        else
            std::transform(std::execution::seq, free_options.begin(),
                           free_options.end(), options_ratios.begin(),
                           compute_inc);

        auto zipped_view_inc = ranges::view::zip(options_ratios, free_options);
        ranges::sort(zipped_view_inc, [](auto && e1, auto && e2) {
            return e1.first > e2.first;
        });

        for(Option option : free_options) {
            const double price = instance.option_cost(option);
            if(purchaised + price > budget) continue;
            purchaised += price;
            solution[option] = 1.0;
            if(verbose) {
                std::cout << "add option: " << option
                          << "\n\t costing: " << price
                          << "\n\t total cost: " << purchaised << std::endl;
            }
        }

        time_ms = chrono.timeMs();

        return solution;
    }
};

}  // namespace solvers
}  // namespace landscape_opt
}  // namespace fhamonic

#endif  // LANDSCAPE_OPT_SOLVERS_STATIC_DECREMENTAL_HPP