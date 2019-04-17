#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <liblangutil/SourceLocation.h>
#include <libsolidity/ast/Types.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace langutil;

namespace dev
{
namespace solidity
{
const string ASTBoogieUtils::BOOGIE_ADDRESS_TYPE = "address_t";
const string ASTBoogieUtils::SOLIDITY_BALANCE = "balance";
const string ASTBoogieUtils::BOOGIE_BALANCE = "__balance";
const string ASTBoogieUtils::SOLIDITY_TRANSFER = "transfer";
const string ASTBoogieUtils::BOOGIE_TRANSFER = "__transfer";
const string ASTBoogieUtils::SOLIDITY_SEND = "send";
const string ASTBoogieUtils::BOOGIE_SEND = "__send";
const string ASTBoogieUtils::SOLIDITY_CALL = "call";
const string ASTBoogieUtils::BOOGIE_CALL = "__call";
const string ASTBoogieUtils::SOLIDITY_SUPER = "super";

const string ASTBoogieUtils::SOLIDITY_SENDER = "sender";
const string ASTBoogieUtils::SOLIDITY_VALUE = "value";
const string ASTBoogieUtils::BOOGIE_MSG_SENDER = "__msg_sender";
const string ASTBoogieUtils::BOOGIE_MSG_VALUE = "__msg_value";

const string ASTBoogieUtils::SOLIDITY_ASSERT = "assert";
const string ASTBoogieUtils::SOLIDITY_REQUIRE = "require";
const string ASTBoogieUtils::SOLIDITY_REVERT = "revert";

const string ASTBoogieUtils::SOLIDITY_THIS = "this";
const string ASTBoogieUtils::BOOGIE_THIS = "__this";
const string ASTBoogieUtils::VERIFIER_MAIN = "__verifier_main";
const string ASTBoogieUtils::VERIFIER_SUM = "__verifier_sum";
const string ASTBoogieUtils::BOOGIE_CONSTRUCTOR = "__constructor";
const string ASTBoogieUtils::BOOGIE_LENGTH = "#length";
const string ASTBoogieUtils::BOOGIE_SUM = "#sum";
const string ASTBoogieUtils::BOOGIE_STRING_TYPE = "string_t";
const string ASTBoogieUtils::BOOGIE_BOOL_TYPE = "bool";
const string ASTBoogieUtils::BOOGIE_INT_TYPE = "int";
const string ASTBoogieUtils::BOOGIE_INT_CONST_TYPE = "int_const";
const string ASTBoogieUtils::ERR_TYPE = "__ERROR_UNSUPPORTED_TYPE";
const string ASTBoogieUtils::BOOGIE_ZERO_ADDRESS = "__zero__address";
const string ASTBoogieUtils::SOLIDITY_NOW = "now";
const string ASTBoogieUtils::BOOGIE_NOW = "__now";
const string ASTBoogieUtils::VERIFIER_OVERFLOW = "__verifier_overflow";

const string ASTBoogieUtils::ERR_EXPR = "__ERROR";

boogie::ProcDeclRef ASTBoogieUtils::createTransferProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value, amount
	list<boogie::Binding> transferParams{
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_VALUE, context.isBvEncoding() ? "bv256" : "int"},
		{"amount", context.isBvEncoding() ? "bv256" : "int"}
	};

	// Body
	boogie::Block::Ref transferImpl = boogie::Block::block();
	boogie::Expr::Ref this_bal = boogie::Expr::sel(BOOGIE_BALANCE, BOOGIE_THIS);
	boogie::Expr::Ref sender_bal = boogie::Expr::sel(BOOGIE_BALANCE, BOOGIE_MSG_SENDER);
	boogie::Expr::Ref amount = boogie::Expr::id("amount");

