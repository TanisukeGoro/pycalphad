/*=============================================================================
	Copyright (c) 2012-2013 Richard Otis

	Based on example code from Boost MultiIndex.
	Copyright (c) 2003-2008 Joaquin M Lopez Munoz.
	See http://www.boost.org/libs/multi_index for library home page.

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// Model for reference Gibbs energy (from pure elements)

#include "libgibbs/include/libgibbs_pch.hpp"
#include "libgibbs/include/models.hpp"
#include "libgibbs/include/optimizer/opt_Gibbs.hpp"
#include <string>
#include <sstream>
#include <set>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/tuple/tuple.hpp>

using boost::spirit::utree;
using boost::multi_index_container;
using namespace boost::multi_index;

sublattice_set build_variable_map(
		const Phase_Collection::const_iterator p_begin,
		const Phase_Collection::const_iterator p_end,
		const evalconditions &conditions
		) {
	sublattice_set ret_set;

	int indexcount = 0; // counter for variable indices (for optimizer)

	// All phases
	for (auto i = p_begin; i != p_end; ++i) {
		auto subl_start = i->second.get_sublattice_iterator();
		auto subl_end = i->second.get_sublattice_iterator_end();
		std::string phasename = i->first;

		// insert fake record for the phase fraction variable at -1 sublattice index
		ret_set.insert(sublattice_entry(-1, indexcount++, 0, phasename, ""));
		// All sublattices
		for (auto j = subl_start; j != subl_end;++j) {
			// All species
			for (auto k = (*j).get_species_iterator(); k != (*j).get_species_iterator_end();++k) {
				// Check if this species in this sublattice is on our list of elements to investigate
				if (std::find(conditions.elements.cbegin(),conditions.elements.cend(),*k) != conditions.elements.cend()) {
					int sublindex = std::distance(subl_start,j);
					double sitecount = (*j).stoi_coef;
					std::string spec = (*k);
					ret_set.insert(sublattice_entry(sublindex, indexcount++, sitecount, phasename, spec));
				}
			}
		}
	}
	return ret_set;
}

utree build_Gibbs_ref(
		const std::string &phasename,
		const sublattice_set &subl_set,
		const parameter_set &param_set
		) {
	utree ret_tree;
	sublattice_set_view ssv;
	parameter_set_view psv;
	parameter_set_view psv_subview;

	// Get all the sublattices for this phase
	boost::multi_index::index<sublattice_set,phases>::type::iterator ic0,ic1;
	boost::tuples::tie(ic0,ic1)=get<phases>(subl_set).equal_range(phasename);

	// Construct a view from the iterators
	while (ic0 != ic1) {
		ssv.insert(*ic0);
		++ic0;
	}

	// Get all the parameters for this phase
	boost::multi_index::index<parameter_set,phase_index>::type::iterator pa_start,pa_end;
	//boost::tuples::tie(pa_start,pa_end)=get<phase_index>(param_set).equal_range(phasename);

	// Construct a view from the iterators
	/*while (pa_start != pa_end) {
		//psv.insert(&*pa_start);
		++pa_start;
	}*/

	// build a subview to the parameters that we are interested in
	parameter_set_view::iterator it0, it1;
	std::string scantype = "G";
	boost::tuples::tie(it0,it1)=psv.equal_range(scantype);

	// Construct a subview from the view
	while (it0 != it1) {
		psv_subview.insert(*it0);
		++it0;
	}

	return permute_site_fractions(ssv, sublattice_set_view(), psv_subview, (int)0);
}

utree permute_site_fractions (
		const sublattice_set_view &total_view, // all sublattices
		const sublattice_set_view &subl_view, // the active sublattice permutation
		const parameter_set_view &param_view,
		const int &sublindex
		) {

	// TODO: optimizations to eliminate products containing 0
	// and remove sums with 0 and products with 1
	utree ret_tree;
	// Construct a view of just the current sublattice
	boost::multi_index::index<sublattice_set_view,myindex>::type::iterator ic0,ic1;
	boost::tuples::tie(ic0,ic1)=get<myindex>(total_view).equal_range(sublindex);

	if (ic0 == ic1) {
		/* We are at the bottom of the recursive loop
		 * or there's an empty sublattice for some reason (bad).
		 * Use the sublattice permutation to find a matching
		 * parameter, if it exists.
		 */

		return find_parameter_ast(subl_view,param_view);
	}

	for (auto i = ic0; i != ic1; ++i) {
		sublattice_set_view temp_view = subl_view;
		utree current_product;
		utree buildtree;
		temp_view.insert((*i)); // add current species to the view
		/* Construct the expression tree.
		 * Start by building the recursive product of site fractions.
		 */
		current_product.push_back("*");
		// The variable will be represented as a string
		const std::string varname = (*i).name();
		current_product.push_back(utree(varname));
		current_product.push_back(
				permute_site_fractions(total_view, temp_view, param_view, sublindex+1)
				);
		// Contribute this product to the sum
		// Check if we are on the first (or only) term in the sum
		if (i != ic0) {
			buildtree.push_back("+");
			buildtree.push_back(ret_tree);
		}
		buildtree.push_back(current_product);
		ret_tree.swap(buildtree);
	}
	return ret_tree;
}

utree find_parameter_ast(const sublattice_set_view &subl_view, const parameter_set_view &param_view) {
	utree ret_tree;
	// build search configuration
	std::vector<std::set<std::string>> search_config;

	auto subl_start = get<myindex>(subl_view).begin();
	auto subl_end = get<myindex>(subl_view).end();

	search_config.reserve(std::distance(subl_start,subl_end));

	while (subl_start != subl_end) {
		search_config[subl_start->index].insert(subl_start->species);
		++subl_start;
	}
	return ret_tree;
}