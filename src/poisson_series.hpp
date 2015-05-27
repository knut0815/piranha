/***************************************************************************
 *   Copyright (C) 2009-2011 by Francesco Biscani                          *
 *   bluescarni@gmail.com                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef PIRANHA_POISSON_SERIES_HPP
#define PIRANHA_POISSON_SERIES_HPP

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "config.hpp"
#include "detail/gcd.hpp"
#include "detail/poisson_series_fwd.hpp"
#include "detail/polynomial_fwd.hpp"
#include "detail/sfinae_types.hpp"
#include "divisor.hpp"
#include "divisor_series.hpp"
#include "exceptions.hpp"
#include "forwarding.hpp"
#include "ipow_substitutable_series.hpp"
#include "is_cf.hpp"
#include "math.hpp"
#include "mp_integer.hpp"
#include "power_series.hpp"
#include "real_trigonometric_kronecker_monomial.hpp"
#include "safe_cast.hpp"
#include "serialization.hpp"
#include "series.hpp"
#include "substitutable_series.hpp"
#include "symbol.hpp"
#include "symbol_set.hpp"
#include "t_substitutable_series.hpp"
#include "term.hpp"
#include "trigonometric_series.hpp"
#include "type_traits.hpp"

namespace piranha
{

namespace detail
{

struct poisson_series_tag {};

// Implementation detail to detect a divisor series.
template <typename T>
struct is_divisor_series
{
	static const bool value = false;
};

template <typename Cf, typename Key>
struct is_divisor_series<divisor_series<Cf,Key>>
{
	static const bool value = true;
};

// Type trait to check whether a Poisson series can provide a linear combination of arguments
// for sine/cosine via the first polynomial coefficient encountered in the hierarchy.
template <typename T, typename = void>
struct ps_has_special_sin_cos
{
	static const bool value = false;
};

template <typename T>
struct ps_has_special_sin_cos<T,typename std::enable_if<(series_recursion_index<T>::value > 0u) &&
	std::is_base_of<detail::polynomial_tag,typename T::term_type::cf_type>::value>::type>
{
	static const bool value = true;
};

template <typename T>
struct ps_has_special_sin_cos<T,typename std::enable_if<(series_recursion_index<T>::value > 0u) &&
	!std::is_base_of<detail::polynomial_tag,typename T::term_type::cf_type>::value>::type>
{
	static const bool value = ps_has_special_sin_cos<typename T::term_type::cf_type>::value;
};

}

/// Poisson series class.
/**
 * This class represents multivariate Poisson series as collections of multivariate Poisson series terms,
 * in which the trigonometric monomials are represented by piranha::rtk_monomial.
 * \p Cf represents the ring over which the Poisson series is defined.
 * 
 * This class satisfies the piranha::is_series type trait.
 * 
 * ## Type requirements ##
 * 
 * \p Cf must be suitable for use in piranha::series as first template argument.
 * 
 * ## Exception safety guarantee ##
 *
 * This class provides the same guarantee as the base series type it derives from.
 *
 * ## Move semantics ##
 *
 * Move semantics is equivalent to the move semantics of the base series type it derives from.
 *
 * ## Serialization ##
 *
 * This class supports serialization if the underlying coefficient type does.
 * 
 * @author Francesco Biscani (bluescarni@gmail.com)
 */