	// Precondition: there is enough ether to transfer
	auto geqResult = encodeArithBinaryOp(context, nullptr, langutil::Token::GreaterThanOrEqual, sender_bal, amount, 256, false);
	transferImpl->addStmt(boogie::Stmt::assume(geqResult.first));
	// balance[this] += amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		transferImpl->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)));
		transferImpl->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, amount, 256, false);
	if (context.overflow())
	{
		transferImpl->addStmt(boogie::Stmt::comment("Implicit assumption that balances cannot overflow"));
		transferImpl->addStmt(boogie::Stmt::assume(addBalance.second));
	}
	transferImpl->addStmt(boogie::Stmt::assign(
			boogie::Expr::id(BOOGIE_BALANCE),
			boogie::Expr::upd(boogie::Expr::id(BOOGIE_BALANCE), boogie::Expr::id(BOOGIE_THIS), addBalance.first)));
	// balance[msg.sender] -= amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		transferImpl->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(sender_bal, tp_uint256)));
		transferImpl->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto subSenderBalance = encodeArithBinaryOp(context, nullptr, Token::Sub, sender_bal, amount, 256, false);
	if (context.overflow())
	{
		transferImpl->addStmt(boogie::Stmt::comment("Implicit assumption that balances cannot overflow"));
		transferImpl->addStmt(boogie::Stmt::assume(subSenderBalance.second));
	}
	transferImpl->addStmt(boogie::Stmt::assign(
			boogie::Expr::id(BOOGIE_BALANCE),
			boogie::Expr::upd(boogie::Expr::id(BOOGIE_BALANCE), boogie::Expr::id(BOOGIE_MSG_SENDER), subSenderBalance.first)));
	transferImpl->addStmt(boogie::Stmt::comment("TODO: call fallback, exception handling"));

	boogie::ProcDeclRef transfer = boogie::Decl::procedure(BOOGIE_TRANSFER, transferParams, {}, {}, {transferImpl});

	transfer->addAttr(boogie::Attr::attr("inline", 1));
	transfer->addAttr(boogie::Attr::attr("message", "transfer"));
	return transfer;
}

boogie::ProcDeclRef ASTBoogieUtils::createCallProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value
	list<boogie::Binding> callParams {
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_VALUE, context.isBvEncoding() ? "bv256" : "int"}
	};

	// Return value
	list<boogie::Binding> callReturns{ {"__result", "bool"} };

	// Body
	// Successful transfer
	boogie::Block::Ref thenBlock = boogie::Block::block();
	boogie::Expr::Ref this_bal = boogie::Expr::sel(BOOGIE_BALANCE, BOOGIE_THIS);
	boogie::Expr::Ref msg_val = boogie::Expr::id(BOOGIE_MSG_VALUE);
	boogie::Expr::Ref result = boogie::Expr::id("__result");

	// balance[this] += msg.value
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		thenBlock->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)));
		thenBlock->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(msg_val, tp_uint256)));
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, msg_val, 256, false);
	if (context.overflow())
	{
		thenBlock->addStmt(boogie::Stmt::comment("Implicit assumption that balances cannot overflow"));
		thenBlock->addStmt(boogie::Stmt::assume(addBalance.second));
	}
	thenBlock->addStmt(boogie::Stmt::assign(
			boogie::Expr::id(BOOGIE_BALANCE),
			boogie::Expr::upd(boogie::Expr::id(BOOGIE_BALANCE), boogie::Expr::id(BOOGIE_THIS), addBalance.first)));
	thenBlock->addStmt(boogie::Stmt::assign(result, boogie::Expr::lit(true)));
	// Unsuccessful transfer
	boogie::Block::Ref elseBlock = boogie::Block::block();
	elseBlock->addStmt(boogie::Stmt::assign(result, boogie::Expr::lit(false)));
	// Nondeterministic choice between success and failure
	boogie::Block::Ref callBlock = boogie::Block::block();
	callBlock->addStmt(boogie::Stmt::comment("TODO: call fallback"));
	callBlock->addStmt(boogie::Stmt::ifelse(boogie::Expr::id("*"), thenBlock, elseBlock));

	boogie::ProcDeclRef callProc = boogie::Decl::procedure(BOOGIE_CALL, callParams, callReturns, {}, {callBlock});
	callProc->addAttr(boogie::Attr::attr("inline", 1));
	callProc->addAttr(boogie::Attr::attr("message", "call"));
	return callProc;
}

