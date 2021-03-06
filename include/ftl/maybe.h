/*
 * Copyright (c) 2013 Björn Aili
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 */
#ifndef FTL_MAYBE_H
#define FTL_MAYBE_H

#include <stdexcept>
#include <type_traits>
#include "monoid.h"
#include "monad.h"

namespace ftl {

	/// Used to distinguish in-place constructors from others
	// TODO: Move to someplace more generic (might be used by other data types)
	struct inplace_tag {};

	/**
	 * \defgroup maybe Maybe
	 *
	 * The maybe data type and associated operations.
	 *
	 * \code
	 *   #include <ftl/maybe.h>
	 * \endcode
	 *
	 * \par Dependencies
	 * - <stdexcept>
	 * - <type_traits>
	 * - \ref monoid
	 * - \ref monad
	 */

	/**
	 * \brief Abstracts the concept of optional arguments and similar.
	 *
	 * In essence, an instance of maybe is either a value, or nothing.
	 * 
	 * \par Concepts
	 * Maybe is an instance of the following concepts:
	 * \li \ref fullycons
	 * \li \ref assignable
	 * \li \ref deref
	 * \li \ref eq, if, and only if, `A` is EqComparable
	 * \li \ref orderable, if, and only if, `A` is Orderable
	 * \li \ref functor (in `A`)
	 * \li \ref applicative (in `A`)
	 * \li \ref monad (in `A`)
	 * \li \ref monoid, if, and only if, `A` is a Monoid
	 *
	 * \ingroup maybe
	 */
	template<typename A>
	class maybe {
	public:
		/**
		 * Compatibility typedef.
		 *
		 * Allows compatibility with plethora of templated functions/structures
		 * that require an object have a value_type member.
		 */
		using value_type = A;

		/**
		 * Default c-tor, equivalent to \c nothing.
		 *
		 * Memory for the contained type is reserved on the stack, but no
		 * initialisation is done. In other words, A's constructor is \em not
		 * called.
		 */
		constexpr maybe() noexcept {}

		/// Copy c-tor
		maybe(const maybe& m)
		noexcept(std::is_nothrow_copy_constructible<A>::value)
	    : isValid(m.isValid) {
			if(isValid) {
				new (&val) value_type(reinterpret_cast<const A&>(m.val));
			}
		}

		/// Move c-tor
		maybe(maybe&& m)
		noexcept(std::is_nothrow_move_constructible<A>::value)
		: isValid(m.isValid) {
			if(isValid) {
				new (&val) value_type(std::move(reinterpret_cast<A&>(m.val)));
				m.isValid = false;
			}
		}

		/**
		 * Construct a value by copy.
		 */
		explicit maybe(const value_type& v)
		noexcept(std::is_nothrow_copy_constructible<A>::value)
		: isValid(true) {
			 new (&val) value_type(v);
		}

		/// Construct a value by move.
		explicit maybe(value_type&& v)
		noexcept(std::is_nothrow_move_constructible<A>::value)
		: isValid(true) {
			new (&val) value_type(std::move(v));
		}

		/**
		 * In-place value construction constructor.
		 *
		 * Constructs a value in the internal storage, forwarding the parameters
		 * to `A`'s constructor.
		 */
		template<typename...Ts>
		maybe(inplace_tag, Ts&&...ts) noexcept(std::is_nothrow_constructible<A,Ts...>::value)
		: isValid(true) {
			new (&val) value_type(std::forward<Ts>(ts)...);
		}

		// TODO: Enable the noexcept specifier once is_nothrow_destructible is
		// available (gcc-4.8).
		~maybe() /*noexcept(std::is_nothrow_destructible<A>::value)*/ {
			self_destruct();
		}

		/**
		 * Check if the maybe is nothing.
		 */
		constexpr bool isNothing() const noexcept {
			return !isValid;
		}

		/**
		 * Check if the maybe is a value.
		 */
		constexpr bool isValue() const noexcept {
			return isValid;
		}

