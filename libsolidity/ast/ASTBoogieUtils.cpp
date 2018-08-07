#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>
#include <libsolidity/ast/ASTBoogieUtils.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

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

const string ASTBoogieUtils::ERR_EXPR = "__ERROR";

smack::ProcDecl* ASTBoogieUtils::createTransferProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value, amount
	list<smack::Binding> transferParams{
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_VALUE, context.bitPrecise() ? "bv256" : "int"},
		{"amount", context.bitPrecise() ? "bv256" : "int"}
	};

	// Body
	smack::Block* transferImpl = smack::Block::block();
	const smack::Expr* this_bal = smack::Expr::sel(BOOGIE_BALANCE, BOOGIE_THIS);
	const smack::Expr* sender_bal = smack::Expr::sel(BOOGIE_BALANCE, BOOGIE_MSG_SENDER);
	const smack::Expr* amount = smack::Expr::id("amount");

	// balance[this] += amount
	transferImpl->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(
					smack::Expr::id(BOOGIE_BALANCE),
					smack::Expr::id(BOOGIE_THIS),
					encodeArithBinOp(context, nullptr, Token::Value::Add, this_bal, amount, 256, false))));
	// balance[msg.sender] -= amount
	transferImpl->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(
					smack::Expr::id(BOOGIE_BALANCE),
					smack::Expr::id(BOOGIE_MSG_SENDER),
					encodeArithBinOp(context, nullptr, Token::Value::Sub, sender_bal, amount, 256, false))));
	transferImpl->addStmt(smack::Stmt::comment("TODO: call fallback, exception handling"));
	smack::ProcDecl* transfer = smack::Decl::procedure(BOOGIE_TRANSFER, transferParams, {}, {}, {transferImpl});

	// Precondition: there is enough ether to transfer
	transfer->getRequires().push_back(smack::Specification::spec(
			encodeArithBinOp(context, nullptr, Token::Value::GreaterThanOrEqual, sender_bal, amount, 256, false),
			{smack::Attr::attr("message", "Transfer might fail due to insufficient ether")}));
	// Postcondition: if sender and receiver is different ether gets transferred, otherwise nothing happens
	transfer->getEnsures().push_back(smack::Specification::spec(smack::Expr::cond(
			smack::Expr::neq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER)),
			smack::Expr::and_(
					smack::Expr::eq(sender_bal,
							encodeArithBinOp(context, nullptr, Token::Value::Sub, smack::Expr::old(sender_bal), amount, 256, false)),
					smack::Expr::eq(this_bal,
							encodeArithBinOp(context, nullptr, Token::Value::Add, smack::Expr::old(this_bal), amount, 256, false))),
			smack::Expr::and_(
					smack::Expr::eq(sender_bal, smack::Expr::old(sender_bal)),
					smack::Expr::eq(this_bal, smack::Expr::old(this_bal))))));

	return transfer;
}

smack::ProcDecl* ASTBoogieUtils::createCallProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value
	list<smack::Binding> callParams {
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_VALUE, context.bitPrecise() ? "bv256" : "int"}
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
	thenBlock->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(
					smack::Expr::id(BOOGIE_BALANCE),
					smack::Expr::id(BOOGIE_THIS),
					encodeArithBinOp(context, nullptr, Token::Value::Add, this_bal, msg_val, 256, false))));
	thenBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(true)));
	// Unsuccessful transfer
	smack::Block* elseBlock = smack::Block::block();
	elseBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(false)));
	// Nondeterministic choice between success and failure
	smack::Block* callBlock = smack::Block::block();
	callBlock->addStmt(smack::Stmt::comment("TODO: call fallback"));
	callBlock->addStmt(smack::Stmt::ifelse(smack::Expr::id("*"), thenBlock, elseBlock));

	smack::ProcDecl* callProc = smack::Decl::procedure(BOOGIE_CALL, callParams, callReturns, {}, {callBlock});
	// Postcondition: if result is false nothing happens
	callProc->getEnsures().push_back(smack::Specification::spec(smack::Expr::or_(result,
			smack::Expr::eq(smack::Expr::id(BOOGIE_BALANCE), smack::Expr::old(smack::Expr::id(BOOGIE_BALANCE))))));
	return callProc;
}

