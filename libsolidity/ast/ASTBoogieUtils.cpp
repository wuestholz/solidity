#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <liblangutil/SourceLocation.h>
#include <libsolidity/ast/Types.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace langutil;

namespace bg = boogie;

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

bg::ProcDeclRef ASTBoogieUtils::createTransferProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value, amount
	list<bg::Binding> transferParams{
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_VALUE, context.intType(256) },
		{"amount", context.intType(256) }
	};

	// Body
	bg::Block::Ref transferImpl = bg::Block::block();
	bg::Expr::Ref this_bal = bg::Expr::sel(BOOGIE_BALANCE, BOOGIE_THIS);
	bg::Expr::Ref sender_bal = bg::Expr::sel(BOOGIE_BALANCE, BOOGIE_MSG_SENDER);
	bg::Expr::Ref amount = bg::Expr::id("amount");

	// Precondition: there is enough ether to transfer
	auto geqResult = encodeArithBinaryOp(context, nullptr, langutil::Token::GreaterThanOrEqual, sender_bal, amount, 256, false);
	transferImpl->addStmt(bg::Stmt::assume(geqResult.first));
	// balance[this] += amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		transferImpl->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)));
		transferImpl->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, amount, 256, false);
	if (context.overflow())
	{
		transferImpl->addStmt(bg::Stmt::comment("Implicit assumption that balances cannot overflow"));
		transferImpl->addStmt(bg::Stmt::assume(addBalance.second));
	}
	transferImpl->addStmt(bg::Stmt::assign(
			bg::Expr::id(BOOGIE_BALANCE),
			bg::Expr::upd(bg::Expr::id(BOOGIE_BALANCE), bg::Expr::id(BOOGIE_THIS), addBalance.first)));
	// balance[msg.sender] -= amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		transferImpl->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(sender_bal, tp_uint256)));
		transferImpl->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto subSenderBalance = encodeArithBinaryOp(context, nullptr, Token::Sub, sender_bal, amount, 256, false);
	if (context.overflow())
	{
		transferImpl->addStmt(bg::Stmt::comment("Implicit assumption that balances cannot overflow"));
		transferImpl->addStmt(bg::Stmt::assume(subSenderBalance.second));
	}
	transferImpl->addStmt(bg::Stmt::assign(
			bg::Expr::id(BOOGIE_BALANCE),
			bg::Expr::upd(bg::Expr::id(BOOGIE_BALANCE), bg::Expr::id(BOOGIE_MSG_SENDER), subSenderBalance.first)));
	transferImpl->addStmt(bg::Stmt::comment("TODO: call fallback, exception handling"));

	bg::ProcDeclRef transfer = bg::Decl::procedure(BOOGIE_TRANSFER, transferParams, {}, {}, {transferImpl});

	transfer->addAttr(bg::Attr::attr("inline", 1));
	transfer->addAttr(bg::Attr::attr("message", "transfer"));
	return transfer;
}