		/// Copy assignment
		const maybe& operator= (const maybe& m)
		/* TODO: Enable noexcept specifier once is_nothrow_destructible is
		 * available.
		noexcept(  std::is_nothrow_copy_constructible<A>::value
				&& std::is_nothrow_destructible<A>::value) */ {
			// Check for self-assignment
			if(this == &m)
				return *this;

			self_destruct();

			isValid = m.isValid;
			if(isValid) {
				new (&val) value_type(m.val);
			}

			return *this;
		}

		/// Move assignment
		const maybe& operator= (maybe&& m)
		/* TODO: Enable noexcept specifier once is_nothrow_destructible is
		 * available.
		noexcept(  std::is_nothrow_copy_constructible<A>::value
				&& std::is_nothrow_destructible<A>::value) */ {
			// Check for self-assignment
			if(this == &m)
				return *this;

			self_destruct();

			isValid = m.isValid;
			if(isValid) {
				new (&val) value_type(std::move(m.val));
				m.isValid = false;
			}

			return *this;
		}

		/**
		 * Bool conversion operator.
		 *
		 * Provided for convenience, to allow syntax such as
		 * \code
		 *   maybe<T> m = ...;
		 *   if(m) {
		 *       doStuff(m);
		 *   }
		 * \endcode
		 */
		constexpr operator bool() const noexcept {
			return isValue();
		}

		/**
		 * Dereference operator.
		 * 
		 * \throws std::logic_error if `this` is `nothing`.
		 */
		value_type& operator* () {
			if(!isValid)
				throw std::logic_error("Attempting to read the value of Nothing.");

			return reinterpret_cast<A&>(val);
		}

		/// \overload
		const value_type& operator* () const {
			if(!isValid)
				throw std::logic_error("Attempting to read the value of Nothing.");

			return reinterpret_cast<const A&>(val);
		}

		/**
		 * Member access operator.
		 * 
		 * \throws std::logic_error if `this` is `nothing`.
		 */
		value_type* operator-> () {
			if(!isValid)
				throw std::logic_error("Attempting to read the value of Nothing.");

			return reinterpret_cast<const A*>(&val);
		}

		/// \overload
		const value_type* operator-> () const {
			if(!isValid)
				throw std::logic_error("Attempting to read the value of Nothing.");

			return reinterpret_cast<const A*>(&val);
		}

		/**
		 * Static constructor of Nothing:s.
		 *
		 * The only purpose of this static method is to be more explicit than
		 * relying on maybe's default constructor. The end result is exactly
		 * equivalent.
		 */
		static constexpr maybe<A> nothing() noexcept {
			return maybe();
		}

	private:
		void self_destruct() {
			if(isValid) {
				reinterpret_cast<A&>(val).~A();
				isValid = false;
			}
		}

		typename std::aligned_storage<
			sizeof(A),
			std::alignment_of<A>::value>::type val;

		bool isValid = false;
	};

	/**
	 * Convenience function to create maybe:s.
	 *
	 * \ingroup maybe
	 */
	template<typename A>
	constexpr maybe<A> value(const A& a)
	noexcept(std::is_nothrow_copy_constructible<A>::value) {
		return maybe<A>(a);
	}

	/**
	 * \overload
	 *
	 * \ingroup maybe
	 */
	template<typename A>
	constexpr maybe<A> value(A&& a)
	noexcept(std::is_nothrow_move_constructible<A>::value) {
		return maybe<A>(std::move(a));
	}

	/**
	 * Equality comparison for maybe.
	 *
	 * \note Instantiating this operator for `A`s that have no equality operator
	 *       of their own will result in compilation error.
	 *
	 * \ingroup maybe
	 */
	template<typename A>
	bool operator== (const maybe<A>& m1, const maybe<A>& m2) {
		if(m1 && m2) {
			return *m1 == *m2;
		}
		else if(!m1 && !m2) {
			return true;
		}
		else {
			return false;
		}
	}

