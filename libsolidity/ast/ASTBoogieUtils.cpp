#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <liblangutil/SourceLocation.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace langutil;

namespace dev
{
namespace solidity
{
const string ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE = "address";
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

const string ASTBoogieUtils::SOLIDITY_MSG = "msg";
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
const string ASTBoogieUtils::ERR_TYPE = "__ERROR_UNSUPPORTED_TYPE";
const string ASTBoogieUtils::BOOGIE_ZERO_ADDRESS = "__zero__address";
const string ASTBoogieUtils::SOLIDITY_NOW = "now";
const string ASTBoogieUtils::BOOGIE_NOW = "__now";
const string ASTBoogieUtils::VERIFIER_OVERFLOW = "__verifier_overflow";

const string ASTBoogieUtils::ERR_EXPR = "__ERROR";

smack::ProcDecl* ASTBoogieUtils::createTransferProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value, amount
	list<smack::Binding> transferParams{
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_VALUE, context.isBvEncoding() ? "bv256" : "int"},
		{"amount", context.isBvEncoding() ? "bv256" : "int"}
	};

	// Body
	smack::Block* transferImpl = smack::Block::block();
	const smack::Expr* this_bal = smack::Expr::sel(BOOGIE_BALANCE, BOOGIE_THIS);
	const smack::Expr* sender_bal = smack::Expr::sel(BOOGIE_BALANCE, BOOGIE_MSG_SENDER);
	const smack::Expr* amount = smack::Expr::id("amount");

	// Precondition: there is enough ether to transfer
	auto geqResult = encodeArithBinaryOp(context, nullptr, langutil::Token::GreaterThanOrEqual, sender_bal, amount, 256, false);
	transferImpl->addStmt(smack::Stmt::assume(geqResult.first));
	// balance[this] += amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		transferImpl->addStmt(smack::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)));
		transferImpl->addStmt(smack::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, amount, 256, false);
	if (context.overflow())
	{
		transferImpl->addStmt(smack::Stmt::comment("Implicit assumption that balances cannot overflow"));
		transferImpl->addStmt(smack::Stmt::assume(addBalance.second));
	}
	transferImpl->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(smack::Expr::id(BOOGIE_BALANCE), smack::Expr::id(BOOGIE_THIS), addBalance.first)));
	// balance[msg.sender] -= amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		transferImpl->addStmt(smack::Stmt::assume(ASTBoogieUtils::getTCCforExpr(sender_bal, tp_uint256)));
		transferImpl->addStmt(smack::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto subSenderBalance = encodeArithBinaryOp(context, nullptr, Token::Sub, sender_bal, amount, 256, false);
	if (context.overflow())
	{
		transferImpl->addStmt(smack::Stmt::comment("Implicit assumption that balances cannot overflow"));
		transferImpl->addStmt(smack::Stmt::assume(subSenderBalance.second));
	}
	transferImpl->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(smack::Expr::id(BOOGIE_BALANCE), smack::Expr::id(BOOGIE_MSG_SENDER), subSenderBalance.first)));
	transferImpl->addStmt(smack::Stmt::comment("TODO: call fallback, exception handling"));

	smack::ProcDecl* transfer = smack::Decl::procedure(BOOGIE_TRANSFER, transferParams, {}, {}, {transferImpl});

	transfer->addAttr(smack::Attr::attr("inline", 1));
	transfer->addAttr(smack::Attr::attr("message", "transfer"));
	return transfer;
}