smack::ProcDecl* ASTBoogieUtils::createSendProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value, amount
	list<smack::Binding> sendParams {
		{BOOGIE_THIS, BOOGIE_ADDRESS_TYPE},
		{BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE},
		{ASTBoogieUtils::BOOGIE_MSG_VALUE, context.bitPrecise() ? "bv256" : "int"},
		{"amount", context.bitPrecise() ? "bv256" : "int"}
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
	thenBlock->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(
					smack::Expr::id(BOOGIE_BALANCE),
					smack::Expr::id(BOOGIE_THIS),
					encodeArithBinOp(context, nullptr, Token::Value::Add, this_bal, amount, 256, false))));
	// balance[msg.sender] -= amount
	thenBlock->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(
					smack::Expr::id(BOOGIE_BALANCE),
					smack::Expr::id(BOOGIE_MSG_SENDER),
					encodeArithBinOp(context, nullptr, Token::Value::Sub, sender_bal, amount, 256, false))));
	thenBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(true)));
	// Unsuccessful transfer
	smack::Block* elseBlock = smack::Block::block();
	elseBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(false)));
	// Nondeterministic choice between success and failure
	smack::Block* transferBlock = smack::Block::block();
	transferBlock->addStmt(smack::Stmt::comment("TODO: call fallback"));
	transferBlock->addStmt(smack::Stmt::ifelse(smack::Expr::id("*"), thenBlock, elseBlock));

	smack::ProcDecl* sendProc = smack::Decl::procedure(BOOGIE_SEND, sendParams, sendReturns, {}, {transferBlock});

	// Precondition: there is enough ether to transfer
	sendProc->getRequires().push_back(smack::Specification::spec(
			encodeArithBinOp(context, nullptr, Token::Value::GreaterThanOrEqual, sender_bal, amount, 256, false),
			{smack::Attr::attr("message", "Send might fail due to insufficient ether")}));
	// Postcondition: if result is true and sender/receiver is different ether gets transferred
	// otherwise nothing happens
	sendProc->getEnsures().push_back(smack::Specification::spec(smack::Expr::cond(
			smack::Expr::and_(result, smack::Expr::neq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER))),
			smack::Expr::and_(
					smack::Expr::eq(sender_bal,
							encodeArithBinOp(context, nullptr, Token::Value::Sub, smack::Expr::old(sender_bal), amount, 256, false)),
					smack::Expr::eq(this_bal,
							encodeArithBinOp(context, nullptr, Token::Value::Add, smack::Expr::old(this_bal), amount, 256, false))),
			smack::Expr::and_(
					smack::Expr::eq(sender_bal, smack::Expr::old(sender_bal)),
					smack::Expr::eq(this_bal, smack::Expr::old(this_bal))))));
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
		if (tpStr == "int" + to_string(i)) return context.bitPrecise() ? "bv" + to_string(i) : "int";
		if (tpStr == "uint" + to_string(i)) return context.bitPrecise() ? "bv" + to_string(i) : "int";
	}
	if (boost::algorithm::starts_with(tpStr, "contract ")) return BOOGIE_ADDRESS_TYPE;

	if (tp->category() == Type::Category::Array)
	{
		auto arrType = dynamic_cast<ArrayType const*>(&*tp);
		return "[" + string(context.bitPrecise() ? "bv256" : "int") + "]" + mapType(arrType->baseType(), _associatedNode, context);
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
		if (context.bitPrecise())
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
		context.errorReporter().error(Error::Type::ParserError, _associatedNode.location(), "Tuples are not supported");
	}
	else
	{
		context.errorReporter().error(Error::Type::ParserError, _associatedNode.location(), "Unsupported type: '" + tpStr.substr(0, tpStr.find(' ')) + "'");
	}
	return ERR_TYPE;
}


list<const smack::Attr*> ASTBoogieUtils::createAttrs(SourceLocation const& loc, std::string const& message, Scanner const& scanner)
{
	int srcLine, srcCol;
	tie(srcLine, srcCol) = scanner.translatePositionToLineColumn(loc.start);
	return {
		smack::Attr::attr("sourceloc", *loc.sourceName, srcLine + 1, srcCol + 1),
		smack::Attr::attr("message", message)
	};
}


