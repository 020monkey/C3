#pragma once

#include "ByteArray.h"

// TODO Examine forwarding pattern in Write methods.
namespace MWR
{
	// forward declaration
	class ByteView;

	/// Idiom for detecting tuple types.
	template <typename T>
	constexpr bool IsTuple = false;
	template<typename ...T>
	constexpr bool IsTuple<std::tuple<T...>> = true;

	/// An owning container.
	class ByteVector : std::vector<std::uint8_t>
	{
	public:
		/// Privately inherited Type.
		using Super = std::vector<std::uint8_t>;

		/// Type of stored values.
		using ValueType = Super::value_type;

		/// Copy constructor.
		/// @param other. Object to copy.
		ByteVector(ByteVector const& other);

		/// Move constructor.
		/// @param other. Object to move.
		ByteVector(ByteVector&& other);

		/// Copy assignment operator.
		/// @param other. Object to copy.
		ByteVector& operator=(ByteVector const& other);

		/// Move assignment operator.
		/// @param other. Object to move.
		ByteVector& operator=(ByteVector&& other);

		/// Create from std::vector.
		/// @param other. Object to copy.
		ByteVector(std::vector<uint8_t> other);

		/// Write all arguments at the end of ByteVector.
		/// @tparam T. Parameter pack to be written. Types are deduced from function call.
		/// @return itself to allow chaining.
		/// @remarks This function is called only for two or more arguments.
		/// @remarks Each type in parameter pack must have corresponding Write method for one argument.
		template <typename ...T, typename std::enable_if_t<(sizeof...(T) > 1), int> = 0>
		ByteVector& Write(T... args)
		{
			VariadicWriter<T...>::Write(*this, args...);
			return *this;
		}

		/// Concat content of of provided object
		/// @tparam T. Type to be stored. T must be a ContiguousContainer and have T::data(), T::size(), T::value_type
		/// @return itself to allow chaining.
		template<typename T>
		ByteVector & Concat(T arg)
		{
			auto oldSize = size();
			resize(oldSize + arg.size() * sizeof(T::value_type));
			std::memcpy(data() + oldSize, arg.data(), arg.size() * sizeof(T::value_type));

			return *this;
		}

		/// Write content of of provided object prefixed with four byte size.
		/// @tparam T. Type to be stored. Supported types are ByteVector, ByteView, std::string, std::string_view, std::wstring, std::wstring_view.
		/// @return itself to allow chaining.
		template<typename T, typename std::enable_if_t<
			(
				std::is_same_v<T, ByteVector>
				|| std::is_same_v<T, ByteView>
				|| std::is_same_v<T, std::string>
				|| std::is_same_v<T, std::string_view>
				|| std::is_same_v<T, std::wstring>
				|| std::is_same_v<T, std::wstring_view>
			), int> = 0>
		ByteVector& Write(T arg)
		{
			auto oldSize = size();
			resize(oldSize + sizeof(uint32_t) + arg.size() * sizeof(T::value_type));
			*reinterpret_cast<uint32_t*>(data() + oldSize) = static_cast<uint32_t>(arg.size());
			std::memcpy(data() + oldSize + sizeof(uint32_t), arg.data(), arg.size() * sizeof(T::value_type));

			return *this;
		}

		/// Write content of array. Size must be known at compile time and will not be saved in data.
		/// @tparam T. Type to be stored. Supported types are ByteArray.
		/// @return itself to allow chaining.
		template<typename T, typename std::enable_if_t<IsByteArray<T>, int> = 0>
		ByteVector& Write(T arg)
		{
			auto oldSize = size();
			resize(oldSize + arg.size());
			std::memcpy(data() + oldSize, arg.data(), arg.size());

			return *this;
		}

		/// Write arithmetic type.
		/// tparam T. Type to be stored.
		/// @return itself to allow chaining.
		template<typename T, typename std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
		ByteVector & Write(T arg)
		{
			auto oldSize = size();
			resize(oldSize + sizeof(T));
			*reinterpret_cast<T*>(data() + oldSize) = arg;

			return *this;
		}