smack::ProcDecl* ASTBoogieUtils::createCallProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value
	list<smack::Binding> callParams {
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_VALUE, context.isBvEncoding() ? "bv256" : "int"}
	};

	// Return value
	list<smack::Binding> callReturns{ {"__result", "bool"} };

	// Body
	// Successful transfer
	smack::Block* thenBlock = smack::Block::block();
	const smack::Expr* this_bal = smack::Expr::sel(BOOGIE_BALANCE, BOOGIE_THIS);
	const smack::Expr* msg_val = smack::Expr::id(BOOGIE_MSG_VALUE);
	const smack::Expr* result = smack::Expr::id("__result");

	// balance[this] += msg.value
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		thenBlock->addStmt(smack::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)));
		thenBlock->addStmt(smack::Stmt::assume(ASTBoogieUtils::getTCCforExpr(msg_val, tp_uint256)));
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, msg_val, 256, false);
	if (context.overflow())
	{
		thenBlock->addStmt(smack::Stmt::comment("Implicit assumption that balances cannot overflow"));
		thenBlock->addStmt(smack::Stmt::assume(addBalance.second));
	}
	thenBlock->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(smack::Expr::id(BOOGIE_BALANCE), smack::Expr::id(BOOGIE_THIS), addBalance.first)));
	thenBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(true)));
	// Unsuccessful transfer
	smack::Block* elseBlock = smack::Block::block();
	elseBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(false)));
	// Nondeterministic choice between success and failure
	smack::Block* callBlock = smack::Block::block();
	callBlock->addStmt(smack::Stmt::comment("TODO: call fallback"));
	callBlock->addStmt(smack::Stmt::ifelse(smack::Expr::id("*"), thenBlock, elseBlock));

	smack::ProcDecl* callProc = smack::Decl::procedure(BOOGIE_CALL, callParams, callReturns, {}, {callBlock});
	callProc->addAttr(smack::Attr::attr("inline", 1));
	callProc->addAttr(smack::Attr::attr("message", "call"));
	return callProc;
}

smack::ProcDecl* ASTBoogieUtils::createSendProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value, amount
	list<smack::Binding> sendParams {
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{ASTBoogieUtils::BOOGIE_MSG_VALUE, context.isBvEncoding() ? "bv256" : "int"},
		{"amount", context.isBvEncoding() ? "bv256" : "int"}
	};

	// Return value
	list<smack::Binding> sendReturns{ {"__result", "bool"} };

	// Body
	// Successful transfer
	smack::Block* thenBlock = smack::Block::block();
	const smack::Expr* this_bal = smack::Expr::sel(BOOGIE_BALANCE, BOOGIE_THIS);
	const smack::Expr* sender_bal = smack::Expr::sel(BOOGIE_BALANCE, BOOGIE_MSG_SENDER);
	const smack::Expr* amount = smack::Expr::id("amount");
	const smack::Expr* result = smack::Expr::id("__result");

	// balance[this] += amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		thenBlock->addStmt(smack::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)));
		thenBlock->addStmt(smack::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, amount, 256, false);
	if (context.overflow())
	{
		thenBlock->addStmt(smack::Stmt::comment("Implicit assumption that balances cannot overflow"));
		thenBlock->addStmt(smack::Stmt::assume(addBalance.second));
	}
	thenBlock->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(smack::Expr::id(BOOGIE_BALANCE), smack::Expr::id(BOOGIE_THIS), addBalance.first)));
	// balance[msg.sender] -= amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
		thenBlock->addStmt(smack::Stmt::assume(ASTBoogieUtils::getTCCforExpr(sender_bal, tp_uint256)));
		thenBlock->addStmt(smack::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto subSenderBalance = encodeArithBinaryOp(context, nullptr, Token::Sub, sender_bal, amount, 256, false);
	if (context.overflow())
	{
		thenBlock->addStmt(smack::Stmt::comment("Implicit assumption that balances cannot overflow"));
		thenBlock->addStmt(smack::Stmt::assume(subSenderBalance.second));
	}
	thenBlock->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(smack::Expr::id(BOOGIE_BALANCE), smack::Expr::id(BOOGIE_MSG_SENDER), subSenderBalance.first)));
	thenBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(true)));
	// Unsuccessful transfer
	smack::Block* elseBlock = smack::Block::block();
	elseBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(false)));

	smack::Block* transferBlock = smack::Block::block();
	// Precondition: there is enough ether to transfer
	auto senderBalanceGEQ = encodeArithBinaryOp(context, nullptr, langutil::Token::GreaterThanOrEqual, sender_bal, amount, 256, false);
	transferBlock->addStmt(smack::Stmt::assume(senderBalanceGEQ.first));
	// Nondeterministic choice between success and failure
	transferBlock->addStmt(smack::Stmt::comment("TODO: call fallback"));
	transferBlock->addStmt(smack::Stmt::ifelse(smack::Expr::id("*"), thenBlock, elseBlock));

	smack::ProcDecl* sendProc = smack::Decl::procedure(BOOGIE_SEND, sendParams, sendReturns, {}, {transferBlock});

	sendProc->addAttr(smack::Attr::attr("inline", 1));
	sendProc->addAttr(smack::Attr::attr("message", "send"));
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