smack::Expr const* ASTBoogieUtils::encodeArithBinOp(BoogieContext& context, ASTNode const* associatedNode, Token::Value op,
		 smack::Expr const* lhs, smack::Expr const* rhs, unsigned bits, bool isSigned)
{
	switch(context.encoding())
	{
	case BoogieContext::Encoding::INT:
		switch(op)
		{
		case Token::Add: return smack::Expr::plus(lhs, rhs);
		case Token::Sub: return smack::Expr::minus(lhs, rhs);
		case Token::Mul: return smack::Expr::times(lhs, rhs);
		// TODO: returning integer division is fine, because Solidity does not support floats yet
		case Token::Div: return smack::Expr::intdiv(lhs, rhs);
		case Token::Mod: return smack::Expr::mod(lhs, rhs);

		case Token::Equal: return smack::Expr::eq(lhs, rhs);
		case Token::NotEqual: return smack::Expr::neq(lhs, rhs);
		case Token::LessThan: return smack::Expr::lt(lhs, rhs);
		case Token::GreaterThan: return smack::Expr::gt(lhs, rhs);
		case Token::LessThanOrEqual: return smack::Expr::lte(lhs, rhs);
		case Token::GreaterThanOrEqual: return smack::Expr::gte(lhs, rhs);

		case Token::Exp:
			if (auto rhsLit = dynamic_cast<smack::IntLit const *>(rhs))
			{
				if (auto lhsLit = dynamic_cast<smack::IntLit const *>(lhs))
				{
					return smack::Expr::lit(boost::multiprecision::pow(lhsLit->getVal(), rhsLit->getVal().convert_to<unsigned>()));
				}
			}
			context.reportError(associatedNode, "Exponentiation is not supported in 'int' encoding");
			return smack::Expr::id(ERR_EXPR);
		default:
			context.reportError(associatedNode, string("Unsupported binary operator in 'int' encoding ") + Token::toString(op));
			return smack::Expr::id(ERR_EXPR);
		}
		break;
	case BoogieContext::Encoding::BV:
		switch (op) {
		case Token::Add:
		case Token::Sub:
		case Token::Mul:
		case Token::Div:
		case Token::BitAnd:
		case Token::BitOr:
		case Token::BitXor:
		case Token::SHL:
		case Token::SAR:
		case Token::Equal:
		case Token::NotEqual:
		case Token::LessThan:
		case Token::GreaterThan:
		case Token::LessThanOrEqual:
		case Token::GreaterThanOrEqual:
			return bvBinaryFunc(context, op, lhs, rhs, bits, isSigned);
		default:
			context.reportError(associatedNode, string("Unsupported binary operator in 'bv' encoding ") + Token::toString(op));
			return smack::Expr::id(ERR_EXPR);
		}
		break;
	case BoogieContext::Encoding::MOD:

		break;

	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unknown encoding"));
		return nullptr;
	}
	return smack::Expr::id(ERR_EXPR);
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

smack::Expr const* ASTBoogieUtils::bvBinaryFunc(BoogieContext& context, Token::Value op,
		smack::Expr const* lhs, smack::Expr const* rhs, unsigned bits, bool isSigned)
{
	string name("");
	string retType("");
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

	case Token::Equal: return smack::Expr::eq(lhs, rhs);
	case Token::NotEqual: return smack::Expr::neq(lhs, rhs);

	case Token::LessThan: name = isSigned ? "slt" : "ult"; retType = "bool"; break;
	case Token::GreaterThan: name = isSigned ? "sgt" : "ugt"; retType = "bool";  break;
	case Token::LessThanOrEqual: name = isSigned ? "sle" : "ule"; retType = "bool";  break;
	case Token::GreaterThanOrEqual: name = isSigned ? "sge" : "uge"; retType = "bool";  break;
	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment(string("Unsupported binary operator in bit-precise mode ") + Token::toString(op)));
		return nullptr;
	}
	string fullName = "bv" + to_string(bits) + name;
	context.includeBuiltInFunction(fullName, smack::Decl::function(
					fullName, {{"", "bv"+to_string(bits)}, {"", "bv"+to_string(bits)}}, retType, nullptr,
					{smack::Attr::attr("bvbuiltin", "bv" + name)}));

	return smack::Expr::fn(fullName, lhs, rhs);
}

smack::Expr const* ASTBoogieUtils::bvUnaryFunc(BoogieContext& context, Token::Value op, smack::Expr const* subExpr, unsigned bits, bool)
{
	string name("");
	switch (op) {
	case Token::Add: return subExpr;
	case Token::Sub: name = "neg"; break;
	case Token::BitNot: name = "not"; break;
	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment(string("Unsupported unary operator in bit-precise mode ") + Token::toString(op)));
		return nullptr;
	}

	string fullName = "bv" + to_string(bits) + name;
	context.includeBuiltInFunction(fullName, smack::Decl::function(
					fullName, {{"", "bv"+to_string(bits)}}, "bv"+to_string(bits), nullptr,
					{smack::Attr::attr("bvbuiltin", "bv" + name)}));

	return smack::Expr::fn(fullName, subExpr);
}


}
}