bg::ProcDeclRef ASTBoogieUtils::createCallProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value
	list<bg::Binding> callParams {
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_VALUE, context.intType(256) }
	};

	// Return value
	list<bg::Binding> callReturns{ {"__result", "bool"} };

	// Body
	// Successful transfer
	bg::Block::Ref thenBlock = bg::Block::block();
	bg::Expr::Ref this_bal = bg::Expr::sel(BOOGIE_BALANCE, BOOGIE_THIS);
	bg::Expr::Ref msg_val = bg::Expr::id(BOOGIE_MSG_VALUE);
	bg::Expr::Ref result = bg::Expr::id("__result");

	// balance[this] += msg.value
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		thenBlock->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)));
		thenBlock->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(msg_val, tp_uint256)));
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, msg_val, 256, false);
	if (context.overflow())
	{
		thenBlock->addStmt(bg::Stmt::comment("Implicit assumption that balances cannot overflow"));
		thenBlock->addStmt(bg::Stmt::assume(addBalance.second));
	}
	thenBlock->addStmt(bg::Stmt::assign(
			bg::Expr::id(BOOGIE_BALANCE),
			bg::Expr::upd(bg::Expr::id(BOOGIE_BALANCE), bg::Expr::id(BOOGIE_THIS), addBalance.first)));
	thenBlock->addStmt(bg::Stmt::assign(result, bg::Expr::lit(true)));
	// Unsuccessful transfer
	bg::Block::Ref elseBlock = bg::Block::block();
	elseBlock->addStmt(bg::Stmt::assign(result, bg::Expr::lit(false)));
	// Nondeterministic choice between success and failure
	bg::Block::Ref callBlock = bg::Block::block();
	callBlock->addStmt(bg::Stmt::comment("TODO: call fallback"));
	callBlock->addStmt(bg::Stmt::ifelse(bg::Expr::id("*"), thenBlock, elseBlock));

	bg::ProcDeclRef callProc = bg::Decl::procedure(BOOGIE_CALL, callParams, callReturns, {}, {callBlock});
	callProc->addAttr(bg::Attr::attr("inline", 1));
	callProc->addAttr(bg::Attr::attr("message", "call"));
	return callProc;
}