boogie::ProcDeclRef ASTBoogieUtils::createSendProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value, amount
	list<boogie::Binding> sendParams {
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{ASTBoogieUtils::BOOGIE_MSG_VALUE, context.isBvEncoding() ? "bv256" : "int"},
		{"amount", context.isBvEncoding() ? "bv256" : "int"}
	};

	// Return value
	list<boogie::Binding> sendReturns{ {"__result", "bool"} };

	// Body
	// Successful transfer
	boogie::Block::Ref thenBlock = boogie::Block::block();
	boogie::Expr::Ref this_bal = boogie::Expr::sel(BOOGIE_BALANCE, BOOGIE_THIS);
	boogie::Expr::Ref sender_bal = boogie::Expr::sel(BOOGIE_BALANCE, BOOGIE_MSG_SENDER);
	boogie::Expr::Ref amount = boogie::Expr::id("amount");
	boogie::Expr::Ref result = boogie::Expr::id("__result");

	// balance[this] += amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		thenBlock->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)));
		thenBlock->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, amount, 256, false);
	if (context.overflow())
	{
		thenBlock->addStmt(boogie::Stmt::comment("Implicit assumption that balances cannot overflow"));
		thenBlock->addStmt(boogie::Stmt::assume(addBalance.second));
	}
	thenBlock->addStmt(boogie::Stmt::assign(
			boogie::Expr::id(BOOGIE_BALANCE),
			boogie::Expr::upd(boogie::Expr::id(BOOGIE_BALANCE), boogie::Expr::id(BOOGIE_THIS), addBalance.first)));
	// balance[msg.sender] -= amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		thenBlock->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(sender_bal, tp_uint256)));
		thenBlock->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto subSenderBalance = encodeArithBinaryOp(context, nullptr, Token::Sub, sender_bal, amount, 256, false);
	if (context.overflow())
	{
		thenBlock->addStmt(boogie::Stmt::comment("Implicit assumption that balances cannot overflow"));
		thenBlock->addStmt(boogie::Stmt::assume(subSenderBalance.second));
	}
	thenBlock->addStmt(boogie::Stmt::assign(
			boogie::Expr::id(BOOGIE_BALANCE),
			boogie::Expr::upd(boogie::Expr::id(BOOGIE_BALANCE), boogie::Expr::id(BOOGIE_MSG_SENDER), subSenderBalance.first)));
	thenBlock->addStmt(boogie::Stmt::assign(result, boogie::Expr::lit(true)));
	// Unsuccessful transfer
	boogie::Block::Ref elseBlock = boogie::Block::block();
	elseBlock->addStmt(boogie::Stmt::assign(result, boogie::Expr::lit(false)));

	boogie::Block::Ref transferBlock = boogie::Block::block();
	// Precondition: there is enough ether to transfer
	auto senderBalanceGEQ = encodeArithBinaryOp(context, nullptr, langutil::Token::GreaterThanOrEqual, sender_bal, amount, 256, false);
	transferBlock->addStmt(boogie::Stmt::assume(senderBalanceGEQ.first));
	// Nondeterministic choice between success and failure
	transferBlock->addStmt(boogie::Stmt::comment("TODO: call fallback"));
	transferBlock->addStmt(boogie::Stmt::ifelse(boogie::Expr::id("*"), thenBlock, elseBlock));

	boogie::ProcDeclRef sendProc = boogie::Decl::procedure(BOOGIE_SEND, sendParams, sendReturns, {}, {transferBlock});

	sendProc->addAttr(boogie::Attr::attr("inline", 1));
	sendProc->addAttr(boogie::Attr::attr("message", "send"));
	return sendProc;
}

string ASTBoogieUtils::mapDeclName(Declaration const& decl)
{
	// Check for special names
	if (decl.name() == VERIFIER_MAIN) return "main";
	if (decl.name() == SOLIDITY_ASSERT) return SOLIDITY_ASSERT;
	if (decl.name() == SOLIDITY_REQUIRE) return SOLIDITY_REQUIRE;
	if (decl.name() == SOLIDITY_REVERT) return SOLIDITY_REVERT;
	if (decl.name() == SOLIDITY_THIS) return BOOGIE_THIS;
	if (decl.name() == SOLIDITY_NOW) return BOOGIE_NOW;

	// ID is important to append, since (1) even fully qualified names can be
	// same for state variables and local variables in functions, (2) return
	// variables might have no name (whereas Boogie requires a name)
	return decl.name() + "#" + to_string(decl.id());
}

string ASTBoogieUtils::boogieBVType(unsigned n) {
	return "bv" + to_string(n);
}