	/**
	 * Not equal to operator for maybe.
	 *
	 * \note Instantiating this operator for `A`s that have no `operator!=`
	 *       of their own will result in compilation error.
	 *
	 * \ingroup maybe
	 */
	template<typename A>
	bool operator!= (const maybe<A>& m1, const maybe<A>& m2) {
		return !(m1 == m2);
	}

	/**
	 * Less than operator.
	 *
	 * \note Will result in compilation error if `A` is not Orderable.
	 *
	 * \ingroup maybe
	 */
	template<typename A>
	bool operator< (const maybe<A>& m1, const maybe<A>& m2) {
		if(m1) {
			if(m2) {
				return *m1 < *m2;
			}
			else {
				return false;
			}
		}
		else if(m2) {
			return true;
		}
		else {
			return false;
		}
	}

	/**
	 * Greater than operator.
	 *
	 * \note Will result in compilation error if `A` is not Orderable.
	 *
	 * \ingroup maybe
	 */
	template<typename A>
	bool operator> (const maybe<A>& m1, const maybe<A>& m2) {
		if(m1) {
			if(m2) {
				return *m1 > *m2;
			}
			else {
				return true;
			}
		}
		else if(m2) {
			return false;
		}
		else {
			return false;
		}
	}

	/**
	 * Monoid implementation for maybe.
	 *
	 * Semantics are:
	 * \code
	 *   id() <=> maybe::nothing() <=> maybe()
	 *   append(value(x), value(y)) <=> value(append(x, y))
	 *   append(value(x), maybe::nothing()) <=> value(x)
	 *   append(maybe::nothing, value(y)) <=> value(y)
	 *   append(maybe::nothing, maybe::nothing) <=> maybe::nothing
	 * \endcode
	 *
	 * In other words, the append operation is simply lifted into the
	 * \c value_type of the maybe and all nothings are ignored (unless
	 * everything is nothing).
	 *
	 * \ingroup maybe
	 */
	template<typename A>
	struct monoid<maybe<A>> {
		static constexpr maybe<A> id() {
			return maybe<A>();
		}

		static maybe<A> append(const maybe<A>& m1, const maybe<A>& m2) {
			if(m1) {
				if(m2) {
					return maybe<A>(monoid<A>::append(*m1, *m2));
				}

				else {
					return m1;
				}
			}
			else if(m2) {
				return m2;
			}

			else {
				return maybe<A>();
			}
		}

		static constexpr bool instance = monoid<A>::instance;
	};

	/**
	 * Implementation of monad for maybe.
	 *
	 * \note This automatically gives maybe default applicative and functor
	 *       instances.
	 *
	 * \ingroup maybe
	 */
	template<>
	struct monad<maybe> {
		template<typename A>
		static constexpr maybe<A> pure(const A& a)
		noexcept(std::is_nothrow_copy_constructible<A>::value) {
			return value(a);
		}

		template<typename A>
		static constexpr maybe<A> pure(A&& a)
		noexcept(std::is_nothrow_move_constructible<A>::value) {
			return value(std::move(a));
		}

		/**
		 * Apply `f` if `m` is a value.
		 */
		template<
			typename F,
			typename A,
			typename B = typename decayed_result<F(A)>::type>
		static maybe<B> map(F f, const maybe<A>& m) {
			return m ? value(f(*m)) : maybe<B>();
		}
		

		/**
		 * Applies a function to unwrapped maybe value.
		 *
		 * \tparam F must satisfy \ref fn<maybe<B>(A)>
		 */
		template<
			typename F,
			typename A,
			typename B = typename decayed_result<F(A)>::type::value_type>
		static maybe<B> bind(const maybe<A>& m, F f) {
			return m ? f(*m) : maybe<B>();
		}

		static constexpr bool instance = true;
	};
}

#endif