string ASTBoogieUtils::mapType(TypePointer tp, ASTNode const& _associatedNode, BoogieContext& context)
{
	string tpStr = tp->toString();
	if (tpStr == SOLIDITY_ADDRESS_TYPE) return BOOGIE_ADDRESS_TYPE;
	if (tpStr == "string storage ref") return BOOGIE_STRING_TYPE;
	if (tpStr == "string memory") return BOOGIE_STRING_TYPE;
	if (tpStr == "bool") return "bool";
	// For literals we return 'int_const' even in bit-precise mode, they will be
	// converted to appropriate type when assigned to something (or implicitly converted)
	if (boost::algorithm::starts_with(tpStr, "int_const ")) return "int_const";
	for (int i = 8; i <= 256; ++i)
	{
		if (tpStr == "int" + to_string(i)) return context.isBvEncoding() ? "bv" + to_string(i) : "int";
		if (tpStr == "uint" + to_string(i)) return context.isBvEncoding() ? "bv" + to_string(i) : "int";
	}
	if (boost::algorithm::starts_with(tpStr, "contract ")) return BOOGIE_ADDRESS_TYPE;

	if (tp->category() == Type::Category::Array)
	{
		auto arrType = dynamic_cast<ArrayType const*>(&*tp);
		return "[" + string(context.isBvEncoding() ? "bv256" : "int") + "]" + mapType(arrType->baseType(), _associatedNode, context);
	}

	if (tp->category() == Type::Category::Mapping)
	{
		auto mappingType = dynamic_cast<MappingType const*>(&*tp);
		return "[" + mapType(mappingType->keyType(), _associatedNode, context) + "]"
				+ mapType(mappingType->valueType(), _associatedNode, context);
	}

	if (tp->category() == Type::Category::FixedBytes)
	{
		auto fbType = dynamic_cast<FixedBytesType const*>(&*tp);
		if (context.isBvEncoding())
		{
			if (fbType->numBytes() == 1) return "bv8";
			else return "[bv256]bv8";
		}
		else
		{
			if (fbType->numBytes() == 1) return "int";
			else return "[int]int";
		}
	}

	// Unsupported types
	if (boost::algorithm::starts_with(tpStr, "tuple("))
	{
		context.reportError(&_associatedNode, "Tuples are not supported");
	}
	else
	{
		context.reportError(&_associatedNode, "Unsupported type: '" + tpStr.substr(0, tpStr.find(' ')) + "'");
	}
	return ERR_TYPE;
}

list<const smack::Attr*> ASTBoogieUtils::createAttrs(SourceLocation const& loc, std::string const& message, Scanner const& scanner)
{
	int srcLine, srcCol;
	tie(srcLine, srcCol) = scanner.translatePositionToLineColumn(loc.start);
	return {
		smack::Attr::attr("sourceloc", loc.source->name(), srcLine + 1, srcCol + 1),
		smack::Attr::attr("message", message)
	};
}