string ASTBoogieUtils::mapType(TypePointer tp, ASTNode const& _associatedNode, BoogieContext& context)
{
	Type::Category tpCategory = tp->category();

	switch (tpCategory) {
	case Type::Category::Address:
		return BOOGIE_ADDRESS_TYPE;
	case Type::Category::StringLiteral:
		return BOOGIE_STRING_TYPE;
	case Type::Category::Bool:
		return BOOGIE_BOOL_TYPE;
	case Type::Category::RationalNumber: {
		auto tpRational = dynamic_pointer_cast<RationalNumberType const>(tp);
		if (!tpRational->isFractional()) {
			return BOOGIE_INT_CONST_TYPE;
		} else {
			context.reportError(&_associatedNode, "Fractional numbers are not supported");
		}
		break;
	}
	case Type::Category::Integer: {
		auto tpInteger = dynamic_pointer_cast<IntegerType const>(tp);
		if (context.isBvEncoding()) {
			return boogieBVType(tpInteger->numBits());
		} else {
			return BOOGIE_INT_TYPE;
		}
	}
	case Type::Category::Contract:
		return BOOGIE_ADDRESS_TYPE;
	case Type::Category::Array: {
		auto arrType = dynamic_cast<ArrayType const*>(&*tp);
		if (arrType->isString()) {
			return BOOGIE_STRING_TYPE;
		} else {
			if (context.isBvEncoding()) {
				return "[" + boogieBVType(256) + "]" + mapType(arrType->baseType(), _associatedNode, context);
			} else {
				return "[" + BOOGIE_INT_TYPE + "]" + mapType(arrType->baseType(), _associatedNode, context);
			}
		}
	}
	case Type::Category::Mapping: {
		auto mappingType = dynamic_cast<MappingType const*>(&*tp);
		return "[" + mapType(mappingType->keyType(), _associatedNode, context) + "]"
				+ mapType(mappingType->valueType(), _associatedNode, context);
	}
	case Type::Category::FixedBytes: {
		// up to 32 bytes
		auto fbType = dynamic_cast<FixedBytesType const*>(&*tp);
		if (context.isBvEncoding()) {
			return boogieBVType(fbType->numBytes() * 8);
		} else {
			if (fbType->numBytes() == 1) return BOOGIE_INT_TYPE;
			else return "[" + BOOGIE_INT_TYPE + "]" + BOOGIE_INT_TYPE;
		}
	}
	case Type::Category::Tuple:
		context.reportError(&_associatedNode, "Tuples are not supported");
		break;
	default: {
		std::string tpStr = tp->toString();
		context.reportError(&_associatedNode, "Unsupported type: '" + tpStr.substr(0, tpStr.find(' ')) + "'");
	}
	}

	return ERR_TYPE;
}

list<boogie::Attr::Ref> ASTBoogieUtils::createAttrs(SourceLocation const& loc, std::string const& message, Scanner const& scanner)
{
	int srcLine, srcCol;
	tie(srcLine, srcCol) = scanner.translatePositionToLineColumn(loc.start);
	return {
		boogie::Attr::attr("sourceloc", loc.source->name(), srcLine + 1, srcCol + 1),
		boogie::Attr::attr("message", message)
	};
}