		/// Write tuple type.
		/// tparam T. Tuple type to be stored.
		/// @return itself to allow chaining.
		template<typename T, typename std::enable_if_t<IsTuple<T>, int> = 0>
		ByteVector & Write(T arg)
		{
			Writer<T>::Write(*this, arg);

			return *this;
		}

		// Enable methods.
		using Super::vector;
		using Super::value_type;
		using Super::allocator_type;
		using Super::size_type;
		using Super::difference_type;
		using Super::reference;
		using Super::const_reference;
		using Super::pointer;
		using Super::const_pointer;
		using Super::iterator;
		using Super::const_iterator;
		using Super::reverse_iterator;
		using Super::const_reverse_iterator;
		using Super::operator=;
		using Super::assign;
		using Super::get_allocator;
		using Super::at;
		using Super::operator[];
		using Super::front;
		using Super::back;
		using Super::data;
		using Super::begin;
		using Super::cbegin;
		using Super::end;
		using Super::cend;
		using Super::rbegin;
		using Super::crbegin;
		using Super::rend;
		using Super::crend;
		using Super::empty;
		using Super::size;
		using Super::max_size;
		using Super::reserve;
		using Super::capacity;
		using Super::shrink_to_fit;
		using Super::clear;
		using Super::insert;
		using Super::emplace;
		using Super::erase;
		using Super::push_back;
		using Super::emplace_back;
		using Super::pop_back;
		using Super::resize;

	private:
		/// Delegate to class idiom.
		/// Function templates cannot be partially specialized.
		/// @tparam T. First type from parameter pack. Each recursive call will unfold one type.
		/// @tparam Rest. Parameter pack storing rest of provided types.
		template <typename T, typename ...Rest>
		struct VariadicWriter
		{
			/// Function responsible for recursively packing data to ByteVector.
			/// @param self. Reference to ByteVector object using VariadicWriter.
			static void Write(ByteVector& self, T current, Rest... rest)
			{
				self.Write(current);
				VariadicWriter<Rest...>::Write(self, rest...);
			}
		};

		/// Delegate to class idiom.
		/// Function templates cannot be partially specialized.
		/// Closure specialization.
		/// @tparam T. Type to be extracted stored in ByteVector.
		template <typename T>
		struct VariadicWriter<T>
		{
			/// Function responsible for recursively packing data to ByteVector.
			/// @param self. Reference to ByteVector object using VariadicWriter.
			static void Write(ByteVector& self, T current)
			{
				self.Write(current);
			}
		};

		/// Delegate to class idiom.
		/// Function templates cannot be partially specialized.
		/// @tparam T. Tuple type to write.
		/// @tparam N. How many elements of tuple to write.
		template <typename T, size_t N = std::tuple_size_v<T>>
		struct Writer
		{
			/// Function responsible for recursively packing data to ByteVector.
			/// @param self. Reference to ByteVector object using VariadicWriter.
			/// @param t. reference to tuple.
			static auto Write(ByteVector& self, T const& t)
			{
				self.Write(std::get<std::tuple_size_v<T> -N>(t));
				Writer<T, N - 1>::Write(self, t);
			}
		};

		/// Delegate to class idiom.
		/// Function templates cannot be partially specialized.
		/// Closure specialization.
		/// @tparam T. Tuple type to write.
		template <typename T>
		struct Writer<T, 0>
		{
			/// Closing function, does nothing.
			static void Write(ByteVector&, T const&)
			{

			}
		};
	};

	/// Checks if the contents of lhs and rhs are equal.
	/// @param lhs. Left hand side of operator.
	/// @param rhs. Right hand side of operator.
	bool operator==(ByteVector const& lhs, ByteVector const& rhs);

	/// Checks if the contents of lhs and rhs are equal.
	/// @param lhs. Left hand side of operator.
	/// @param rhs. Right hand side of operator.
	bool operator!=(ByteVector const& lhs, ByteVector const& rhs);

	namespace Literals
	{
		/// Create ByteVector with syntax ""_bvec
		ByteVector operator "" _b(const char* data, size_t size);

		/// Create ByteVector with syntax L""_bvec
		ByteVector operator "" _b(const wchar_t* data, size_t size);
	}
}