ASTBoogieUtils::expr_pair ASTBoogieUtils::encodeArithBinaryOp(BoogieContext& context, ASTNode const* associatedNode, langutil::Token op,
		 smack::Expr const* lhs, smack::Expr const* rhs, unsigned bits, bool isSigned)
{
	const smack::Expr* result = nullptr;
	const smack::Expr* ecc = nullptr;

	switch(context.encoding())
	{
	case BoogieContext::Encoding::INT:
		switch(op)
		{
		case Token::Add: result = smack::Expr::plus(lhs, rhs); break;
		case Token::Sub: result = smack::Expr::minus(lhs, rhs); break;
		case Token::Mul: result = smack::Expr::times(lhs, rhs); break;
		// TODO: returning integer division is fine, because Solidity does not support floats yet
		case Token::Div: result = smack::Expr::intdiv(lhs, rhs); break;
		case Token::Mod: result = smack::Expr::mod(lhs, rhs); break;

		case Token::Equal: result = smack::Expr::eq(lhs, rhs); break;
		case Token::NotEqual: result = smack::Expr::neq(lhs, rhs); break;
		case Token::LessThan: result = smack::Expr::lt(lhs, rhs); break;
		case Token::GreaterThan: result = smack::Expr::gt(lhs, rhs); break;
		case Token::LessThanOrEqual: result = smack::Expr::lte(lhs, rhs); break;
		case Token::GreaterThanOrEqual: result = smack::Expr::gte(lhs, rhs); break;

		case Token::Exp:
			if (auto rhsLit = dynamic_cast<smack::IntLit const *>(rhs))
			{
				if (auto lhsLit = dynamic_cast<smack::IntLit const *>(lhs))
				{
					result = smack::Expr::lit(boost::multiprecision::pow(lhsLit->getVal(), rhsLit->getVal().convert_to<unsigned>()));
				}
			}
			context.reportError(associatedNode, "Exponentiation is not supported in 'int' encoding");
			if (result == nullptr) {
				result = smack::Expr::id(ERR_EXPR);
			}
			break;
		default:
			context.reportError(associatedNode, string("Unsupported binary operator in 'int' encoding ") + TokenTraits::toString(op));
			result = smack::Expr::id(ERR_EXPR);
		}
		break;
	case BoogieContext::Encoding::BV:
		{
			string name;
			string retType;

			switch (op) {
			case Token::Add: name = "add"; retType = "bv" + to_string(bits); break;
			case Token::Sub: name = "sub"; retType = "bv" + to_string(bits); break;
			case Token::Mul: name = "mul"; retType = "bv" + to_string(bits); break;
			case Token::Div: name = isSigned ? "sdiv" : "udiv"; retType = "bv" + to_string(bits); break;

			case Token::BitAnd: name = "and"; retType = "bv" + to_string(bits); break;
			case Token::BitOr: name = "or"; retType = "bv" + to_string(bits); break;
			case Token::BitXor: name = "xor"; retType = "bv" + to_string(bits); break;
			case Token::SAR: name = isSigned ? "ashr" : "lshr"; retType = "bv" + to_string(bits); break;
			case Token::SHL: name = "shl"; retType = "bv" + to_string(bits); break;

			case Token::Equal: result = smack::Expr::eq(lhs, rhs); break;
			case Token::NotEqual: result = smack::Expr::neq(lhs, rhs); break;

			case Token::LessThan: name = isSigned ? "slt" : "ult"; retType = "bool"; break;
			case Token::GreaterThan: name = isSigned ? "sgt" : "ugt"; retType = "bool";  break;
			case Token::LessThanOrEqual: name = isSigned ? "sle" : "ule"; retType = "bool";  break;
			case Token::GreaterThanOrEqual: name = isSigned ? "sge" : "uge"; retType = "bool";  break;

			default:
				context.reportError(associatedNode, string("Unsupported binary operator in 'bv' encoding ") + TokenTraits::toString(op));
				result = smack::Expr::id(ERR_EXPR);
			}
			if (result == nullptr) { // not computd yet, no error
				string fullName = "bv" + to_string(bits) + name;
				context.includeBuiltInFunction(fullName, smack::Decl::function(
							fullName, {{"", "bv"+to_string(bits)}, {"", "bv"+to_string(bits)}}, retType, nullptr,
							{smack::Attr::attr("bvbuiltin", "bv" + name)}));
				result = smack::Expr::fn(fullName, lhs, rhs);
			}
		}
		break;
	case BoogieContext::Encoding::MOD:
		{
			auto modulo = smack::Expr::lit(boost::multiprecision::pow(smack::bigint(2), bits));
			auto largestSigned = smack::Expr::lit(boost::multiprecision::pow(smack::bigint(2), bits - 1) - 1);
			auto smallestSigned = smack::Expr::lit(-boost::multiprecision::pow(smack::bigint(2), bits - 1));
			switch(op)
			{
			case Token::Add:
			{
				auto sum = smack::Expr::plus(lhs, rhs);
				if (isSigned)
				{
					result = smack::Expr::cond(smack::Expr::gt(sum, largestSigned),
						smack::Expr::minus(sum, modulo),
						smack::Expr::cond(smack::Expr::lt(sum, smallestSigned), smack::Expr::plus(sum, modulo), sum));
				}
				else
				{
					result = smack::Expr::cond(smack::Expr::gte(sum, modulo), smack::Expr::minus(sum, modulo), sum);
				}
				ecc = smack::Expr::eq(sum, result);
				break;
			}
			case Token::Sub:
			{
				auto diff = smack::Expr::minus(lhs, rhs);
				if (isSigned)
				{
					result = smack::Expr::cond(smack::Expr::gt(diff, largestSigned),
						smack::Expr::minus(diff, modulo),
						smack::Expr::cond(smack::Expr::lt(diff, smallestSigned), smack::Expr::plus(diff, modulo), diff));
				}
				else
				{
					result = smack::Expr::cond(smack::Expr::gte(lhs, rhs), diff, smack::Expr::plus(diff, modulo));
				}
				ecc = smack::Expr::eq(diff, result);
				break;
			}
			case Token::Mul:
			{
				auto prod = smack::Expr::times(lhs, rhs);
				if (isSigned)
				{
					auto lhs1 = smack::Expr::cond(smack::Expr::gte(lhs, smack::Expr::lit((long)0)), lhs, smack::Expr::plus(modulo, lhs));
					auto rhs1 = smack::Expr::cond(smack::Expr::gte(rhs, smack::Expr::lit((long)0)), rhs, smack::Expr::plus(modulo, rhs));
					auto prod = smack::Expr::mod(smack::Expr::times(lhs1, rhs1), modulo);
					result = smack::Expr::cond(smack::Expr::gt(prod, largestSigned), smack::Expr::minus(prod, modulo), prod);
				}
				else
				{
					result = smack::Expr::cond(smack::Expr::gte(prod, modulo), smack::Expr::mod(prod, modulo), prod);
				}
				ecc = smack::Expr::eq(prod, result);
				break;
			}
			case Token::Div:
			{
				auto div = smack::Expr::intdiv(lhs, rhs);
				if (isSigned)
				{
					result = smack::Expr::cond(smack::Expr::gt(div, largestSigned),
						smack::Expr::minus(div, modulo),
						smack::Expr::cond(smack::Expr::lt(div, smallestSigned), smack::Expr::plus(div, modulo), div));
				}
				else
				{
					result = div;
				}
				ecc = smack::Expr::eq(div, result);
				break;
			}

			case Token::Equal:
				result = smack::Expr::eq(lhs, rhs);
				break;
			case Token::NotEqual:
				result = smack::Expr::neq(lhs, rhs);
				break;
			case Token::LessThan:
				result = smack::Expr::lt(lhs, rhs);
				break;
			case Token::GreaterThan:
				result = smack::Expr::gt(lhs, rhs);
				break;
			case Token::LessThanOrEqual:
				result = smack::Expr::lte(lhs, rhs);
				break;
			case Token::GreaterThanOrEqual:
				result = smack::Expr::gte(lhs, rhs);
				break;
			default:
				context.reportError(associatedNode, string("Unsupported binary operator in 'mod' encoding ") + TokenTraits::toString(op));
				result = smack::Expr::id(ERR_EXPR);
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
		smack::Expr const* subExpr, unsigned bits, bool isSigned)
{
	const smack::Expr* result = nullptr;
	const smack::Expr* ecc = nullptr;

	switch(context.encoding())
	{
	case BoogieContext::Encoding::INT:
		switch(op)
		{
		case Token::Add: result = subExpr; break; // Unary plus does not do anything
		case Token::Sub: result = smack::Expr::neg(subExpr); break;
		default:
			context.reportError(associatedNode, string("Unsupported unary operator in 'int' encoding ") + TokenTraits::toString(op));
			result = smack::Expr::id(ERR_EXPR);
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
				result = smack::Expr::id(ERR_EXPR);
				break;
			}
			if (!result)
			{
				string fullName = "bv" + to_string(bits) + name;
				context.includeBuiltInFunction(fullName, smack::Decl::function(
								fullName, {{"", "bv"+to_string(bits)}}, "bv"+to_string(bits), nullptr,
								{smack::Attr::attr("bvbuiltin", "bv" + name)}));

				result = smack::Expr::fn(fullName, subExpr);
			}
		}
		break;

	case BoogieContext::Encoding::MOD:
		switch(op)
		{
		case Token::Add: result = subExpr; break; // Unary plus does not do anything
		case Token::Sub:
		{
			auto sub = smack::Expr::neg(subExpr);
			if (isSigned)
			{
				auto smallestSigned = smack::Expr::lit(-boost::multiprecision::pow(smack::bigint(2), bits - 1));
				result = smack::Expr::cond(smack::Expr::eq(subExpr, smallestSigned),
						smallestSigned,
						sub);
			}
			else
			{
				auto modulo = smack::Expr::lit(boost::multiprecision::pow(smack::bigint(2), bits));
				result = smack::Expr::cond(smack::Expr::eq(subExpr, smack::Expr::lit((long)0)),
						smack::Expr::lit((long)0),
						smack::Expr::minus(modulo, subExpr));
			}
			ecc = smack::Expr::eq(sub, result);
			break;
		}
		default:
			context.reportError(associatedNode, string("Unsupported unary operator in 'mod' encoding ") + TokenTraits::toString(op));
			result = smack::Expr::id(ERR_EXPR);
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

smack::Expr const* ASTBoogieUtils::checkImplicitBvConversion(smack::Expr const* expr, TypePointer exprType, TypePointer targetType, BoogieContext& context)
{
	// Do nothing if any of the types is unknown
	if (!targetType || !exprType) { return expr; }

	if (isBitPreciseType(targetType))
	{
		unsigned targetBits = getBits(targetType);
		// Create bitvector from literals
		if (auto exprLit = dynamic_cast<smack::IntLit const*>(expr))
		{
			if (exprLit->getVal() < 0) // Negative literals are tricky
			{
				string fullName = "bv" + to_string(targetBits) + "neg";
				context.includeBuiltInFunction(fullName, smack::Decl::function(
								fullName, {{"", "bv"+to_string(targetBits)}}, "bv"+to_string(targetBits), nullptr,
								{smack::Attr::attr("bvbuiltin", "bvneg")}));
				return smack::Expr::fn(fullName, smack::Expr::lit(-exprLit->getVal(), targetBits));
			}
			else
			{
				return smack::Expr::lit(exprLit->getVal(), targetBits);
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
				context.includeBuiltInFunction(fullName, smack::Decl::function(
						fullName, {{"", "bv"+to_string(exprBits)}}, "bv"+to_string(targetBits), nullptr,
						{smack::Attr::attr("bvbuiltin", "zero_extend " + to_string(targetBits - exprBits))}));
				return smack::Expr::fn(fullName, expr);
			}
			else if (targetSigned) // Signed can only be converted to signed with sign extension
			{
				string fullName = "bvsignext" + to_string(exprBits) + "to" + to_string(targetBits);
				context.includeBuiltInFunction(fullName, smack::Decl::function(
						fullName, {{"", "bv"+to_string(exprBits)}}, "bv"+to_string(targetBits), nullptr,
						{smack::Attr::attr("bvbuiltin", "sign_extend " + to_string(targetBits - exprBits))}));
				return smack::Expr::fn(fullName, expr);
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

smack::Expr const* ASTBoogieUtils::checkExplicitBvConversion(smack::Expr const* expr, TypePointer exprType, TypePointer targetType, BoogieContext& context)
{
	// Do nothing if any of the types is unknown
	if (!targetType || !exprType) { return expr; }

	if (isBitPreciseType(targetType))
	{
		unsigned targetBits = getBits(targetType);
		// Literals can be handled by implicit conversion
		if (dynamic_cast<smack::IntLit const*>(expr))
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
					context.includeBuiltInFunction(fullName, smack::Decl::function(
							fullName, {{"", "bv"+to_string(exprBits)}}, "bv"+to_string(targetBits), nullptr,
							{smack::Attr::attr("bvbuiltin", "sign_extend " + to_string(targetBits - exprBits))}));
					return smack::Expr::fn(fullName, expr);
				}
				// For smaller sizes, higher-order bits are discarded
				else
				{
					string fullName = "extract" + to_string(targetBits) + "from" + to_string(exprBits);
					context.includeBuiltInFunction(fullName, smack::Decl::function(
							fullName, {{"", "bv"+to_string(exprBits)}}, "bv"+to_string(targetBits), nullptr,
							{smack::Attr::attr("bvbuiltin", "(_ extract " + to_string(targetBits - 1) + " 0)")}));
					return smack::Expr::fn(fullName, expr);
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

smack::Expr const* ASTBoogieUtils::getTCCforExpr(smack::Expr const* expr, TypePointer tp)
{
	if (isBitPreciseType(tp))
	{
		unsigned bits = getBits(tp);
		if (isSigned(tp))
		{
			auto largestSigned = smack::Expr::lit(boost::multiprecision::pow(smack::bigint(2), bits - 1) - 1);
			auto smallestSigned = smack::Expr::lit(-boost::multiprecision::pow(smack::bigint(2), bits - 1));
			return smack::Expr::and_(
					smack::Expr::lte(smallestSigned, expr),
					smack::Expr::lte(expr, largestSigned));
		}
		else
		{
			auto largestUnsigned = smack::Expr::lit(boost::multiprecision::pow(smack::bigint(2), bits) - 1);
			auto smallestUnsigned = smack::Expr::lit(long(0));
			return smack::Expr::and_(
					smack::Expr::lte(smallestUnsigned, expr),
					smack::Expr::lte(expr, largestUnsigned));
		}
	}
	return smack::Expr::lit(true);
}

}
}