ASTBoogieUtils::expr_pair ASTBoogieUtils::encodeArithBinaryOp(BoogieContext& context, ASTNode const* associatedNode, langutil::Token op,
		 boogie::Expr::Ref lhs, boogie::Expr::Ref rhs, unsigned bits, bool isSigned)
{
	boogie::Expr::Ref result = nullptr;
	boogie::Expr::Ref ecc = nullptr;

	switch(context.encoding())
	{
	case BoogieContext::Encoding::INT:
		switch(op)
		{
		case Token::Add: result = boogie::Expr::plus(lhs, rhs); break;
		case Token::Sub: result = boogie::Expr::minus(lhs, rhs); break;
		case Token::Mul: result = boogie::Expr::times(lhs, rhs); break;
		// TODO: returning integer division is fine, because Solidity does not support floats yet
		case Token::Div: result = boogie::Expr::intdiv(lhs, rhs); break;
		case Token::Mod: result = boogie::Expr::mod(lhs, rhs); break;

		case Token::Equal: result = boogie::Expr::eq(lhs, rhs); break;
		case Token::NotEqual: result = boogie::Expr::neq(lhs, rhs); break;
		case Token::LessThan: result = boogie::Expr::lt(lhs, rhs); break;
		case Token::GreaterThan: result = boogie::Expr::gt(lhs, rhs); break;
		case Token::LessThanOrEqual: result = boogie::Expr::lte(lhs, rhs); break;
		case Token::GreaterThanOrEqual: result = boogie::Expr::gte(lhs, rhs); break;

		case Token::Exp:
			if (auto rhsLit = dynamic_pointer_cast<boogie::IntLit const>(rhs))
			{
				if (auto lhsLit = dynamic_pointer_cast<boogie::IntLit const>(lhs))
				{
					result = boogie::Expr::lit(boost::multiprecision::pow(lhsLit->getVal(), rhsLit->getVal().convert_to<unsigned>()));
				}
			}
			context.reportError(associatedNode, "Exponentiation is not supported in 'int' encoding");
			if (result == nullptr) {
				result = boogie::Expr::id(ERR_EXPR);
			}
			break;
		default:
			context.reportError(associatedNode, string("Unsupported binary operator in 'int' encoding ") + TokenTraits::toString(op));
			result = boogie::Expr::id(ERR_EXPR);
		}
		break;
	case BoogieContext::Encoding::BV:
		{
			string name;
			string retType;

			switch (op) {
			case Token::Add: name = "add"; retType = boogieBVType(bits); break;
			case Token::Sub: name = "sub"; retType = boogieBVType(bits); break;
			case Token::Mul: name = "mul"; retType = boogieBVType(bits); break;
			case Token::Div: name = isSigned ? "sdiv" : "udiv"; retType = boogieBVType(bits); break;

			case Token::BitAnd: name = "and"; retType = boogieBVType(bits); break;
			case Token::BitOr: name = "or"; retType = boogieBVType(bits); break;
			case Token::BitXor: name = "xor"; retType = boogieBVType(bits); break;
			case Token::SAR: name = isSigned ? "ashr" : "lshr"; retType = boogieBVType(bits); break;
			case Token::SHL: name = "shl"; retType = boogieBVType(bits); break;

			case Token::Equal: result = boogie::Expr::eq(lhs, rhs); break;
			case Token::NotEqual: result = boogie::Expr::neq(lhs, rhs); break;

			case Token::LessThan: name = isSigned ? "slt" : "ult"; retType = "bool"; break;
			case Token::GreaterThan: name = isSigned ? "sgt" : "ugt"; retType = "bool";  break;
			case Token::LessThanOrEqual: name = isSigned ? "sle" : "ule"; retType = "bool";  break;
			case Token::GreaterThanOrEqual: name = isSigned ? "sge" : "uge"; retType = "bool";  break;

			default:
				context.reportError(associatedNode, string("Unsupported binary operator in 'bv' encoding ") + TokenTraits::toString(op));
				result = boogie::Expr::id(ERR_EXPR);
			}
			if (result == nullptr) { // not computd yet, no error
				string fullName = boogieBVType(bits) + name;
				context.includeBuiltInFunction(fullName, boogie::Decl::function(
							fullName, {{"", boogieBVType(bits)}, {"", boogieBVType(bits)}}, retType, nullptr,
							{boogie::Attr::attr("bvbuiltin", "bv" + name)}));
				result = boogie::Expr::fn(fullName, lhs, rhs);
			}
		}
		break;
	case BoogieContext::Encoding::MOD:
		{
			auto modulo = boogie::Expr::lit(boost::multiprecision::pow(boogie::bigint(2), bits));
			auto largestSigned = boogie::Expr::lit(boost::multiprecision::pow(boogie::bigint(2), bits - 1) - 1);
			auto smallestSigned = boogie::Expr::lit(-boost::multiprecision::pow(boogie::bigint(2), bits - 1));
			switch(op)
			{
			case Token::Add:
			{
				auto sum = boogie::Expr::plus(lhs, rhs);
				if (isSigned)
				{
					result = boogie::Expr::cond(boogie::Expr::gt(sum, largestSigned),
						boogie::Expr::minus(sum, modulo),
						boogie::Expr::cond(boogie::Expr::lt(sum, smallestSigned), boogie::Expr::plus(sum, modulo), sum));
				}
				else
				{
					result = boogie::Expr::cond(boogie::Expr::gte(sum, modulo), boogie::Expr::minus(sum, modulo), sum);
				}
				ecc = boogie::Expr::eq(sum, result);
				break;
			}
			case Token::Sub:
			{
				auto diff = boogie::Expr::minus(lhs, rhs);
				if (isSigned)
				{
					result = boogie::Expr::cond(boogie::Expr::gt(diff, largestSigned),
						boogie::Expr::minus(diff, modulo),
						boogie::Expr::cond(boogie::Expr::lt(diff, smallestSigned), boogie::Expr::plus(diff, modulo), diff));
				}
				else
				{
					result = boogie::Expr::cond(boogie::Expr::gte(lhs, rhs), diff, boogie::Expr::plus(diff, modulo));
				}
				ecc = boogie::Expr::eq(diff, result);
				break;
			}
			case Token::Mul:
			{
				auto prod = boogie::Expr::times(lhs, rhs);
				if (isSigned)
				{
					auto lhs1 = boogie::Expr::cond(boogie::Expr::gte(lhs, boogie::Expr::lit((long)0)), lhs, boogie::Expr::plus(modulo, lhs));
					auto rhs1 = boogie::Expr::cond(boogie::Expr::gte(rhs, boogie::Expr::lit((long)0)), rhs, boogie::Expr::plus(modulo, rhs));
					auto prod = boogie::Expr::mod(boogie::Expr::times(lhs1, rhs1), modulo);
					result = boogie::Expr::cond(boogie::Expr::gt(prod, largestSigned), boogie::Expr::minus(prod, modulo), prod);
				}
				else
				{
					result = boogie::Expr::cond(boogie::Expr::gte(prod, modulo), boogie::Expr::mod(prod, modulo), prod);
				}
				ecc = boogie::Expr::eq(prod, result);
				break;
			}
			case Token::Div:
			{
				auto div = boogie::Expr::intdiv(lhs, rhs);
				if (isSigned)
				{
					result = boogie::Expr::cond(boogie::Expr::gt(div, largestSigned),
						boogie::Expr::minus(div, modulo),
						boogie::Expr::cond(boogie::Expr::lt(div, smallestSigned), boogie::Expr::plus(div, modulo), div));
				}
				else
				{
					result = div;
				}
				ecc = boogie::Expr::eq(div, result);
				break;
			}

			case Token::Equal:
				result = boogie::Expr::eq(lhs, rhs);
				break;
			case Token::NotEqual:
				result = boogie::Expr::neq(lhs, rhs);
				break;
			case Token::LessThan:
				result = boogie::Expr::lt(lhs, rhs);
				break;
			case Token::GreaterThan:
				result = boogie::Expr::gt(lhs, rhs);
				break;
			case Token::LessThanOrEqual:
				result = boogie::Expr::lte(lhs, rhs);
				break;
			case Token::GreaterThanOrEqual:
				result = boogie::Expr::gte(lhs, rhs);
				break;
			default:
				context.reportError(associatedNode, string("Unsupported binary operator in 'mod' encoding ") + TokenTraits::toString(op));
				result = boogie::Expr::id(ERR_EXPR);
			}
		}
		break;
	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unknown encoding"));
	}

	assert(result != nullptr);
	return expr_pair(result, ecc);
}

