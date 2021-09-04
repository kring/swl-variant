#ifdef SWL_CPP_LIBRARY_VARIANT_HPP

template <int N>
constexpr int find_first_true(bool (&&arr)[N]){
	for (int k = 0; k < N; ++k)
		if (arr[k]) 
			return k;
	return -1;
}

template <class T, class... Ts>
inline constexpr bool appears_exactly_once = (static_cast<unsigned short>(std::is_same_v<T, Ts>) + ...) == 1;

// ============= type pack element 

template <unsigned char = 1>
struct find_type_i;

template <>
struct find_type_i<1> {
	template <std::size_t Idx, class T, class... Ts>
	using f = typename find_type_i<(Idx != 1)>::template f<Idx - 1, Ts...>;
};

template <>
struct find_type_i<0> {
	template <std::size_t, class T, class... Ts>
	using f = T;
};

template <std::size_t K, class... Ts>
using type_pack_element = typename find_type_i<(K != 0)>::template f<K, Ts...>;

// ============= overload match detector. to be used for variant generic assignment

template <class T>
using arr1 = T[1];

template <std::size_t N, class A>
struct overload_frag {
	using type = A;
	template <class T>
		requires requires { arr1<A>{std::declval<T>()}; }
	auto operator()(A, T&&) -> overload_frag<N, A>;
}; 

template <class Seq, class... Args>
struct make_overload;

template <std::size_t... Idx, class... Args>
struct make_overload<std::integer_sequence<std::size_t, Idx...>, Args...>
	 : overload_frag<Idx, Args>... { 
	using overload_frag<Idx, Args>::operator()...;
};

template <class T, class... Ts>
using best_overload_match = typename decltype( 
	make_overload<std::make_index_sequence<sizeof...(Ts)>, Ts...>{}
	( std::declval<T>(), std::declval<T>() )
)::type;
	
template <class T, class... Ts>
concept has_non_ambiguous_match = 
	requires { typename best_overload_match<T, Ts...>; };

// ================================== rel ops

template <class From, class To>
concept convertible = std::is_convertible_v<From, To>;

template <class T>
concept has_eq_comp = requires (T a, T b) { 
	{ a == b } -> convertible<bool>; 
};

template <class T>
concept has_lesser_comp = requires (T a, T b) { 
	{ a < b } -> convertible<bool>; 
};

template <class T>
concept has_less_or_eq_comp = requires (T a, T b) { 
	{ a <= b } -> convertible<bool>;
};

template <class A>
struct emplace_no_dtor_from_elem {
	template <class T>
	constexpr void operator()(T&& elem, auto index_) const {
		a.template emplace_no_dtor<index_>( static_cast<T&&>(elem) ); 
	}
	A& a;
};

template <class E, class T>
constexpr void destruct(T& obj){
	if constexpr (not std::is_trivially_destructible_v<E>)
		obj.~E();
}
	
// =============================== variant union types
	
// =================== base variant storage type 
// this type is used to build a N-ary tree of union. 

struct dummy_type{}; // used to fill the back of union nodes

using union_index_t = unsigned;

#define TRAIT(trait) ( std::is_##trait##_v<A> && std::is_##trait##_v<B> )