// TODO:
// - make this more general, make the key type selectable;
// - once the above is done, remeber to fix the rebind alias.
// - once we have a selectable key type, we must take care that in a few places we assume that the value type
//   of the key is a C++ integral, but this might not be the case any more (e.g., in the sin/cos implementation
//   we will need a safe cast) -> also in integrate(), there are a few occurrences of this (e.g., == 0 should
//   become math::is_zero() etc.). Will also need the is_integrable check on the key type.
template <typename Cf>
class poisson_series:
	public power_series<ipow_substitutable_series<substitutable_series<t_substitutable_series<trigonometric_series<series<Cf,rtk_monomial,poisson_series<Cf>>>,poisson_series<Cf>>,
		poisson_series<Cf>>,poisson_series<Cf>>,poisson_series<Cf>>,detail::poisson_series_tag
{
		using base = power_series<ipow_substitutable_series<substitutable_series<t_substitutable_series<trigonometric_series<series<Cf,rtk_monomial,poisson_series<Cf>>>,poisson_series<Cf>>,
			poisson_series<Cf>>,poisson_series<Cf>>,poisson_series<Cf>>;
		// Sin/cos utils.
		// Type coming out of sin()/cos() for the base type. These will also be the final types.
		template <typename T>
		using sin_type = decltype(math::sin(std::declval<const typename T::base &>()));
		template <typename T>
		using cos_type = decltype(math::cos(std::declval<const typename T::base &>()));
		// Case 0: Poisson series is not suitable for special sin() implementation. Just forward to the base one, via casting.
		template <typename T = poisson_series, typename std::enable_if<!detail::ps_has_special_sin_cos<T>::value,int>::type = 0>
		sin_type<T> sin_impl() const
		{
			return math::sin(*static_cast<const base *>(this));
		}
		// Case 1: Poisson series is suitable for special sin() implementation. This can fail at runtime depending on what is
		// contained in the coefficients. The return type is the same as the base one, as in this routine we only need operations
		// which are supported by all coefficient types, no need for rebinding or anything like that.
		template <typename T = poisson_series, typename std::enable_if<detail::ps_has_special_sin_cos<T>::value,int>::type = 0>
		sin_type<T> sin_impl() const
		{
			return special_sin_cos<false,sin_type<T>>(*this);
		}
		// Same as above, for cos().
		template <typename T = poisson_series, typename std::enable_if<!detail::ps_has_special_sin_cos<T>::value,int>::type = 0>
		cos_type<T> cos_impl() const
		{
			return math::cos(*static_cast<const base *>(this));
		}
		template <typename T = poisson_series, typename std::enable_if<detail::ps_has_special_sin_cos<T>::value,int>::type = 0>
		cos_type<T> cos_impl() const
		{
			return special_sin_cos<true,cos_type<T>>(*this);
		}
		// Functions to handle the case in which special sin/cos implementation fails. They will just call the base sin/cos, we need
		// them with tag dispatching as the implementation below is generic and needs to cope with potentially different sin/cos return
		// types.
		template <typename T>
		static cos_type<T> special_sin_cos_failure(const T &s, const std::true_type &)
		{
			return math::cos(static_cast<const typename T::base &>(s));
		}
		template <typename T>
		static sin_type<T> special_sin_cos_failure(const T &s, const std::false_type &)
		{
			return math::sin(static_cast<const typename T::base &>(s));
		}
		// Special sin/cos implementation when we have reached the first polynomial coefficient in the hierarchy.
		template <bool IsCos, typename RetT, typename T, typename std::enable_if<std::is_base_of<detail::polynomial_tag,
			typename T::term_type::cf_type>::value,int>::type = 0>
		RetT special_sin_cos(const T &s) const
		{
			// Do something only if the series is equivalent to a polynomial.
			if (s.is_single_coefficient() && !s.empty()) {
				try {
					// Shortcuts.
					typedef typename RetT::term_type term_type;
					typedef typename term_type::cf_type cf_type;
					typedef typename term_type::key_type key_type;
					typedef typename key_type::value_type value_type;
					// Try to get the integral combination from the poly coefficient.
					auto lc = s._container().begin()->m_cf.integral_combination();
					// Change sign if needed.
					bool sign_change = false;
					if (!lc.empty() && lc.begin()->second.sign() < 0) {
						std::for_each(lc.begin(),lc.end(),[](std::pair<const std::string,integer> &p) {p.second.negate();});
						sign_change = true;
					}
					// Return value.
					RetT retval;
					// Build vector of integral multipliers.
					std::vector<value_type> v;
					for (auto it = lc.begin(); it != lc.end(); ++it) {
						retval.m_symbol_set.add(it->first);
						// NOTE: this should probably be a safe_cast.
						// The value type here could be anything, and not guaranteed to be castable,
						// even if in the current implementation this is guaranteed to be a signed
						// int of some kind.
						v.push_back(static_cast<value_type>(it->second));
					}
					// Build term, fix signs and flavour and move-insert it.
					term_type term(cf_type(1),key_type(v.begin(),v.end()));
					if (!IsCos) {
						term.m_key.set_flavour(false);
						if (sign_change) {
							// NOTE: negate is supported by any coefficient type.
							math::negate(term.m_cf);
						}
					}
					retval.insert(std::move(term));
					return retval;
				} catch (const std::invalid_argument &) {
					// Interpret invalid_argument as a failure in extracting integral combination,
					// and move on.
				}
			}
			return special_sin_cos_failure(*this,std::integral_constant<bool,IsCos>{});
		}
		// The coefficient is not a polynomial: recurse to the inner coefficient type, if the current coefficient type
		// is suitable.
		template <bool IsCos, typename RetT, typename T, typename std::enable_if<!std::is_base_of<detail::polynomial_tag,
			typename T::term_type::cf_type>::value,int>::type = 0>
		RetT special_sin_cos(const T &s) const
		{
			if (s.is_single_coefficient() && !s.empty()) {
				return special_sin_cos<IsCos,RetT>(s._container().begin()->m_cf);
			}
			return special_sin_cos_failure(*this,std::integral_constant<bool,IsCos>{});
		}
		// Integration utils.
		// The type resulting from the integration of the key of series T.
		template <typename T>
		using key_integrate_type = decltype(std::declval<const typename T::term_type::key_type &>().integrate(
			std::declval<const symbol &>(),std::declval<const symbol_set &>()).first);
		// Basic integration requirements for series T, to be satisfied both when the coefficient is a polynomial
		// and when it is not. ResT is the type of the result of the integration.
		template <typename T, typename ResT>
		using basic_integrate_requirements = typename std::enable_if<
			// Coefficient differentiable, and can call is_zero on the result.
			has_is_zero<decltype(math::partial(std::declval<const typename T::term_type::cf_type &>(),std::declval<const std::string &>()))>::value &&
			// The result needs to be addable in-place.
			is_addable_in_place<ResT>::value &&
			// It also needs to be ctible from zero.
			std::is_constructible<ResT,int>::value
		>::type;
		// Machinery for the integration of the coefficient only.
		// Type resulting from the integration of the coefficient only.
		template <typename ResT, typename T>
		using i_cf_only_type = decltype(math::integrate(std::declval<const typename T::term_type::cf_type &>(),
			std::declval<const std::string &>()) * std::declval<const T &>());
		// Integration of coefficient only is enabled only if the type above is well defined
		// and it is the same as the result type.
		template <typename ResT, typename T, typename = void>
		struct i_cf_only_enabler
		{
			static const bool value = false;
		};
		template <typename ResT, typename T>
		struct i_cf_only_enabler<ResT,T,typename std::enable_if<
			std::is_same<ResT,i_cf_only_type<ResT,T>>::value>::type>
		{
			static const bool value = true;
		};
		template <typename ResT, typename It, typename T = poisson_series>
		void integrate_coefficient_only(ResT &retval, It it, const std::string &name,
			typename std::enable_if<i_cf_only_enabler<ResT,T>::value>::type * = nullptr) const
		{
			using term_type = typename base::term_type;
			using cf_type = typename term_type::cf_type;
			poisson_series tmp;
			tmp.set_symbol_set(this->m_symbol_set);
			tmp.insert(term_type(cf_type(1),it->m_key));
			retval += math::integrate(it->m_cf,name) * tmp;
		}
		template <typename ResT, typename It, typename T = poisson_series>
		void integrate_coefficient_only(ResT &, It, const std::string &,
			typename std::enable_if<!i_cf_only_enabler<ResT,T>::value>::type * = nullptr) const
		{
			piranha_throw(std::invalid_argument,"unable to perform Poisson series integration: coefficient type is not integrable");
		}
		// Empty for SFINAE.
		template <typename T,typename = void>
		struct integrate_type_
		{};
		// Non-polynomial coefficient.
		template <typename T>
		using npc_res_type = decltype((std::declval<const T &>() * std::declval<const typename T::term_type::cf_type &>()) /
			std::declval<const key_integrate_type<T> &>());
		template <typename T>
		struct integrate_type_<T,typename std::enable_if<!std::is_base_of<detail::polynomial_tag,typename T::term_type::cf_type>::value &&
			detail::true_tt<basic_integrate_requirements<T,npc_res_type<T>>>::value>::type>
		{
			using type = npc_res_type<T>;
		};
		// Polynomial coefficient.
		// Coefficient type divided by the value coming from the integration of the key.
		template <typename T>
		using i_cf_type = decltype(std::declval<const typename T::term_type::cf_type &>() / std::declval<const key_integrate_type<T> &>());
		// Derivative of the type above.
		template <typename T>
		using i_cf_type_p = decltype(math::partial(std::declval<const i_cf_type<T> &>(),std::declval<const std::string &>()));
		// The final return type.
		template <typename T>
		using pc_res_type = decltype(std::declval<const i_cf_type_p<T> &>() * std::declval<const T &>());
		template <typename T>
		struct integrate_type_<T,typename std::enable_if<std::is_base_of<detail::polynomial_tag,typename T::term_type::cf_type>::value &&
			detail::true_tt<basic_integrate_requirements<T,pc_res_type<T>>>::value &&
			// We need to be able to add in the npc type.
			is_addable_in_place<pc_res_type<T>,npc_res_type<T>>::value &&
			// We need to be able to compute the degree of the polynomials and
			// convert it safely to integer.
			has_safe_cast<integer,decltype(math::degree(std::declval<const typename T::term_type::cf_type &>(),std::vector<std::string>{}))>::value &&
			// We need this conversion in the algorithm below.
			std::is_constructible<i_cf_type_p<T>,i_cf_type<T>>::value &&
			// This type needs also to be negated.
			has_negate<i_cf_type_p<T>>::value
			>::type>
		{
			using type = pc_res_type<T>;
		};
		// The final typedef.
		template <typename T>
		using integrate_type = typename integrate_type_<T>::type;
		template <typename T = poisson_series>
		integrate_type<T> integrate_impl(const symbol &s, const typename base::term_type &term,
			const std::true_type &) const
		{
			typedef typename base::term_type term_type;
			typedef typename term_type::cf_type cf_type;
			integer degree;
			try {
				degree = safe_cast<integer>(math::degree(term.m_cf,{s.get_name()}));
			} catch (const std::invalid_argument &) {
				piranha_throw(std::invalid_argument,
					"unable to perform Poisson series integration: cannot convert polynomial degree to an integer");
			}
			// If the variable is in both cf and key, and the cf degree is negative, we cannot integrate.
			if (degree.sign() < 0) {
				piranha_throw(std::invalid_argument,
					"unable to perform Poisson series integration: polynomial coefficient has negative integral degree");
			}
			// Init retval and auxiliary quantities for the iteration.
			auto key_int = term.m_key.integrate(s,this->m_symbol_set);
			// NOTE: here we are sure that the variable is contained in the monomial.
			piranha_assert(key_int.first != 0);
			poisson_series tmp;
			tmp.set_symbol_set(this->m_symbol_set);
			// NOTE: not move for .second, as it is needed in the loop below.
			tmp.insert(term_type(cf_type(1),key_int.second));
			i_cf_type_p<T> p_cf(term.m_cf / std::move(key_int.first));
			integrate_type<T> retval(p_cf * tmp);
			for (integer i(1); i <= degree; ++i) {
				key_int = key_int.second.integrate(s,this->m_symbol_set);
				piranha_assert(key_int.first != 0);
				p_cf = math::partial(p_cf / std::move(key_int.first),s.get_name());
				// Sign change due to the second portion of integration by part.
				math::negate(p_cf);
				tmp = poisson_series{};
				tmp.set_symbol_set(this->m_symbol_set);
				// NOTE: don't move second.
				tmp.insert(term_type(cf_type(1),key_int.second));
				retval += p_cf * tmp;
			}
			return retval;
		}
		template <typename T = poisson_series>
		integrate_type<T> integrate_impl(const symbol &, const typename base::term_type &,
			const std::false_type &) const
		{
			piranha_throw(std::invalid_argument,"unable to perform Poisson series integration: coefficient type is not a polynomial");
		}
		// Time integration.
		// Definition of the divisor type. NOTE: this is temporary and should be changed in the future.
		template <typename T>
		using t_int_div_key_type = divisor<short>;
		template <typename T>
		using t_int_div_cf_type = decltype((std::declval<const typename T::term_type::cf_type &>() * 1) /
			std::declval<const typename t_int_div_key_type<T>::value_type &>());
		template <typename T>
		using ti_type_ = typename std::enable_if<is_cf<t_int_div_cf_type<T>>::value,
			piranha::poisson_series<divisor_series<t_int_div_cf_type<T>,t_int_div_key_type<T>>>>::type;
		// Overload if cf is not a divisor series already. The result will be a Poisson series with the same key type, in which the coefficient
		// is a divisor series whose coefficient is calculated from the operations needed in the integration, and the key type is a divisor whose
		// value type is deduced from the trigonometric key.
		template <typename T = poisson_series, typename std::enable_if<!detail::is_divisor_series<typename T::term_type::cf_type>::value,int>::type = 0>
		ti_type_<T> t_integrate_impl() const
		{
			using return_type = ti_type_<T>;
			// The value type of the trigonometric key.
			using k_value_type = typename base::term_type::key_type::value_type;
			// The divisor type in the return type.
			using div_type = typename return_type::term_type::cf_type::term_type::key_type;
			// Initialise the return value. It has the same set of trig arguments as this.
			return_type retval;
			retval.set_symbol_set(this->m_symbol_set);
			// The symbol set for the divisor series - built from the trigonometric arguments.
			symbol_set div_symbols;
			for (auto it = retval.get_symbol_set().begin(); it != retval.get_symbol_set().end(); ++it) {
				div_symbols.add(std::string("\\nu_{") + it->get_name() + "}");
			}
			// A temp vector of integers used to normalise the divisors coming
			// out of the integration operation from the trig keys.
			std::vector<integer> tmp_int;
			// Build the return value.
			const auto it_f = this->m_container.end();
			for (auto it = this->m_container.begin(); it != it_f; ++it) {
				// Clear the tmp integer vector.
				tmp_int.clear();
				// Get the vector of trigonometric multipliers.
				const auto trig_vector = it->m_key.unpack(this->m_symbol_set);
				// Copy it over to the tmp_int as integer values.
				std::transform(trig_vector.begin(),trig_vector.end(),std::back_inserter(tmp_int),
					[](const k_value_type &n) {return integer(n);});
				// Determine the common divisor.
				// NOTE: both the divisor and the trigonometric key share the canonical form in which the
				// first nonzero multiplier is positive, so we don't need to account for sign flips when
				// constructing a divisor from the trigonometric part. We just need to take care
				// of the common divisor.
				integer cd(0);
				bool first_nonzero_found = false;
				for (auto it2 = tmp_int.begin(); it2 != tmp_int.end(); ++it2) {
					// NOTE: gcd is safe, operating on integers.
					cd = detail::gcd(cd,*it2);
					if (!first_nonzero_found && !math::is_zero(*it2)) {
						piranha_assert(*it2 > 0);
						first_nonzero_found = true;
					}
				}
				if (unlikely(math::is_zero(cd))) {
					piranha_throw(std::invalid_argument,"an invalid trigonometric term was encountered while "
						"attempting a time integration");
				}
				// Take the abs of the cd.
				cd = cd.abs();
				// Divide the vector by the common divisor.
				for (auto it2 = tmp_int.begin(); it2 != tmp_int.end(); ++it2) {
					*it2 /= cd;
				}
				// Build first the divisor series - the coefficient of the term to be inserted
				// into retval.
				typename return_type::term_type::cf_type div_series;
				// Set the arguments of the divisor series.
				div_series.set_symbol_set(div_symbols);
				// The coefficient of the only term of the divisor series is the original coefficient
				// multiplied by any sign change from the integration or the change in sign in the divisors,
				// and divided by the common divisor (cast to the appropriate type).
				typename return_type::term_type::cf_type::term_type::cf_type div_cf = (it->m_cf *
					(it->m_key.get_flavour() ? 1 : -1)) /
					static_cast<typename div_type::value_type>(cd);
				// Build the divisor.
				typename div_type::value_type exponent(1);
				typename return_type::term_type::cf_type::term_type::key_type div_key;
				div_key.insert(tmp_int.begin(),tmp_int.end(),exponent);
				// Insert the term into the divisor series.
				div_series.insert(typename return_type::term_type::cf_type::term_type{std::move(div_cf),std::move(div_key)});
				// Insert into the return value.
				auto tmp_key = it->m_key;
				// Switch the flavour for integration.
				tmp_key.set_flavour(!tmp_key.get_flavour());
				retval.insert(typename return_type::term_type{std::move(div_series),std::move(tmp_key)});
			}
			return retval;
		}
		// Final type definition.
		template <typename T>
		using ti_type = decltype(std::declval<const T &>().t_integrate_impl());
		// Serialization.
		PIRANHA_SERIALIZE_THROUGH_BASE(base)
	public:
		/// Series rebind alias.
		template <typename Cf2>
		using rebind = poisson_series<Cf2>;
		/// Defaulted default constructor.
		/**
		 * Will construct a Poisson series with zero terms.
		 */
		poisson_series() = default;
		/// Defaulted copy constructor.
		poisson_series(const poisson_series &) = default;
		/// Defaulted move constructor.
		poisson_series(poisson_series &&) = default;
		PIRANHA_FORWARDING_CTOR(poisson_series,base)
		/// Trivial destructor.
		~poisson_series()
		{
			PIRANHA_TT_CHECK(is_series,poisson_series);
		}
		/// Defaulted copy assignment operator.
		poisson_series &operator=(const poisson_series &) = default;
		/// Defaulted move assignment operator.
		poisson_series &operator=(poisson_series &&) = default;
		PIRANHA_FORWARDING_ASSIGNMENT(poisson_series,base)
		/// Sine.
		/**
		 * \note
		 * This template method is enabled only if math::sin() can be called on the class
		 * from which piranha::poisson_series derives (i.e., only if the default math::sin()
		 * implementation for series is appropriate).
		 *
		 * In general, this method behaves exactly like the default implementation of piranha::math::sin() for series
		 * types. If, however, a polynomial appears in the hierarchy of coefficients, then this method
		 * will attempt to extract an integral linear combination of symbolic arguments and use it to construct
		 * a Poisson series with a single term, unitary coefficient and the trigonometric key built from the linear
		 * combination.
		 *
		 * For instance, if the calling Poisson series is
		 * \f[
		 * -2x + y,
		 * \f]
		 * then calling this method will produce the Poisson series
		 * \f[
		 * -\sin \left( 2x - y \right).
		 * \f]
		 *
		 * If for any reason it is not possible to extract the linear integral combination from the polynomial part,
		 * then this method will forward the call to the default implementation of piranha::math::sin() for series
		 * types.
		 *
		 * @return the sine of \p this.
		 *
		 * @throws unspecified any exception thrown by:
		 * - piranha::series::is_single_coefficient(), piranha::series::insert(),
		 * - piranha::symbol_set::add(),
		 * - memory allocation errors in standard containers,
		 * - the constructors of coefficient, key and term types,
		 * - the cast operator of piranha::integer,
		 * - piranha::math::negate(),
		 * - piranha::math::sin(),
		 * - the extraction of a linear combination of integral arguments from the polynomial coefficient.
		 */
		template <typename T = poisson_series>
		sin_type<T> sin() const
		{
			return sin_impl();
		}
		/// Cosine.
		/**
		 * This method works in the same way as piranha::poisson_series::sin().
		 *
		 * @return the cosine of \p this.
		 */
		template <typename T = poisson_series>
		cos_type<T> cos() const
		{
			return cos_impl();
		}
		/// Integration.
		/**
		 * \note
		 * This method is enabled only if the algorithm described below is supported by all the involved types.
		 *
		 * This method will attempt to compute the antiderivative of the Poisson series term by term using the
		 * following procedure:
		 * - if the term's monomial does not depend on the integration variable, the integration will be deferred to the coefficient;
		 * - otherwise:
		 *   - if the coefficient does not depend on the integration variable, the monomial is integrated;
		 *   - if the coefficient is a polynomial, a strategy of integration by parts is attempted, its success depending on whether
		 *     the degree of the polynomial is a non-negative integral value;
		 *   - otherwise, an error will be produced.
		 * 
		 * @param[in] name integration variable.
		 * 
		 * @return the antiderivative of \p this with respect to \p name.
		 * 
		 * @throws std::invalid_argument if the integration procedure fails.
		 * @throws unspecified any exception thrown by:
		 * - piranha::symbol construction,
		 * - piranha::math::partial(), piranha::math::is_zero(), piranha::math::integrate(), piranha::safe_cast() and
		 *   piranha::math::negate(),
		 * - the assignment operator of piranha::symbol_set,
		 * - term construction,
		 * - coefficient construction, assignment and arithmetics,
		 * - integration, construction and assignment of the key type,
		 * - insert(),
		 * - piranha::polynomial::degree(),
		 * - series arithmetics.
		 */
		template <typename T = poisson_series>
		integrate_type<T> integrate(const std::string &name) const
		{
			typedef typename base::term_type term_type;
			typedef typename term_type::cf_type cf_type;
			// Turn name into symbol.
			const symbol s(name);
			// Init the return value.
			integrate_type<T> retval(0);
			const auto it_f = this->m_container.end();
			for (auto it = this->m_container.begin(); it != it_f; ++it) {
				// Integrate the key first.
				auto key_int = it->m_key.integrate(s,this->m_symbol_set);
				// If the variable does not appear in the monomial, try deferring the integration
				// to the coefficient.
				if (key_int.first == 0) {
					integrate_coefficient_only(retval,it,name);
					continue;
				}
				// The variable is in the monomial, let's check if the variable is also in the coefficient.
				if (math::is_zero(math::partial(it->m_cf,name))) {
					// No variable in the coefficient, proceed with the integrated key and divide by multiplier.
					poisson_series tmp;
					tmp.set_symbol_set(this->m_symbol_set);
					tmp.insert(term_type(cf_type(1),std::move(key_int.second)));
					retval += (std::move(tmp) * it->m_cf) / key_int.first;
				} else {
					// With the variable both in the coefficient and the key, we only know how to proceed with polynomials.
					retval += integrate_impl(s,*it,std::integral_constant<bool,std::is_base_of<detail::polynomial_tag,Cf>::value>());
				}
			}
			return retval;
		}
		/// Time integration.
		/**
		 * \note
		 * This method is enabled only if:
		 * - the calling series is not already an echeloned Poisson series,
		 * - the operations required by the computation of the time integration are supported by all
		 *   the involved types.
		 *
		 * This is a special type of integration in which the trigonometric arguments are considered as linear functions
		 * of time, and in which the integration variable is time itself. The result of the operation is a so-called echeloned
		 * Poisson series, that it, a Poisson series in which the coefficient is a piranha::divisor_series whose coefficient type
		 * is the original coefficient type of the Poisson series.
		 *
		 * For instance, if the original series is
		 * \f[
		 * \frac{1}{5}z\cos\left( x - y \right),
		 * \f]
		 * the result of the time integration is
		 * \f[
		 * \frac{1}{5}{z}\frac{1}{\left(\nu_{x}-\nu_{y}\right)}\sin{\left({x}-{y}\right)},
		 * \f]
		 * where \f$ \nu_{x} \f$ and \f$ \nu_{y} \f$ are the frequencies associated to \f$ x \f$ and \f$ y \f$ (that is,
		 * \f$ x = \nu_{x}t \f$ and \f$ x = \nu_{y}t \f$).
		 *
		 * This method will throw an error if any term of the calling series has a unitary key (e.g., in the Poisson series
		 * \f$ \frac{1}{5}z \f$ the only trigonometric key is \f$ \cos\left( 0 \right) \f$ and would thus result in a division by zero
		 * during a time integration).
		 *
		 * @return the result of the time integration.
		 *
		 * @throws unspecified any exception thrown by:
		 * - memory errors in standard containers,
		 * - the public interfaces of piranha::symbol_set, piranha::mp_integer and piranha::series,
		 * - piranha::math::is_zero(),
		 * - the mathematical operations needed to compute the result,
		 * - piranha::divisor::insert(),
		 * - construction of the involved types.
		 */
		template <typename T = poisson_series>
		ti_type<T> t_integrate() const
		{
			return t_integrate_impl();
		}
};

namespace detail
{

// Enabler for the math::integrate() specialisation: type needs to be a Poisson series which supports
// the integration method.
template <typename Series>
using ps_integrate_enabler = typename std::enable_if<std::is_base_of<poisson_series_tag,Series>::value &&
	true_tt<decltype(std::declval<const Series &>().integrate(std::declval<const std::string &>()))>::value>::type;

}

namespace math
{

/// Specialisation of the piranha::math::integrate() functor for Poisson series.
/**
 * This specialisation is activated when \p Series is an instance of piranha::poisson_series which supports
 * piranha::poisson_series::integrate().
 */
template <typename Series>
struct integrate_impl<Series,detail::ps_integrate_enabler<Series>>
{
	/// Call operator.
	/**
	 * The implementation will use piranha::poisson_series::integrate().
	 * 
	 * @param[in] s input Poisson series.
	 * @param[in] name integration variable.
	 * 
	 * @return antiderivative of \p s with respect to \p name.
	 * 
	 * @throws unspecified any exception thrown by piranha::poisson_series::integrate().
	 */
	auto operator()(const Series &s, const std::string &name) -> decltype(s.integrate(name))
	{
		return s.integrate(name);
	}
};

}

}

#endif