bg::ProcDeclRef ASTBoogieUtils::createSendProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value, amount
	list<bg::Binding> sendParams {
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{ASTBoogieUtils::BOOGIE_MSG_VALUE, context.intType(256) },
		{"amount", context.intType(256) }
	};

	// Return value
	list<bg::Binding> sendReturns{ {"__result", "bool"} };

	// Body
	// Successful transfer
	bg::Block::Ref thenBlock = bg::Block::block();
	bg::Expr::Ref this_bal = bg::Expr::sel(BOOGIE_BALANCE, BOOGIE_THIS);
	bg::Expr::Ref sender_bal = bg::Expr::sel(BOOGIE_BALANCE, BOOGIE_MSG_SENDER);
	bg::Expr::Ref amount = bg::Expr::id("amount");
	bg::Expr::Ref result = bg::Expr::id("__result");

	// balance[this] += amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		thenBlock->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)));
		thenBlock->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, amount, 256, false);
	if (context.overflow())
	{
		thenBlock->addStmt(bg::Stmt::comment("Implicit assumption that balances cannot overflow"));
		thenBlock->addStmt(bg::Stmt::assume(addBalance.second));
	}
	thenBlock->addStmt(bg::Stmt::assign(
			bg::Expr::id(BOOGIE_BALANCE),
			bg::Expr::upd(bg::Expr::id(BOOGIE_BALANCE), bg::Expr::id(BOOGIE_THIS), addBalance.first)));
	// balance[msg.sender] -= amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		thenBlock->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(sender_bal, tp_uint256)));
		thenBlock->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto subSenderBalance = encodeArithBinaryOp(context, nullptr, Token::Sub, sender_bal, amount, 256, false);
	if (context.overflow())
	{
		thenBlock->addStmt(bg::Stmt::comment("Implicit assumption that balances cannot overflow"));
		thenBlock->addStmt(bg::Stmt::assume(subSenderBalance.second));
	}
	thenBlock->addStmt(bg::Stmt::assign(
			bg::Expr::id(BOOGIE_BALANCE),
			bg::Expr::upd(bg::Expr::id(BOOGIE_BALANCE), bg::Expr::id(BOOGIE_MSG_SENDER), subSenderBalance.first)));
	thenBlock->addStmt(bg::Stmt::assign(result, bg::Expr::lit(true)));
	// Unsuccessful transfer
	bg::Block::Ref elseBlock = bg::Block::block();
	elseBlock->addStmt(bg::Stmt::assign(result, bg::Expr::lit(false)));

	bg::Block::Ref transferBlock = bg::Block::block();
	// Precondition: there is enough ether to transfer
	auto senderBalanceGEQ = encodeArithBinaryOp(context, nullptr, langutil::Token::GreaterThanOrEqual, sender_bal, amount, 256, false);
	transferBlock->addStmt(bg::Stmt::assume(senderBalanceGEQ.first));
	// Nondeterministic choice between success and failure
	transferBlock->addStmt(bg::Stmt::comment("TODO: call fallback"));
	transferBlock->addStmt(bg::Stmt::ifelse(bg::Expr::id("*"), thenBlock, elseBlock));

	bg::ProcDeclRef sendProc = bg::Decl::procedure(BOOGIE_SEND, sendParams, sendReturns, {}, {transferBlock});

	sendProc->addAttr(bg::Attr::attr("inline", 1));
	sendProc->addAttr(bg::Attr::attr("message", "send"));
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
		return context.intType(tpInteger->numBits());
	}
	case Type::Category::Contract:
		return BOOGIE_ADDRESS_TYPE;
	case Type::Category::Array: {
		auto arrType = dynamic_cast<ArrayType const*>(&*tp);
		if (arrType->isString()) {
			return BOOGIE_STRING_TYPE;
		} else {
			return "[" + context.intType(256) + "]" + mapType(arrType->baseType(), _associatedNode, context);
		}
	}
	case Type::Category::Mapping: {
		auto mappingType = dynamic_cast<MappingType const*>(&*tp);
		return "[" + mapType(mappingType->keyType(), _associatedNode, context) + "]"
				+ mapType(mappingType->valueType(), _associatedNode, context);
	}
	case Type::Category::FixedBytes: {
		// up to 32 bytes (use integer and slice it up)
		auto fbType = dynamic_cast<FixedBytesType const*>(&*tp);
		return context.intType(fbType->numBytes() * 8);
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

list<bg::Attr::Ref> ASTBoogieUtils::createAttrs(SourceLocation const& loc, std::string const& message, Scanner const& scanner)
{
	int srcLine, srcCol;
	tie(srcLine, srcCol) = scanner.translatePositionToLineColumn(loc.start);
	return {
		bg::Attr::attr("sourceloc", loc.source->name(), srcLine + 1, srcCol + 1),
		bg::Attr::attr("message", message)
	};
}

ASTBoogieUtils::expr_pair ASTBoogieUtils::encodeArithBinaryOp(BoogieContext& context, ASTNode const* associatedNode, langutil::Token op,
		 bg::Expr::Ref lhs, bg::Expr::Ref rhs, unsigned bits, bool isSigned)
{
	bg::Expr::Ref result = nullptr;
	bg::Expr::Ref ecc = nullptr;

	switch(context.encoding())
	{
	case BoogieContext::Encoding::INT:
		switch(op)
		{
		case Token::Add: result = bg::Expr::plus(lhs, rhs); break;
		case Token::Sub: result = bg::Expr::minus(lhs, rhs); break;
		case Token::Mul: result = bg::Expr::times(lhs, rhs); break;
		// TODO: returning integer division is fine, because Solidity does not support floats yet
		case Token::Div: result = bg::Expr::intdiv(lhs, rhs); break;
		case Token::Mod: result = bg::Expr::mod(lhs, rhs); break;

		case Token::Equal: result = bg::Expr::eq(lhs, rhs); break;
		case Token::NotEqual: result = bg::Expr::neq(lhs, rhs); break;
		case Token::LessThan: result = bg::Expr::lt(lhs, rhs); break;
		case Token::GreaterThan: result = bg::Expr::gt(lhs, rhs); break;
		case Token::LessThanOrEqual: result = bg::Expr::lte(lhs, rhs); break;
		case Token::GreaterThanOrEqual: result = bg::Expr::gte(lhs, rhs); break;

		case Token::Exp:
			if (auto rhsLit = dynamic_pointer_cast<bg::IntLit const>(rhs))
			{
				if (auto lhsLit = dynamic_pointer_cast<bg::IntLit const>(lhs))
				{
					result = bg::Expr::lit(boost::multiprecision::pow(lhsLit->getVal(), rhsLit->getVal().convert_to<unsigned>()));
				}
			}
			context.reportError(associatedNode, "Exponentiation is not supported in 'int' encoding");
			if (result == nullptr) {
				result = bg::Expr::id(ERR_EXPR);
			}
			break;
		default:
			context.reportError(associatedNode, string("Unsupported binary operator in 'int' encoding ") + TokenTraits::toString(op));
			result = bg::Expr::id(ERR_EXPR);
		}
		break;
	case BoogieContext::Encoding::BV:
		{
			string name;
			string retType;

			switch (op) {
			case Token::Add: result = context.bvAdd(bits, lhs, rhs); break;
			case Token::Sub: result = context.bvSub(bits, lhs, rhs); break;
			case Token::Mul: result = context.bvMul(bits, lhs, rhs); break;
			case Token::Div:
				if (isSigned) { result = context.bvSDiv(bits, lhs, rhs); }
				else { result = context.bvUDiv(bits, lhs, rhs); }
				break;
			case Token::BitAnd: result = context.bvAnd(bits, lhs, rhs); break;
			case Token::BitOr: result = context.bvOr(bits, lhs, rhs); break;
			case Token::BitXor: result = context.bvXor(bits, lhs, rhs); break;
			case Token::SAR:
				if (isSigned) { result = context.bvAShr(bits, lhs, rhs); }
				else { result = context.bvLShr(bits, lhs, rhs); }
				break;
			case Token::SHL: result = context.bvShl(bits, lhs, rhs); break;
			case Token::Equal: result = bg::Expr::eq(lhs, rhs); break;
			case Token::NotEqual: result = bg::Expr::neq(lhs, rhs); break;
			case Token::LessThan:
				if (isSigned) { result = context.bvSlt(bits, lhs, rhs); }
				else { result = context.bvUlt(bits, lhs, rhs); }
				break;
			case Token::GreaterThan:
				if (isSigned) { result = context.bvSgt(bits, lhs, rhs); }
				else { result = context.bvUgt(bits, lhs, rhs); }
				break;
			case Token::LessThanOrEqual:
				if (isSigned) { result = context.bvSle(bits, lhs, rhs); }
				else { result = context.bvUle(bits, lhs, rhs); }
				break;
			case Token::GreaterThanOrEqual:
				if (isSigned) { result = context.bvSge(bits, lhs, rhs); }
				else { result = context.bvUge(bits, lhs, rhs); }
				break;
			default:
				context.reportError(associatedNode, string("Unsupported binary operator in 'bv' encoding ") + TokenTraits::toString(op));
				result = bg::Expr::id(ERR_EXPR);
			}
		}
		break;
	case BoogieContext::Encoding::MOD:
		{
			auto modulo = bg::Expr::lit(boost::multiprecision::pow(bg::bigint(2), bits));
			auto largestSigned = bg::Expr::lit(boost::multiprecision::pow(bg::bigint(2), bits - 1) - 1);
			auto smallestSigned = bg::Expr::lit(-boost::multiprecision::pow(bg::bigint(2), bits - 1));
			switch(op)
			{
			case Token::Add:
			{
				auto sum = bg::Expr::plus(lhs, rhs);
				if (isSigned)
				{
					result = bg::Expr::cond(bg::Expr::gt(sum, largestSigned),
						bg::Expr::minus(sum, modulo),
						bg::Expr::cond(bg::Expr::lt(sum, smallestSigned), bg::Expr::plus(sum, modulo), sum));
				}
				else
				{
					result = bg::Expr::cond(bg::Expr::gte(sum, modulo), bg::Expr::minus(sum, modulo), sum);
				}
				ecc = bg::Expr::eq(sum, result);
				break;
			}
			case Token::Sub:
			{
				auto diff = bg::Expr::minus(lhs, rhs);
				if (isSigned)
				{
					result = bg::Expr::cond(bg::Expr::gt(diff, largestSigned),
						bg::Expr::minus(diff, modulo),
						bg::Expr::cond(bg::Expr::lt(diff, smallestSigned), bg::Expr::plus(diff, modulo), diff));
				}
				else
				{
					result = bg::Expr::cond(bg::Expr::gte(lhs, rhs), diff, bg::Expr::plus(diff, modulo));
				}
				ecc = bg::Expr::eq(diff, result);
				break;
			}
			case Token::Mul:
			{
				auto prod = bg::Expr::times(lhs, rhs);
				if (isSigned)
				{
					auto lhs1 = bg::Expr::cond(bg::Expr::gte(lhs, bg::Expr::lit((long)0)), lhs, bg::Expr::plus(modulo, lhs));
					auto rhs1 = bg::Expr::cond(bg::Expr::gte(rhs, bg::Expr::lit((long)0)), rhs, bg::Expr::plus(modulo, rhs));
					auto prod = bg::Expr::mod(bg::Expr::times(lhs1, rhs1), modulo);
					result = bg::Expr::cond(bg::Expr::gt(prod, largestSigned), bg::Expr::minus(prod, modulo), prod);
				}
				else
				{
					result = bg::Expr::cond(bg::Expr::gte(prod, modulo), bg::Expr::mod(prod, modulo), prod);
				}
				ecc = bg::Expr::eq(prod, result);
				break;
			}
			case Token::Div:
			{
				auto div = bg::Expr::intdiv(lhs, rhs);
				if (isSigned)
				{
					result = bg::Expr::cond(bg::Expr::gt(div, largestSigned),
						bg::Expr::minus(div, modulo),
						bg::Expr::cond(bg::Expr::lt(div, smallestSigned), bg::Expr::plus(div, modulo), div));
				}
				else
				{
					result = div;
				}
				ecc = bg::Expr::eq(div, result);
				break;
			}

			case Token::Equal:
				result = bg::Expr::eq(lhs, rhs);
				break;
			case Token::NotEqual:
				result = bg::Expr::neq(lhs, rhs);
				break;
			case Token::LessThan:
				result = bg::Expr::lt(lhs, rhs);
				break;
			case Token::GreaterThan:
				result = bg::Expr::gt(lhs, rhs);
				break;
			case Token::LessThanOrEqual:
				result = bg::Expr::lte(lhs, rhs);
				break;
			case Token::GreaterThanOrEqual:
				result = bg::Expr::gte(lhs, rhs);
				break;
			default:
				context.reportError(associatedNode, string("Unsupported binary operator in 'mod' encoding ") + TokenTraits::toString(op));
				result = bg::Expr::id(ERR_EXPR);
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
		bg::Expr::Ref subExpr, unsigned bits, bool isSigned)
{
	bg::Expr::Ref result = nullptr;
	bg::Expr::Ref ecc = nullptr;

	switch(context.encoding())
	{
	case BoogieContext::Encoding::INT:
		switch(op)
		{
		case Token::Add: result = subExpr; break; // Unary plus does not do anything
		case Token::Sub: result = bg::Expr::neg(subExpr); break;
		default:
			context.reportError(associatedNode, string("Unsupported unary operator in 'int' encoding ") + TokenTraits::toString(op));
			result = bg::Expr::id(ERR_EXPR);
			break;
		}
		break;

	case BoogieContext::Encoding::BV:
		{
			string name("");
			switch (op)
			{
			case Token::Add: result = subExpr; break; // Unary plus does not do anything
			case Token::Sub: result = context.bvNeg(bits, subExpr); break;
			case Token::BitNot: result = context.bvNot(bits, subExpr); break;
			default:
				context.reportError(associatedNode, string("Unsupported unary operator in 'bv' encoding ") + TokenTraits::toString(op));
				result = bg::Expr::id(ERR_EXPR);
				break;
			}
		}
		break;

	case BoogieContext::Encoding::MOD:
		switch(op)
		{
		case Token::Add: result = subExpr; break; // Unary plus does not do anything
		case Token::Sub:
		{
			auto sub = bg::Expr::neg(subExpr);
			if (isSigned)
			{
				auto smallestSigned = bg::Expr::lit(-boost::multiprecision::pow(bg::bigint(2), bits - 1));
				result = bg::Expr::cond(bg::Expr::eq(subExpr, smallestSigned),
						smallestSigned,
						sub);
			}
			else
			{
				auto modulo = bg::Expr::lit(boost::multiprecision::pow(bg::bigint(2), bits));
				result = bg::Expr::cond(bg::Expr::eq(subExpr, bg::Expr::lit((long)0)),
						bg::Expr::lit((long)0),
						bg::Expr::minus(modulo, subExpr));
			}
			ecc = bg::Expr::eq(sub, result);
			break;
		}
		default:
			context.reportError(associatedNode, string("Unsupported unary operator in 'mod' encoding ") + TokenTraits::toString(op));
			result = bg::Expr::id(ERR_EXPR);
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

bg::Expr::Ref ASTBoogieUtils::checkImplicitBvConversion(bg::Expr::Ref expr, TypePointer exprType, TypePointer targetType, BoogieContext& context)
{
	// Do nothing if any of the types is unknown
	if (!targetType || !exprType) { return expr; }

	if (isBitPreciseType(targetType))
	{
		unsigned targetBits = getBits(targetType);
		// Create bitvector from literals
		if (auto exprLit = dynamic_pointer_cast<bg::IntLit const>(expr))
		{
			if (exprLit->getVal() < 0) // Negative literals are tricky
				return context.bvNeg(targetBits, bg::Expr::lit(-exprLit->getVal(), targetBits));
			else
				return bg::Expr::lit(exprLit->getVal(), targetBits);
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
				return context.bvZeroExt(expr, exprBits, targetBits);
			else if (targetSigned) // Signed can only be converted to signed with sign extension
				return context.bvSignExt(expr, exprBits, targetBits);
			else // Signed to unsigned should have already been detected by the compiler
			{
				BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Implicit conversion from signed to unsigned"));
				return nullptr;
			}
		}
	}

	return expr;
}

bg::Expr::Ref ASTBoogieUtils::checkExplicitBvConversion(bg::Expr::Ref expr, TypePointer exprType, TypePointer targetType, BoogieContext& context)
{
	// Do nothing if any of the types is unknown
	if (!targetType || !exprType) { return expr; }

	if (isBitPreciseType(targetType))
	{
		unsigned targetBits = getBits(targetType);
		// Literals can be handled by implicit conversion
		if (dynamic_pointer_cast<bg::IntLit const>(expr))
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
				if (targetBits == exprBits)
					return expr;
				// For larger sizes, sign extension is done
				else if (targetBits > exprBits)
					return context.bvSignExt(expr, exprBits, targetBits);
				// For smaller sizes, higher-order bits are discarded
				else
					return context.bvExtract(expr, exprBits,targetBits-1, 0);
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

bg::Expr::Ref ASTBoogieUtils::getTCCforExpr(bg::Expr::Ref expr, TypePointer tp)
{
	if (isBitPreciseType(tp))
	{
		unsigned bits = getBits(tp);
		if (isSigned(tp))
		{
			auto largestSigned = bg::Expr::lit(boost::multiprecision::pow(bg::bigint(2), bits - 1) - 1);
			auto smallestSigned = bg::Expr::lit(-boost::multiprecision::pow(bg::bigint(2), bits - 1));
			return bg::Expr::and_(
					bg::Expr::lte(smallestSigned, expr),
					bg::Expr::lte(expr, largestSigned));
		}
		else
		{
			auto largestUnsigned = bg::Expr::lit(boost::multiprecision::pow(bg::bigint(2), bits) - 1);
			auto smallestUnsigned = bg::Expr::lit(long(0));
			return bg::Expr::and_(
					bg::Expr::lte(smallestUnsigned, expr),
					bg::Expr::lte(expr, largestUnsigned));
		}
	}
	return bg::Expr::lit(true);
}

}
}