#define SFM(signature, trait) \
	signature = default; \
	signature requires (TRAIT(trait) and not TRAIT(trivially_##trait)) {} 

// given the two members of type A and B of an union X
// this create the proper conditionally trivial special members functions
#define INJECT_UNION_SFM_FRAG(X) \
	SFM(constexpr X (const X &), copy_constructible) \
	SFM(constexpr X (X&&), move_constructible) \
	SFM(constexpr X& operator=(const X&), copy_assignable) \
	SFM(constexpr X& operator=(X&&), move_assignable) \
	SFM(constexpr ~X(), destructible)
	
template <bool IsTerminal, class... Ts>
union variant_union;

template <class A, class B>
union variant_union<false, A, B> {
	
	static constexpr auto elem_size = A::elem_size + B::elem_size;
	
	constexpr variant_union() = default;
	
	template <std::size_t Index, class... Args>
		requires (Index < A::elem_size)
	constexpr variant_union(in_place_index_t<Index>, Args&&... args)
	: a{ in_place_index<Index>, static_cast<Args&&>(args)... } {}
	
	template <std::size_t Index, class... Args>
		requires (Index >= A::elem_size)
	constexpr variant_union(in_place_index_t<Index>, Args&&... args)
	: b{ in_place_index<Index - A::elem_size>, static_cast<Args&&>(args)... } {} 
	
	template <union_index_t Index>
	constexpr auto& get(){
		if constexpr 		( Index < A::elem_size )
			return a.template get<Index>();
		else 
			return b.template get<Index - A::elem_size>();
	}
	
	INJECT_UNION_SFM_FRAG(variant_union)
	
	A a;
	B b;
};

template <class A, class B>
union variant_union<true, A, B> {
	
	static constexpr union_index_t elem_size = not( std::is_same_v<B, dummy_type> ) ? 2 : 1;
	
	constexpr variant_union() = default;
	
	template <class... Args>
	constexpr variant_union(in_place_index_t<0>, Args&&... args)
	: a{static_cast<Args&&>(args)...} {}
	
	template <class... Args>
	constexpr variant_union(in_place_index_t<1>, Args&&... args)
	: b{static_cast<Args&&>(args)...} {}
	
	template <union_index_t Index>
	constexpr auto& get(){
		if constexpr 		( Index == 0 )
			return a;
		else return b;
	}
	
	INJECT_UNION_SFM_FRAG(variant_union)
		
	A a;
	B b;
};

struct valueless_construct_t{};

template <class Impl>
union variant_top_union;

template <class A>
union variant_top_union {
	
	constexpr variant_top_union() = default;
	constexpr variant_top_union(valueless_construct_t) : dummy{} {}
	
	template <std::size_t Idx, class... Args>
	constexpr variant_top_union(in_place_index_t<Idx> tag, Args&&... args) 
	: impl{tag, static_cast<Args&&>(args)...} {}
	
	using B = dummy_type;
	
	INJECT_UNION_SFM_FRAG(variant_top_union)
	
	A impl;
	dummy_type dummy;
};

#undef INJECT_UNION_SFM_FRAG
#undef SFM
#undef TRAIT

// =================== algorithm to build the tree of unions 
// take a sequence of types and perform an order preserving fold until only one type is left
// the first parameter is the numbers of types remaining for the current pass

constexpr unsigned char pick_next(unsigned remaining){
	return remaining >= 2 ? 2 : remaining;
}

template <unsigned char Pick, unsigned char GoOn, bool FirstPass>
struct make_tree;

template <bool IsFirstPass>
struct make_tree<2, 1, IsFirstPass> {
	template <unsigned Remaining, class A, class B, class... Ts>
	using f = typename make_tree<pick_next(Remaining - 2), 
								 sizeof...(Ts) != 0,
								 IsFirstPass
								 >::template f< Remaining - 2, 
								 				Ts..., 
								 				variant_union<IsFirstPass, A, B>
								 			  >; 
};

// only one type left, stop
template <bool F>
struct make_tree<0, 0, F> {
	template <unsigned, class A>
	using f = A;
};

// end of one pass, restart
template <bool IsFirstPass>
struct make_tree<0, 1, IsFirstPass> {
	template <unsigned Remaining, class... Ts>
	using f = typename make_tree<pick_next(sizeof...(Ts)), 
								 (sizeof...(Ts) != 1), 
								 false  // <- both first pass and tail call recurse into a tail call
								>::template f<sizeof...(Ts), Ts...>;
};

// one odd type left in the pass, put it at the back to preserve the order
template <>
struct make_tree<1, 1, false> {
	template <unsigned Remaining, class A, class... Ts>
	using f = typename make_tree<0, sizeof...(Ts) != 0, false>::template f<0, Ts..., A>;
};

// one odd type left in the first pass, wrap it in an union
template <>
struct make_tree<1, 1, true> {
	template <unsigned, class A, class... Ts>
	using f = typename make_tree<0, sizeof...(Ts) != 0, false>
		::template f<0, Ts..., 
					 variant_union<true, A, dummy_type>
					>;
};

template <class... Ts>
using make_tree_union = typename 
	make_tree<pick_next(sizeof...(Ts)), 1, true>::template f<sizeof...(Ts), Ts...>;

// ============================================================

// Ts... must be sorted in ascending size 
template <std::size_t Num, class... Ts>
using smallest_suitable_integer_type = 
	type_pack_element<(static_cast<unsigned char>(Num > std::numeric_limits<Ts>::max()) + ...),
					  Ts...
					  >;

// why do we need this again? i think something to do with GCC? 
namespace swap_trait {
	using std::swap;    
	
	template <class A>
	concept able = requires (A a, A b) { swap(a, b); };
	
	template <class A>
	inline constexpr bool nothrow = noexcept( swap(std::declval<A&>(), std::declval<A&>()) );
}

#ifndef SWL_VARIANT_NO_STD_HASH
	template <class T>
	inline constexpr bool has_std_hash = requires (T t) { 
		std::size_t( ::std::hash< std::remove_cvref_t<T> >{}(t) ); 
	};
#endif

template <class T>
inline constexpr T* addressof( T& obj ) noexcept {
	#if defined(__GNUC__) || defined( __clang__ )
		return __builtin_addressof(obj);
	#elif defined (SWL_VARIANT_NO_CONSTEXPR_EMPLACE)
		// if & is overloaded, use the ugly version
		if constexpr ( requires { obj.operator&(); } )
			return reinterpret_cast<T*>
			(&const_cast<char&>(reinterpret_cast<const volatile char&>(obj)));
		else
			return &obj;
	#else
		return std::address_of(obj);
	#endif
}

#endif // eof
					  