ASTBoogieUtils::expr_pair ASTBoogieUtils::encodeArithUnaryOp(BoogieContext& context, ASTNode const* associatedNode, Token op,
		boogie::Expr::Ref subExpr, unsigned bits, bool isSigned)
{
	boogie::Expr::Ref result = nullptr;
	boogie::Expr::Ref ecc = nullptr;

	switch(context.encoding())
	{
	case BoogieContext::Encoding::INT:
		switch(op)
		{
		case Token::Add: result = subExpr; break; // Unary plus does not do anything
		case Token::Sub: result = boogie::Expr::neg(subExpr); break;
		default:
			context.reportError(associatedNode, string("Unsupported unary operator in 'int' encoding ") + TokenTraits::toString(op));
			result = boogie::Expr::id(ERR_EXPR);
			break;
		}
		break;

	case BoogieContext::Encoding::BV:
		{
			string name("");
			switch (op)
			{
			case Token::Add: result = subExpr; break; // Unary plus does not do anything

			case Token::Sub: name = "neg"; break;
			case Token::BitNot: name = "not"; break;
			default:
				context.reportError(associatedNode, string("Unsupported unary operator in 'bv' encoding ") + TokenTraits::toString(op));
				result = boogie::Expr::id(ERR_EXPR);
				break;
			}
			if (!result)
			{
				string fullName = boogieBVType(bits) + name;
				context.includeBuiltInFunction(fullName, boogie::Decl::function(
								fullName, {{"", boogieBVType(bits)}}, boogieBVType(bits), nullptr,
								{boogie::Attr::attr("bvbuiltin", "bv" + name)}));

				result = boogie::Expr::fn(fullName, subExpr);
			}
		}
		break;

	case BoogieContext::Encoding::MOD:
		switch(op)
		{
		case Token::Add: result = subExpr; break; // Unary plus does not do anything
		case Token::Sub:
		{
			auto sub = boogie::Expr::neg(subExpr);
			if (isSigned)
			{
				auto smallestSigned = boogie::Expr::lit(-boost::multiprecision::pow(boogie::bigint(2), bits - 1));
				result = boogie::Expr::cond(boogie::Expr::eq(subExpr, smallestSigned),
						smallestSigned,
						sub);
			}
			else
			{
				auto modulo = boogie::Expr::lit(boost::multiprecision::pow(boogie::bigint(2), bits));
				result = boogie::Expr::cond(boogie::Expr::eq(subExpr, boogie::Expr::lit((long)0)),
						boogie::Expr::lit((long)0),
						boogie::Expr::minus(modulo, subExpr));
			}
			ecc = boogie::Expr::eq(sub, result);
			break;
		}
		default:
			context.reportError(associatedNode, string("Unsupported unary operator in 'mod' encoding ") + TokenTraits::toString(op));
			result = boogie::Expr::id(ERR_EXPR);
			break;
		}
		break;

	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unknown encoding"));
	}
	assert(result != nullptr);
	return expr_pair(result, ecc);
}

