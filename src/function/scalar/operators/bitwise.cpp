#include "duckdb/function/scalar/operators.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/common/types/cast_helpers.hpp"
#include "duckdb/function/scalar/bit_functions.hpp"
#include "duckdb/common/types/bit.hpp"

namespace duckdb {

template <class OP>
static scalar_function_t GetScalarIntegerUnaryFunction(const LogicalType &type) {
	scalar_function_t function;
	switch (type.id()) {
	case LogicalTypeId::TINYINT:
		function = &ScalarFunction::UnaryFunction<int8_t, int8_t, OP>;
		break;
	case LogicalTypeId::SMALLINT:
		function = &ScalarFunction::UnaryFunction<int16_t, int16_t, OP>;
		break;
	case LogicalTypeId::INTEGER:
		function = &ScalarFunction::UnaryFunction<int32_t, int32_t, OP>;
		break;
	case LogicalTypeId::BIGINT:
		function = &ScalarFunction::UnaryFunction<int64_t, int64_t, OP>;
		break;
	case LogicalTypeId::UTINYINT:
		function = &ScalarFunction::UnaryFunction<uint8_t, uint8_t, OP>;
		break;
	case LogicalTypeId::USMALLINT:
		function = &ScalarFunction::UnaryFunction<uint16_t, uint16_t, OP>;
		break;
	case LogicalTypeId::UINTEGER:
		function = &ScalarFunction::UnaryFunction<uint32_t, uint32_t, OP>;
		break;
	case LogicalTypeId::UBIGINT:
		function = &ScalarFunction::UnaryFunction<uint64_t, uint64_t, OP>;
		break;
	case LogicalTypeId::HUGEINT:
		function = &ScalarFunction::UnaryFunction<hugeint_t, hugeint_t, OP>;
		break;
	default:
		throw NotImplementedException("Unimplemented type for GetScalarIntegerUnaryFunction");
	}
	return function;
}

template <class OP>
static scalar_function_t GetScalarBitstringUnaryFunction(const LogicalType &type) {

	if (type != LogicalTypeId::BIT) {
		throw NotImplementedException("Unimplemented type for GetScalarBitstringUnaryFunction");
	}
	return &ScalarFunction::UnaryFunction<string_t, string_t, OP>;
}

template <class OP>
static scalar_function_t GetScalarIntegerBinaryFunction(const LogicalType &type) {
	scalar_function_t function;
	switch (type.id()) {
	case LogicalTypeId::TINYINT:
		function = &ScalarFunction::BinaryFunction<int8_t, int8_t, int8_t, OP>;
		break;
	case LogicalTypeId::SMALLINT:
		function = &ScalarFunction::BinaryFunction<int16_t, int16_t, int16_t, OP>;
		break;
	case LogicalTypeId::INTEGER:
		function = &ScalarFunction::BinaryFunction<int32_t, int32_t, int32_t, OP>;
		break;
	case LogicalTypeId::BIGINT:
		function = &ScalarFunction::BinaryFunction<int64_t, int64_t, int64_t, OP>;
		break;
	case LogicalTypeId::UTINYINT:
		function = &ScalarFunction::BinaryFunction<uint8_t, uint8_t, uint8_t, OP>;
		break;
	case LogicalTypeId::USMALLINT:
		function = &ScalarFunction::BinaryFunction<uint16_t, uint16_t, uint16_t, OP>;
		break;
	case LogicalTypeId::UINTEGER:
		function = &ScalarFunction::BinaryFunction<uint32_t, uint32_t, uint32_t, OP>;
		break;
	case LogicalTypeId::UBIGINT:
		function = &ScalarFunction::BinaryFunction<uint64_t, uint64_t, uint64_t, OP>;
		break;
	case LogicalTypeId::HUGEINT:
		function = &ScalarFunction::BinaryFunction<hugeint_t, hugeint_t, hugeint_t, OP>;
		break;
	default:
		throw NotImplementedException("Unimplemented type for GetScalarIntegerBinaryFunction");
	}
	return function;
}

template <class OP>
static scalar_function_t GetScalarBitstringBinaryFunction(const LogicalType &type) {

	if (type != LogicalTypeId::BIT) {
		throw NotImplementedException("Unimplemented type for GetScalarBitstringBinaryFunction");
	}
	return &ScalarFunction::BinaryFunction<string_t, string_t, string_t, OP>;
}

//===--------------------------------------------------------------------===//
// & [bitwise_and]
//===--------------------------------------------------------------------===//
struct BitwiseANDOperator {
	template <class TA, class TB, class TR>
	static inline TR Operation(TA left, TB right) {
		return left & right;
	}
};

void BitwiseAndFun::RegisterFunction(BuiltinFunctions &set) {
	ScalarFunctionSet functions("&");
	for (auto &type : LogicalType::Integral()) {
		functions.AddFunction(
		    ScalarFunction({type, type}, type, GetScalarIntegerBinaryFunction<BitwiseANDOperator>(type)));
	}
	functions.AddFunction(ScalarFunction({LogicalType::BIT, LogicalType::BIT}, LogicalType::BIT,
	                                     GetScalarBitstringBinaryFunction<BitwiseANDOperator>(LogicalType::BIT)));
	set.AddFunction(functions);
}

//===--------------------------------------------------------------------===//
// | [bitwise_or]
//===--------------------------------------------------------------------===//
struct BitwiseOROperator {
	template <class TA, class TB, class TR>
	static inline TR Operation(TA left, TB right) {
		return left | right;
	}
};

void BitwiseOrFun::RegisterFunction(BuiltinFunctions &set) {
	ScalarFunctionSet functions("|");
	for (auto &type : LogicalType::Integral()) {
		functions.AddFunction(
		    ScalarFunction({type, type}, type, GetScalarIntegerBinaryFunction<BitwiseOROperator>(type)));
	}
	functions.AddFunction(ScalarFunction({LogicalType::BIT, LogicalType::BIT}, LogicalType::BIT,
	                                     GetScalarBitstringBinaryFunction<BitwiseOROperator>(LogicalType::BIT)));
	set.AddFunction(functions);
}

//===--------------------------------------------------------------------===//
// # [bitwise_xor]
//===--------------------------------------------------------------------===//
struct BitwiseXOROperator {
	template <class TA, class TB, class TR>
	static inline TR Operation(TA left, TB right) {
		return left ^ right;
	}
};

void BitwiseXorFun::RegisterFunction(BuiltinFunctions &set) {
	ScalarFunctionSet functions("xor");
	for (auto &type : LogicalType::Integral()) {
		functions.AddFunction(
		    ScalarFunction({type, type}, type, GetScalarIntegerBinaryFunction<BitwiseXOROperator>(type)));
	}
	functions.AddFunction(ScalarFunction({LogicalType::BIT, LogicalType::BIT}, LogicalType::BIT,
	                                     GetScalarBitstringBinaryFunction<BitwiseXOROperator>(LogicalType::BIT)));
	set.AddFunction(functions);
}

//===--------------------------------------------------------------------===//
// << [bitwise_left_shift]
//===--------------------------------------------------------------------===//

struct BitwiseShiftLeftOperator {
	template <class TA, class TB, class TR>
	static inline TR Operation(TA input, TB shift) {
		TA max_shift = TA(sizeof(TA) * 8);
		if (input < 0) {
			throw OutOfRangeException("Cannot left-shift negative number %s", NumericHelper::ToString(input));
		}
		if (shift < 0) {
			throw OutOfRangeException("Cannot left-shift by negative number %s", NumericHelper::ToString(shift));
		}
		if (shift >= max_shift) {
			if (input == 0) {
				return 0;
			}
			throw OutOfRangeException("Left-shift value %s is out of range", NumericHelper::ToString(shift));
		}
		if (shift == 0) {
			return input;
		}
		TA max_value = (TA(1) << (max_shift - shift - 1));
		if (input >= max_value) {
			throw OutOfRangeException("Overflow in left shift (%s << %s)", NumericHelper::ToString(input),
			                          NumericHelper::ToString(shift));
		}
		return input << shift;
	}
};

template <>
inline string_t BitwiseShiftLeftOperator::Operation<string_t, int32_t, string_t>(string_t input, int32_t shift) {

	int32_t max_shift = input.GetSize() * 8;
	if (shift == 0) {
		return input;
	}
	if (shift < 0) {
		throw OutOfRangeException("Cannot left-shift by negative number %s", NumericHelper::ToString(shift));
	}
	if (shift >= max_shift) {
		return (string_t(input.GetSize())); // all zero bit string
	}
	// return input << shift;
	return Bit::LeftShift(input, shift);
}

void LeftShiftFun::RegisterFunction(BuiltinFunctions &set) {
	ScalarFunctionSet functions("<<");
	for (auto &type : LogicalType::Integral()) {
		functions.AddFunction(
		    ScalarFunction({type, type}, type, GetScalarIntegerBinaryFunction<BitwiseShiftLeftOperator>(type)));
	}
	functions.AddFunction(
	    ScalarFunction({LogicalType::BIT, LogicalType::INTEGER}, LogicalType::BIT,
	                   &ScalarFunction::BinaryFunction<string_t, int32_t, string_t, BitwiseShiftLeftOperator>));
	set.AddFunction(functions);
}

//===--------------------------------------------------------------------===//
// >> [bitwise_right_shift]
//===--------------------------------------------------------------------===//
template <class T>
bool RightShiftInRange(T shift) {
	return shift >= 0 && shift < T(sizeof(T) * 8);
}

struct BitwiseShiftRightOperator {
	template <class TA, class TB, class TR>
	static inline TR Operation(TA input, TB shift) {
		return RightShiftInRange(shift) ? input >> shift : 0;
	}
};

template <>
inline string_t BitwiseShiftRightOperator::Operation<string_t, int32_t, string_t>(string_t input, int32_t shift) {
	int32_t max_shift = input.GetSize() * 8;
	if (shift == 0) {
		return input;
	}
	if (shift < 0 || shift >= max_shift) {
		return (string_t(input.GetSize())); // all zero bit string
	}
	// return input >> shift;
	return Bit::RightShift(input, shift);
}

void RightShiftFun::RegisterFunction(BuiltinFunctions &set) {
	ScalarFunctionSet functions(">>");
	for (auto &type : LogicalType::Integral()) {
		functions.AddFunction(
		    ScalarFunction({type, type}, type, GetScalarIntegerBinaryFunction<BitwiseShiftRightOperator>(type)));
	}
	functions.AddFunction(
	    ScalarFunction({LogicalType::BIT, LogicalType::INTEGER}, LogicalType::BIT,
	                   &ScalarFunction::BinaryFunction<string_t, int32_t, string_t, BitwiseShiftRightOperator>));
	set.AddFunction(functions);
}

//===--------------------------------------------------------------------===//
// ~ [bitwise_not]
//===--------------------------------------------------------------------===//
struct BitwiseNotOperator {
	template <class TA, class TR>
	static inline TR Operation(TA input) {
		return ~input;
	}
};

void BitwiseNotFun::RegisterFunction(BuiltinFunctions &set) {
	ScalarFunctionSet functions("~");
	for (auto &type : LogicalType::Integral()) {
		functions.AddFunction(ScalarFunction({type}, type, GetScalarIntegerUnaryFunction<BitwiseNotOperator>(type)));
	}
	functions.AddFunction(ScalarFunction({LogicalType::BIT}, LogicalType::BIT,
	                                     GetScalarBitstringUnaryFunction<BitwiseNotOperator>(LogicalType::BIT)));
	set.AddFunction(functions);
}

//===--------------------------------------------------------------------===//
// get_bit
//===--------------------------------------------------------------------===//
struct GetBitOperator {
	template <class TA, class TB, class TR>
	static inline TR Operation(TA input, TB n) {
		if (n < 0 || (idx_t)n > Bit::BitLength(input) - 1) {
			throw OutOfRangeException("bit index %s out of valid range (0..%s)", NumericHelper::ToString(n),
			                          NumericHelper::ToString(Bit::BitLength(input) - 1));
		}
		return Bit::GetBit(input, n);
	}
};

void GetBitFun::RegisterFunction(BuiltinFunctions &set) {
	set.AddFunction(ScalarFunction("get_bit", {LogicalType::BIT, LogicalType::INTEGER}, LogicalType::INTEGER,
	                               ScalarFunction::BinaryFunction<string_t, int32_t, int32_t, GetBitOperator>));
}

//===--------------------------------------------------------------------===//
// set_bit
//===--------------------------------------------------------------------===//
struct SetBitOperator {
	template <class TA, class TB, class TC, class TR>
	static inline TR Operation(TA input, TB n, TC new_value) {
		if (new_value != 0 && new_value != 1) {
			throw InvalidInputException("The new bit must be 1 or 0");
		}
		if (n < 0 || (idx_t)n > Bit::BitLength(input) - 1) {
			throw OutOfRangeException("bit index %s out of valid range (0..%s)", NumericHelper::ToString(n),
			                          NumericHelper::ToString(Bit::BitLength(input) - 1));
		}
		Bit::SetBit(input, n, new_value);
		return input;
	}
};

void SetBitFun::RegisterFunction(BuiltinFunctions &set) {
	set.AddFunction(
	    ScalarFunction("set_bit", {LogicalType::BIT, LogicalType::INTEGER, LogicalType::INTEGER}, LogicalType::BIT,
	                   ScalarFunction::TernaryFunction<string_t, int32_t, int32_t, string_t, SetBitOperator>));
}

//===--------------------------------------------------------------------===//
// bit_position
//===--------------------------------------------------------------------===//
struct BitPositionOperator {
	template <class TA, class TB, class TR>
	static inline TR Operation(TA substring, TB input) {
		if (substring.GetSize() > input.GetSize()) {
			return 0;
		}
		return Bit::BitPosition(substring, input);
	}
};

void BitPositionFun::RegisterFunction(BuiltinFunctions &set) {
	set.AddFunction(ScalarFunction("bit_position", {LogicalType::BIT, LogicalType::BIT}, LogicalType::INTEGER,
	                               ScalarFunction::BinaryFunction<string_t, string_t, int32_t, BitPositionOperator>));
}

} // namespace duckdb