bool ASTBoogieUtils::isBitPreciseType(TypePointer type)
{
	if (!type) { return false; }
	boost::regex regex{"u?int\\d(\\d)?(\\d?)"}; // uintXXX and intXXX
	return boost::regex_match(type->toString(), regex);
}

unsigned ASTBoogieUtils::getBits(TypePointer type)
{
	if (!isBitPreciseType(type)) { return 0; }
	string typeStr = type->toString();
	return stoi(typeStr.substr(typeStr.find("t") + 1)); // Parse stuff after the last 't' in 'int' or 'uint'
}

bool ASTBoogieUtils::isSigned(TypePointer type)
{
	if (!isBitPreciseType(type)) { return false; }
	string typeStr = type->toString();
	return typeStr[0] == 'i'; // Check if 'int' or 'uint'
}

boogie::Expr::Ref ASTBoogieUtils::checkImplicitBvConversion(boogie::Expr::Ref expr, TypePointer exprType, TypePointer targetType, BoogieContext& context)
{
	// Do nothing if any of the types is unknown
	if (!targetType || !exprType) { return expr; }

	if (isBitPreciseType(targetType))
	{
		unsigned targetBits = getBits(targetType);
		// Create bitvector from literals
		if (auto exprLit = dynamic_pointer_cast<boogie::IntLit const>(expr))
		{
			if (exprLit->getVal() < 0) // Negative literals are tricky
			{
				string fullName = boogieBVType(targetBits) + "neg";
				context.includeBuiltInFunction(fullName, boogie::Decl::function(
								fullName, {{"", boogieBVType(targetBits)}}, boogieBVType(targetBits), nullptr,
								{boogie::Attr::attr("bvbuiltin", "bvneg")}));
				return boogie::Expr::fn(fullName, boogie::Expr::lit(-exprLit->getVal(), targetBits));
			}
			else
			{
				return boogie::Expr::lit(exprLit->getVal(), targetBits);
			}
		}
		else if (isBitPreciseType(exprType))
		{
			unsigned exprBits = getBits(exprType);
			bool targetSigned = isSigned(targetType);
			bool exprSigned = isSigned(exprType);

			// Nothing to do if size and signedness is the same
			if (targetBits == exprBits && targetSigned == exprSigned) { return expr; }
			// Conversion to smaller type should have already been detected by the compiler
			if (targetBits < exprBits)
			{
				BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Implicit conversion to smaller type"));
				return nullptr;
			}

			if (!exprSigned) // Unsigned can be converted to larger (signed or unsigned) with zero extension
			{
				string fullName = "bvzeroext" + to_string(exprBits) + "to" + to_string(targetBits);
				context.includeBuiltInFunction(fullName, boogie::Decl::function(
						fullName, {{"", boogieBVType(exprBits)}}, boogieBVType(targetBits), nullptr,
						{boogie::Attr::attr("bvbuiltin", "zero_extend " + to_string(targetBits - exprBits))}));
				return boogie::Expr::fn(fullName, expr);
			}
			else if (targetSigned) // Signed can only be converted to signed with sign extension
			{
				string fullName = "bvsignext" + to_string(exprBits) + "to" + to_string(targetBits);
				context.includeBuiltInFunction(fullName, boogie::Decl::function(
						fullName, {{"", boogieBVType(exprBits)}}, boogieBVType(targetBits), nullptr,
						{boogie::Attr::attr("bvbuiltin", "sign_extend " + to_string(targetBits - exprBits))}));
				return boogie::Expr::fn(fullName, expr);
			}
			else // Signed to unsigned should have alrady been detected by the compiler
			{
				BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Implicit conversion from signed to unsigned"));
				return nullptr;
			}
		}
	}

	return expr;
}

boogie::Expr::Ref ASTBoogieUtils::checkExplicitBvConversion(boogie::Expr::Ref expr, TypePointer exprType, TypePointer targetType, BoogieContext& context)
{
	// Do nothing if any of the types is unknown
	if (!targetType || !exprType) { return expr; }

	if (isBitPreciseType(targetType))
	{
		unsigned targetBits = getBits(targetType);
		// Literals can be handled by implicit conversion
		if (dynamic_pointer_cast<boogie::IntLit const>(expr))
		{
			return checkImplicitBvConversion(expr, exprType, targetType, context);
		}
		else if (isBitPreciseType(exprType))
		{
			unsigned exprBits = getBits(exprType);
			bool targetSigned = isSigned(targetType);
			bool exprSigned = isSigned(exprType);

			// Check if explicit conversion is really needed:
			// - converting to smaller size
			// - converting from signed to unsigned
			// - converting from unsinged to same size signed
			if (targetBits < exprBits || (exprSigned && !targetSigned) || (targetBits == exprBits && !exprSigned && targetSigned))
			{
				// Nothing to do for same size, since Boogie bitvectors do not have signs
				if (targetBits == exprBits) { return expr; }
				// For larger sizes, sign extension is done
				else if (targetBits > exprBits)
				{
					string fullName = "bvsignext" + to_string(exprBits) + "to" + to_string(targetBits);
					context.includeBuiltInFunction(fullName, boogie::Decl::function(
							fullName, {{"", boogieBVType(exprBits)}}, boogieBVType(targetBits), nullptr,
							{boogie::Attr::attr("bvbuiltin", "sign_extend " + to_string(targetBits - exprBits))}));
					return boogie::Expr::fn(fullName, expr);
				}
				// For smaller sizes, higher-order bits are discarded
				else
				{
					string fullName = "extract" + to_string(targetBits) + "from" + to_string(exprBits);
					context.includeBuiltInFunction(fullName, boogie::Decl::function(
							fullName, {{"", boogieBVType(exprBits)}}, boogieBVType(targetBits), nullptr,
							{boogie::Attr::attr("bvbuiltin", "(_ extract " + to_string(targetBits - 1) + " 0)")}));
					return boogie::Expr::fn(fullName, expr);
				}
			}
			// Otherwise the implicit will handle it
			else
			{
				return checkImplicitBvConversion(expr, exprType, targetType, context);
			}
		}
	}

	return expr;
}

boogie::Expr::Ref ASTBoogieUtils::getTCCforExpr(boogie::Expr::Ref expr, TypePointer tp)
{
	if (isBitPreciseType(tp))
	{
		unsigned bits = getBits(tp);
		if (isSigned(tp))
		{
			auto largestSigned = boogie::Expr::lit(boost::multiprecision::pow(boogie::bigint(2), bits - 1) - 1);
			auto smallestSigned = boogie::Expr::lit(-boost::multiprecision::pow(boogie::bigint(2), bits - 1));
			return boogie::Expr::and_(
					boogie::Expr::lte(smallestSigned, expr),
					boogie::Expr::lte(expr, largestSigned));
		}
		else
		{
			auto largestUnsigned = boogie::Expr::lit(boost::multiprecision::pow(boogie::bigint(2), bits) - 1);
			auto smallestUnsigned = boogie::Expr::lit(long(0));
			return boogie::Expr::and_(
					boogie::Expr::lte(smallestUnsigned, expr),
					boogie::Expr::lte(expr, largestUnsigned));
		}
	}
	return boogie::Expr::lit(true);
}

}
}
