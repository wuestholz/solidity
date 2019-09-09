#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/ast/Types.h>
#include <libsolidity/ast/TypeProvider.h>
#include <liblangutil/SourceLocation.h>
#include <liblangutil/Exceptions.h>
#include <liblangutil/Scanner.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace langutil;

namespace bg = boogie;

namespace dev
{
namespace solidity
{
string const ASTBoogieUtils::SOLIDITY_BALANCE = "balance";
string const ASTBoogieUtils::SOLIDITY_TRANSFER = "transfer";
string const ASTBoogieUtils::BOOGIE_TRANSFER = "__transfer";
string const ASTBoogieUtils::SOLIDITY_SEND = "send";
string const ASTBoogieUtils::BOOGIE_SEND = "__send";
string const ASTBoogieUtils::SOLIDITY_CALL = "call";
string const ASTBoogieUtils::BOOGIE_CALL = "__call";
string const ASTBoogieUtils::SOLIDITY_SUPER = "super";

string const ASTBoogieUtils::SOLIDITY_SENDER = "sender";
string const ASTBoogieUtils::SOLIDITY_VALUE = "value";

string const ASTBoogieUtils::SOLIDITY_ASSERT = "assert";
string const ASTBoogieUtils::SOLIDITY_REQUIRE = "require";
string const ASTBoogieUtils::SOLIDITY_REVERT = "revert";

string const ASTBoogieUtils::SOLIDITY_THIS = "this";
string const ASTBoogieUtils::VERIFIER_SUM = "__verifier_sum";
string const ASTBoogieUtils::VERIFIER_IDX = "__verifier_idx";
string const ASTBoogieUtils::VERIFIER_OLD = "__verifier_old";
string const ASTBoogieUtils::VERIFIER_EQ = "__verifier_eq";
string const ASTBoogieUtils::BOOGIE_CONSTRUCTOR = "__constructor";
string const ASTBoogieUtils::SOLIDITY_NOW = "now";
string const ASTBoogieUtils::BOOGIE_NOW = "__now";
string const ASTBoogieUtils::SOLIDITY_NUMBER = "number";
string const ASTBoogieUtils::BOOGIE_BLOCKNO = "__block__number";
string const ASTBoogieUtils::VERIFIER_OVERFLOW = "__verifier_overflow";

string const ASTBoogieUtils::ERR_EXPR = "__ERROR";

string const ASTBoogieUtils::DOCTAG_CONTRACT_INVAR = "invariant";
string const ASTBoogieUtils::DOCTAG_CONTRACT_INVARS_INCLUDE = "{contractInvariants}";
string const ASTBoogieUtils::DOCTAG_LOOP_INVAR = "invariant";
string const ASTBoogieUtils::DOCTAG_PRECOND = "precondition";
string const ASTBoogieUtils::DOCTAG_POSTCOND = "postcondition";
string const ASTBoogieUtils::DOCTAG_MODIFIES = "modifies";
string const ASTBoogieUtils::DOCTAG_MODIFIES_ALL = DOCTAG_MODIFIES + " *";
string const ASTBoogieUtils::DOCTAG_MODIFIES_COND = " if ";

bg::ProcDeclRef ASTBoogieUtils::createTransferProc(BoogieContext& context)
{
	// Parameters: this, msg.sender, msg.value, amount
	vector<bg::Binding> transferParams{
		{context.boogieThis()->getRefTo(), context.boogieThis()->getType() },
		{context.boogieMsgSender()->getRefTo(), context.boogieMsgSender()->getType() },
		{context.boogieMsgValue()->getRefTo(), context.boogieMsgValue()->getType() },
		{bg::Expr::id("amount"), context.intType(256) }
	};

	// Type to pass around
	TypePointer tp_uint256 = TypeProvider::integer(256, IntegerType::Modifier::Unsigned);

	// Body
	bg::Block::Ref transferImpl = bg::Block::block();
	bg::Expr::Ref this_bal = bg::Expr::arrsel(context.boogieBalance()->getRefTo(), context.boogieThis()->getRefTo());
	bg::Expr::Ref sender_bal = bg::Expr::arrsel(context.boogieBalance()->getRefTo(), context.boogieMsgSender()->getRefTo());
	bg::Expr::Ref amount = bg::Expr::id("amount");

	// Precondition: there is enough ether to transfer
	auto geqResult = encodeArithBinaryOp(context, nullptr, langutil::Token::GreaterThanOrEqual, sender_bal, amount, 256, false);
	transferImpl->addStmt(bg::Stmt::assume(geqResult.expr));
	// balance[this] += amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		transferImpl->addStmts({
			bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)),
			bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256))
		});
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, amount, 256, false);
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		transferImpl->addStmts({
			bg::Stmt::comment("Implicit assumption that balances cannot overflow"),
			bg::Stmt::assume(addBalance.cc)
		});
	}
	transferImpl->addStmt(bg::Stmt::assign(
			context.boogieBalance()->getRefTo(),
			bg::Expr::arrupd(context.boogieBalance()->getRefTo(), context.boogieThis()->getRefTo(), addBalance.expr)));
	// balance[msg.sender] -= amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		transferImpl->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(sender_bal, tp_uint256)));
		transferImpl->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256)));
	}
	auto subSenderBalance = encodeArithBinaryOp(context, nullptr, Token::Sub, sender_bal, amount, 256, false);
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		transferImpl->addStmt(bg::Stmt::comment("Implicit assumption that balances cannot overflow"));
		transferImpl->addStmt(bg::Stmt::assume(subSenderBalance.cc));
	}
	transferImpl->addStmt(bg::Stmt::assign(
			context.boogieBalance()->getRefTo(),
			bg::Expr::arrupd(context.boogieBalance()->getRefTo(), context.boogieMsgSender()->getRefTo(), subSenderBalance.expr)));
	transferImpl->addStmt(bg::Stmt::comment("TODO: call fallback, exception handling"));

	bg::ProcDeclRef transfer = bg::Decl::procedure(BOOGIE_TRANSFER, transferParams, {}, {}, {transferImpl});

	transfer->addAttrs({
		bg::Attr::attr("inline", 1),
		bg::Attr::attr("message", "transfer")
	});
	return transfer;
}

bg::ProcDeclRef ASTBoogieUtils::createCallProc(BoogieContext& context)
{

	// Parameters: this, msg.sender, msg.value
	vector<bg::Binding> callParams {
		{context.boogieThis()->getRefTo(), context.boogieThis()->getType() },
		{context.boogieMsgSender()->getRefTo(), context.boogieMsgSender()->getType() },
		{context.boogieMsgValue()->getRefTo(), context.boogieMsgValue()->getType() }
	};

	// Type to pass around
	TypePointer tp_uint256 = TypeProvider::integer(256, IntegerType::Modifier::Unsigned);

	// Get the type of the call function
	auto callType = TypeProvider::address()->memberType("call");
	auto callFunctionType = dynamic_cast<FunctionType const*>(callType);

	solAssert(callFunctionType, "");
	solAssert(callFunctionType->returnParameterTypes().size() == 2, "");

	// Return value
	vector<bg::Binding> callReturns{
		{bg::Expr::id("__result"), context.toBoogieType(callFunctionType->returnParameterTypes()[0], nullptr)},
		{bg::Expr::id("__calldata"), context.toBoogieType(callFunctionType->returnParameterTypes()[1], nullptr)}
	};

	// Body
	// Successful transfer
	bg::Block::Ref thenBlock = bg::Block::block();
	bg::Expr::Ref this_bal = bg::Expr::arrsel(context.boogieBalance()->getRefTo(), context.boogieThis()->getRefTo());
	bg::Expr::Ref msg_val = context.boogieMsgValue()->getRefTo();
	bg::Expr::Ref result = bg::Expr::id("__result");

	// balance[this] += msg.value
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		thenBlock->addStmts({
			bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)),
			bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(msg_val, tp_uint256))
		});
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, msg_val, 256, false);
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		thenBlock->addStmts({
			bg::Stmt::comment("Implicit assumption that balances cannot overflow"),
			bg::Stmt::assume(addBalance.cc)
		});
	}
	thenBlock->addStmt(bg::Stmt::assign(
			context.boogieBalance()->getRefTo(),
			bg::Expr::arrupd(context.boogieBalance()->getRefTo(), context.boogieThis()->getRefTo(), addBalance.expr)));
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
	bg::Expr::Ref amount = bg::Expr::id("amount");
	bg::Expr::Ref result = bg::Expr::id("__result");
	// Parameters: this, msg.sender, msg.value, amount
	vector<bg::Binding> sendParams {
		{context.boogieThis()->getRefTo(), context.boogieThis()->getType()},
		{context.boogieMsgSender()->getRefTo(), context.boogieMsgSender()->getType()},
		{context.boogieMsgValue()->getRefTo(), context.boogieMsgValue()->getType()},
		{amount, context.intType(256)}
	};

	// Type to pass around
	TypePointer tp_uint256 = TypeProvider::integer(256, IntegerType::Modifier::Unsigned);

	// Return value
	vector<bg::Binding> sendReturns{ {result, context.boolType()} };

	// Body
	// Successful transfer
	bg::Block::Ref thenBlock = bg::Block::block();
	bg::Expr::Ref this_bal = bg::Expr::arrsel(context.boogieBalance()->getRefTo(), context.boogieThis()->getRefTo());
	bg::Expr::Ref sender_bal = bg::Expr::arrsel(context.boogieBalance()->getRefTo(), context.boogieMsgSender()->getRefTo());

	// balance[this] += amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		thenBlock->addStmts({
			bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)),
			bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256))
		});
	}
	auto addBalance = encodeArithBinaryOp(context, nullptr, Token::Add, this_bal, amount, 256, false);
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		thenBlock->addStmts({
			bg::Stmt::comment("Implicit assumption that balances cannot overflow"),
			bg::Stmt::assume(addBalance.cc)
		});
	}
	thenBlock->addStmt(bg::Stmt::assign(
			context.boogieBalance()->getRefTo(),
			bg::Expr::arrupd(context.boogieBalance()->getRefTo(), context.boogieThis()->getRefTo(), addBalance.expr)));
	// balance[msg.sender] -= amount
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		thenBlock->addStmts({
			bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(sender_bal, tp_uint256)),
			bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(amount, tp_uint256))
		});
	}
	auto subSenderBalance = encodeArithBinaryOp(context, nullptr, Token::Sub, sender_bal, amount, 256, false);
	if (context.encoding() == BoogieContext::Encoding::MOD)
	{
		thenBlock->addStmts({
			bg::Stmt::comment("Implicit assumption that balances cannot overflow"),
			bg::Stmt::assume(subSenderBalance.cc)
		});
	}
	thenBlock->addStmt(bg::Stmt::assign(
			context.boogieBalance()->getRefTo(),
			bg::Expr::arrupd(context.boogieBalance()->getRefTo(), context.boogieMsgSender()->getRefTo(), subSenderBalance.expr)));
	thenBlock->addStmt(bg::Stmt::assign(result, bg::Expr::lit(true)));

	// Unsuccessful transfer
	bg::Block::Ref elseBlock = bg::Block::block();
	elseBlock->addStmt(bg::Stmt::assign(result, bg::Expr::lit(false)));

	bg::Block::Ref transferBlock = bg::Block::block();
	// Precondition: there is enough ether to transfer
	auto senderBalanceGEQ = encodeArithBinaryOp(context, nullptr, langutil::Token::GreaterThanOrEqual, sender_bal, amount, 256, false);
	transferBlock->addStmt(bg::Stmt::assume(senderBalanceGEQ.expr));

	// Nondeterministic choice between success and failure
	transferBlock->addStmts({
		bg::Stmt::comment("TODO: call fallback"),
		bg::Stmt::ifelse(bg::Expr::id("*"), thenBlock, elseBlock)
	});

	bg::ProcDeclRef sendProc = bg::Decl::procedure(BOOGIE_SEND, sendParams, sendReturns, {}, {transferBlock});

	sendProc->addAttrs({
		bg::Attr::attr("inline", 1),
		bg::Attr::attr("message", "send")
	});

	return sendProc;
}

string ASTBoogieUtils::dataLocToStr(DataLocation loc)
{
	switch(loc)
	{
	case DataLocation::Storage: return "storage";
	case DataLocation::Memory: return "memory";
	case DataLocation::CallData: return "calldata";
	default:
		solAssert(false, "Unknown storage location.");
	}
	return "";
}

string ASTBoogieUtils::getConstructorName(ContractDefinition const* contract)
{
	return BOOGIE_CONSTRUCTOR + "#" + toString(contract->id());
}

std::vector<bg::Attr::Ref> ASTBoogieUtils::createAttrs(SourceLocation const& loc, std::string const& message, Scanner const& scanner)
{
	int srcLine, srcCol;
	tie(srcLine, srcCol) = scanner.translatePositionToLineColumn(loc.start);
	return {
		bg::Attr::attr("sourceloc", loc.source->name(), srcLine + 1, srcCol + 1),
		bg::Attr::attr("message", message)
	};
}

ASTBoogieUtils::ExprWithCC ASTBoogieUtils::encodeArithBinaryOp(BoogieContext& context, ASTNode const* associatedNode, langutil::Token op,
		bg::Expr::Ref lhs, bg::Expr::Ref rhs, unsigned bits, bool isSigned)
{
	bg::Expr::Ref result = nullptr;
	bg::Expr::Ref ecc = nullptr;

	switch(context.encoding())
	{
	case BoogieContext::Encoding::INT:
		switch(op)
		{
		case Token::Add:
		case Token::AssignAdd:
			result = bg::Expr::plus(lhs, rhs); break;
		case Token::Sub:
		case Token::AssignSub:
			result = bg::Expr::minus(lhs, rhs); break;
		case Token::Mul:
		case Token::AssignMul:
			result = bg::Expr::times(lhs, rhs); break;
		// TODO: returning integer division is fine, because Solidity does not support floats
		case Token::Div:
		case Token::AssignDiv:
			result = bg::Expr::intdiv(lhs, rhs); break;
		case Token::Mod:
		case Token::AssignMod:
			result = bg::Expr::mod(lhs, rhs); break;

		case Token::LessThan: result = bg::Expr::lt(lhs, rhs); break;
		case Token::GreaterThan: result = bg::Expr::gt(lhs, rhs); break;
		case Token::LessThanOrEqual: result = bg::Expr::lte(lhs, rhs); break;
		case Token::GreaterThanOrEqual: result = bg::Expr::gte(lhs, rhs); break;

		case Token::Exp:
		{
			auto lhsLit = dynamic_pointer_cast<bg::IntLit const>(lhs);
			auto rhsLit = dynamic_pointer_cast<bg::IntLit const>(rhs);
			if (lhsLit && rhsLit)
				result = bg::Expr::lit(boost::multiprecision::pow(lhsLit->getVal(), rhsLit->getVal().convert_to<unsigned>()));
			else
			{
				context.reportError(associatedNode, "Exponentiation is not supported in 'int' encoding");
				result = bg::Expr::id(ERR_EXPR);
			}
			break;
		}
		default:
			context.reportError(associatedNode, string("Unsupported binary operator in 'int' encoding ") + TokenTraits::toString(op));
			result = bg::Expr::id(ERR_EXPR);
		}
		break;
	case BoogieContext::Encoding::BV:
	{
		string name;
		string retType;

		switch (op)
		{
		case Token::Add:
		case Token::AssignAdd:
			result = context.bvAdd(bits, lhs, rhs); break;
		case Token::Sub:
		case Token::AssignSub:
			result = context.bvSub(bits, lhs, rhs); break;
		case Token::Mul:
		case Token::AssignMul:
			result = context.bvMul(bits, lhs, rhs); break;
		case Token::Div:
		case Token::AssignDiv:
			if (isSigned)
				result = context.bvSDiv(bits, lhs, rhs);
			else
				result = context.bvUDiv(bits, lhs, rhs);
			break;

		case Token::BitAnd:
		case Token::AssignBitAnd:
			result = context.bvAnd(bits, lhs, rhs); break;
		case Token::BitOr:
		case Token::AssignBitOr:
			result = context.bvOr(bits, lhs, rhs); break;
		case Token::BitXor:
		case Token::AssignBitXor:
			result = context.bvXor(bits, lhs, rhs); break;
		case Token::SAR:
		case Token::AssignSar:
			if (isSigned)
				result = context.bvAShr(bits, lhs, rhs);
			else
				result = context.bvLShr(bits, lhs, rhs);
			break;
		case Token::SHL:
		case Token::AssignShl:
			result = context.bvShl(bits, lhs, rhs); break;

		case Token::LessThan:
			if (isSigned)
				result = context.bvSlt(bits, lhs, rhs);
			else
				result = context.bvUlt(bits, lhs, rhs);
			break;
		case Token::GreaterThan:
			if (isSigned)
				result = context.bvSgt(bits, lhs, rhs);
			else
				result = context.bvUgt(bits, lhs, rhs);
			break;
		case Token::LessThanOrEqual:
			if (isSigned)
				result = context.bvSle(bits, lhs, rhs);
			else
				result = context.bvUle(bits, lhs, rhs);
			break;
		case Token::GreaterThanOrEqual:
			if (isSigned)
				result = context.bvSge(bits, lhs, rhs);
			else
				result = context.bvUge(bits, lhs, rhs);
			break;
		case Token::Exp:
		{
			auto lhsLit = dynamic_pointer_cast<bg::BvLit const>(lhs);
			auto rhsLit = dynamic_pointer_cast<bg::BvLit const>(rhs);
			if (lhsLit && rhsLit)
			{
				auto power = boost::multiprecision::pow(bg::bigint(lhsLit->getVal()),
						bg::bigint(rhsLit->getVal()).convert_to<unsigned>());
				result = context.intLit(power % boost::multiprecision::pow(bg::bigint(2), bits), bits);
			}
			else
			{
				context.reportError(associatedNode, "Exponentiation is not supported in 'bv' encoding");
				result = bg::Expr::id(ERR_EXPR);
			}
			break;
		}
		default:
			context.reportError(associatedNode, string("Unsupported binary operator in 'bv' encoding ") + TokenTraits::toString(op));
			result = bg::Expr::id(ERR_EXPR);
		}
		break;
	}
	case BoogieContext::Encoding::MOD:
	{
		auto modulo = bg::Expr::lit(boost::multiprecision::pow(bg::bigint(2), bits));
		auto largestSigned = bg::Expr::lit(boost::multiprecision::pow(bg::bigint(2), bits - 1) - 1);
		auto smallestSigned = bg::Expr::lit(-boost::multiprecision::pow(bg::bigint(2), bits - 1));
		switch(op)
		{
		case Token::Add:
		case Token::AssignAdd:
		{
			auto sum = bg::Expr::plus(lhs, rhs);
			if (isSigned)
				result = bg::Expr::cond(bg::Expr::gt(sum, largestSigned),
					bg::Expr::minus(sum, modulo),
					bg::Expr::cond(bg::Expr::lt(sum, smallestSigned), bg::Expr::plus(sum, modulo), sum));
			else
				result = bg::Expr::cond(bg::Expr::gte(sum, modulo), bg::Expr::minus(sum, modulo), sum);
			ecc = bg::Expr::eq(sum, result);
			break;
		}
		case Token::Sub:
		case Token::AssignSub:
		{
			auto diff = bg::Expr::minus(lhs, rhs);
			if (isSigned)
				result = bg::Expr::cond(bg::Expr::gt(diff, largestSigned),
					bg::Expr::minus(diff, modulo),
					bg::Expr::cond(bg::Expr::lt(diff, smallestSigned), bg::Expr::plus(diff, modulo), diff));
			else
				result = bg::Expr::cond(bg::Expr::gte(lhs, rhs), diff, bg::Expr::plus(diff, modulo));
			ecc = bg::Expr::eq(diff, result);
			break;
		}
		case Token::Mul:
		case Token::AssignMul:
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
		case Token::AssignDiv:
		{
			auto div = bg::Expr::intdiv(lhs, rhs);
			if (isSigned)
				result = bg::Expr::cond(bg::Expr::gt(div, largestSigned),
					bg::Expr::minus(div, modulo),
					bg::Expr::cond(bg::Expr::lt(div, smallestSigned), bg::Expr::plus(div, modulo), div));
			else
				result = div;
			ecc = bg::Expr::eq(div, result);
			break;
		}

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
		case Token::Exp:
		{
			auto lhsLit = dynamic_pointer_cast<bg::IntLit const>(lhs);
			auto rhsLit = dynamic_pointer_cast<bg::IntLit const>(rhs);
			if (lhsLit && rhsLit)
			{
				auto power = boost::multiprecision::pow(lhsLit->getVal(),rhsLit->getVal().convert_to<unsigned>());
				result = context.intLit(power % boost::multiprecision::pow(bg::bigint(2), isSigned ? bits - 1 : bits), bits);
				ecc = bg::Expr::eq(context.intLit(power, bits), result);
			}
			else
			{
				context.reportError(associatedNode, "Exponentiation is not supported in 'mod' encoding");
				result = bg::Expr::id(ERR_EXPR);
			}
			break;
		}
		default:
			context.reportError(associatedNode, string("Unsupported binary operator in 'mod' encoding ") + TokenTraits::toString(op));
			result = bg::Expr::id(ERR_EXPR);
		}
		break;
	}
	default:
		solAssert(false, "Unknown arithmetic encoding");
	}

	assert(result != nullptr);
	return ExprWithCC{result, ecc};
}

ASTBoogieUtils::ExprWithCC ASTBoogieUtils::encodeArithUnaryOp(BoogieContext& context, ASTNode const* associatedNode, Token op,
		bg::Expr::Ref subExpr, unsigned bits, bool isSigned)
{
	bg::Expr::Ref result = nullptr;
	bg::Expr::Ref ecc = nullptr;

	switch(context.encoding())
	{
	case BoogieContext::Encoding::INT:
		switch(op)
		{
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
		solAssert(false, "Unknown arithmetic encoding");
	}
	assert(result != nullptr);
	return ExprWithCC{result, ecc};
}

bool ASTBoogieUtils::isBitPreciseType(TypePointer type)
{
	switch (type->category())
	{
	case Type::Category::Integer:
	case Type::Category::FixedBytes:
	case Type::Category::Enum:
		return true;
	default:
		return false;
	}
	return true;
}

unsigned ASTBoogieUtils::getBits(TypePointer type)
{
	if (auto intType = dynamic_cast<IntegerType const*>(type))
		return intType->numBits();
	if (dynamic_cast<EnumType const*>(type))
		return 256;
	if (auto fb = dynamic_cast<FixedBytesType const*>(type))
		return fb->numBytes()*8;

	solAssert(false, "Trying to get bits for non-bitprecise type");
	return 0;
}

bool ASTBoogieUtils::isSigned(TypePointer type)
{
	if (auto intType = dynamic_cast<IntegerType const*>(type))
		return intType->isSigned();
	if (dynamic_cast<EnumType const*>(type))
		return false;
	if (dynamic_cast<FixedBytesType const*>(type))
		return false;

	solAssert(false, "Trying to get sign for non-bitprecise type");
	return false;
}

bg::Expr::Ref ASTBoogieUtils::checkImplicitBvConversion(bg::Expr::Ref expr, TypePointer exprType, TypePointer targetType, BoogieContext& context)
{
	solAssert(exprType, "");
	solAssert(targetType, "");

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
			if (targetBits == exprBits && targetSigned == exprSigned)
				return expr;
			// Conversion to smaller type should have already been detected by the compiler
			solAssert (targetBits >= exprBits, "Implicit conversion to smaller type");

			if (!exprSigned) // Unsigned can be converted to larger (signed or unsigned) with zero extension
				return context.bvZeroExt(expr, exprBits, targetBits);
			else if (targetSigned) // Signed can only be converted to signed with sign extension
				return context.bvSignExt(expr, exprBits, targetBits);
			else // Signed to unsigned should have already been detected by the compiler
			{
				solAssert(false, "Implicit conversion from signed to unsigned");
				return nullptr;
			}
		}
	}

	return expr;
}

bg::Expr::Ref ASTBoogieUtils::checkExplicitBvConversion(bg::Expr::Ref expr, TypePointer exprType, TypePointer targetType, BoogieContext& context)
{
	// Do nothing if any of the types is unknown
	if (!targetType || !exprType)
		return expr;

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
			// - converting from unsigned to same size signed
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
	// For enums we know the number of members
	if (tp->category() == Type::Category::Enum)
	{
		auto enumTp = dynamic_cast<EnumType const *>(tp);
		solAssert(enumTp, "");
		return bg::Expr::and_(
				bg::Expr::lte(bg::Expr::lit((long)0), expr),
				bg::Expr::lt(expr, bg::Expr::lit(enumTp->enumDefinition().members().size())));

	}
	// Otherwise get smallest and largest
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

bg::Expr::Ref ASTBoogieUtils::defaultValue(TypePointer type, BoogieContext& context)
{
	return defaultValueInternal(type, context).bgExpr;
}

ASTBoogieUtils::DefVal ASTBoogieUtils::defaultValueInternal(TypePointer type, BoogieContext& context)
{
	switch (type->category()) // TODO: bitvectors
	{
	case Type::Category::Integer:
	{
		// 0
		auto lit = context.intLit(0, ASTBoogieUtils::getBits(type));
		return {lit->tostring(), lit};
	}
	case Type::Category::Address:
	case Type::Category::Contract:
	case Type::Category::Enum:
	{
		// 0
		auto lit = context.intLit(0, 256);
		return {lit->tostring(), lit};
	}
	case Type::Category::Bool:
	{
		// False
		auto lit = bg::Expr::lit(false);
		return {lit->tostring(), lit};
	}
	case Type::Category::FixedBytes:
	{
		auto fbType = dynamic_cast<FixedBytesType const*>(type);
		auto lit = context.intLit(0, fbType->numBytes() * 8);
		return {lit->tostring(), lit};
	}
	case Type::Category::Struct:
	{
		// default for all members
		StructType const* structType = dynamic_cast<StructType const*>(type);
		if (structType->dataStoredIn(DataLocation::Storage))
		{
			MemberList const& members = structType->members(0); // No need for scope, just regular struct members
			// Get the default value for the members
			vector<bg::Expr::Ref> memberDefValExprs;
			vector<string> memberDefValSmts;
			for (auto& member: members)
			{
				DefVal memberDefaultValue = defaultValueInternal(member.type, context);
				if (memberDefaultValue.bgExpr == nullptr)
					return {"", nullptr};
				memberDefValExprs.push_back(memberDefaultValue.bgExpr);
				memberDefValSmts.push_back(memberDefaultValue.smt);
			}
			// Now construct the struct value
			StructDefinition const& structDefinition = structType->structDefinition();
			bg::FuncDeclRef structConstr = context.getStructConstructor(&structDefinition);
			string smtExpr = "(|" + structConstr->getName() + "|";
			for (string s: memberDefValSmts)
				smtExpr += " " + s;
			smtExpr += ")";
			return {smtExpr, bg::Expr::fn(structConstr->getName(), memberDefValExprs)};
		}
		break;
	}
	case Type::Category::Array:
	{
		ArrayType const* arrayType = dynamic_cast<ArrayType const*>(type);
		if (arrayType->dataStoredIn(DataLocation::Storage))
		{
			auto keyType = context.intType(256);
			auto valType = context.toBoogieType(arrayType->baseType(), nullptr);
			auto baseDefVal = defaultValueInternal(arrayType->baseType(), context);
			string smtType = "(Array " + keyType->getSmtType() + " " + valType->getSmtType() + ")";
			string smtExpr = "((as const " + smtType + ") " + baseDefVal.smt + ")";
			bg::FuncDeclRef arrConstr = context.getArrayConstructor(valType);
			auto arrLength = context.intLit(arrayType->length(), 256);
			auto innerArr = bg::Expr::fn(context.defaultArray(keyType, valType, smtExpr)->getName(), vector<bg::Expr::Ref>());
			vector<bg::Expr::Ref> args;
			args.push_back(innerArr);
			args.push_back(arrLength);
			auto bgExpr = bg::Expr::fn(arrConstr->getName(), args);
			// SMT expression is constructed afterwards (not to be included in the const array function)
			smtExpr = "(|" + arrConstr->getName() + "| " + smtExpr + " " + arrLength->tostring() + ")";
			return {smtExpr, bgExpr};
		}
		break;
	}
	case Type::Category::Mapping:
	{
		MappingType const* mappingType = dynamic_cast<MappingType const*>(type);
		auto keyType = context.toBoogieType(mappingType->keyType(), nullptr);
		auto valType = context.toBoogieType(mappingType->valueType(), nullptr);
		auto mapType = context.toBoogieType(type, nullptr);

		auto baseDefVal = defaultValueInternal(mappingType->valueType(), context);
		string smtType = mapType->getSmtType();
		string smtExpr = "((as const " + smtType + ") " + baseDefVal.smt + ")";
		auto bgExpr = bg::Expr::fn(context.defaultArray(keyType, valType, smtExpr)->getName(), vector<bg::Expr::Ref>());
		return {smtExpr, bgExpr};
	}
	default:
		// For unhandled, just return null
		break;
	}

	return {"", nullptr};
}

bg::Decl::Ref ASTBoogieUtils::newStruct(StructDefinition const* structDef, BoogieContext& context)
{
	// Address of the new struct
	// TODO: make sure that it is a new address
	string prefix = "new_struct_" + structDef->name();
	bg::TypeDeclRef varType = context.getStructType(structDef, DataLocation::Memory);
	return context.freshTempVar(varType, prefix);
}

bg::Decl::Ref ASTBoogieUtils::newArray(bg::TypeDeclRef type, BoogieContext& context)
{
	// TODO: make sure that it is a new address
	return context.freshTempVar(type, "new_array");
}

ASTBoogieUtils::AssignResult ASTBoogieUtils::makeAssign(AssignParam lhs, AssignParam rhs, langutil::Token op,
		ASTNode const* assocNode, BoogieContext& context)
{
	AssignResult res;
	makeAssignInternal(lhs, rhs, op, assocNode, context, res);
	return res;
}

void ASTBoogieUtils::makeAssignInternal(AssignParam lhs, AssignParam rhs, langutil::Token op,
		ASTNode const* assocNode, BoogieContext& context, AssignResult& result)
{
	if (dynamic_cast<TupleType const*>(lhs.type))
	{
		if (dynamic_cast<TupleType const*>(rhs.type))
			makeTupleAssign(lhs, rhs, assocNode, context, result);
		else
			context.reportError(assocNode, "LHS is tuple but RHS is not");
		return;
	}

	if (dynamic_cast<StructType const*>(lhs.type))
	{
		if (dynamic_cast<StructType const*>(rhs.type))
			makeStructAssign(lhs, rhs, assocNode, context, result);
		else
			context.reportError(assocNode, "LHS is struct but RHS is not");
		return;
	}

	if (auto lhsArrayType = dynamic_cast<ArrayType const*>(lhs.type))
	{
		if (lhsArrayType->isString())
		{
			makeBasicAssign(lhs, rhs, op, assocNode, context, result);
			return;
		}
		if (dynamic_cast<ArrayType const*>(rhs.type))
			makeArrayAssign(lhs, rhs, assocNode, context, result);
		else
			context.reportError(assocNode, "LHS is array but RHS is not");
		return;
	}

	makeBasicAssign(lhs, rhs, op, assocNode, context, result);
}

void ASTBoogieUtils::makeTupleAssign(AssignParam lhs, AssignParam rhs, ASTNode const* assocNode, BoogieContext& context, AssignResult& result)
{
	auto lhsTuple = dynamic_cast<bg::TupleExpr const*>(lhs.bgExpr.get());
	solAssert(lhsTuple, "Expected tuple as LHS");
	if (context.isBvEncoding())
		rhs.bgExpr = checkImplicitBvConversion(rhs.bgExpr, rhs.type, lhs.type, context);
	auto rhsTuple = dynamic_cast<bg::TupleExpr const*>(rhs.bgExpr.get());
	solAssert(rhsTuple, "Expected tuple as RHS");
	auto lhsTupleExpr = dynamic_cast<TupleExpression const*>(lhs.expr);
	auto rhsTupleExpr = dynamic_cast<TupleExpression const*>(rhs.expr);
	auto lhsType = dynamic_cast<TupleType const*>(lhs.type);
	solAssert(lhsType, "Expected tuple type for LHS");
	auto rhsType = dynamic_cast<TupleType const*>(rhs.type);
	solAssert(rhsType, "Expected tuple type for RHS");
	auto const& lhsElems = lhsTuple->elements();
	auto const& rhsElems = rhsTuple->elements();

	// Introduce temp variables due to expressions like (a, b) = (b, a)
	vector<bg::VarDeclRef> tmpVars;
	for (unsigned i = 0; i < lhsElems.size(); ++ i)
	{
		if (lhsElems[i])
		{
			auto rhsElem = rhsTupleExpr ? rhsTupleExpr->components().at(i).get() : nullptr;
			auto rhsElemType = rhsType->components().at(i);
			// Temp variable has the type of LHS by default, so that implicit conversions can happen
			auto tmpVarType = lhsType->components().at(i);
			// For reference types in storage, it should be local storage pointer
			auto rhsRef = dynamic_cast<ReferenceType const*>(rhsElemType);
			if (rhsRef && rhsElemType->dataStoredIn(DataLocation::Storage))
				tmpVarType = TypeProvider::withLocation(rhsRef,DataLocation::Storage, true);

			auto tmp = context.freshTempVar(context.toBoogieType(tmpVarType, assocNode));
			result.newDecls.push_back(tmp);
			tmpVars.push_back(tmp);
			makeAssignInternal(
					AssignParam{tmp->getRefTo(), tmpVarType, nullptr },
					AssignParam{rhsElems[i], rhsElemType, rhsElem },
					Token::Assign, assocNode, context, result);
		}
		else
			tmpVars.push_back(nullptr);
	}

	for (int i = lhsElems.size() - 1; i >= 0; --i)
	{
		if (lhsElems[i])
		{
			auto const lhsElem = lhsTupleExpr ? lhsTupleExpr->components().at(i).get() : nullptr;
			auto lhsElemType = lhsType->components().at(i);
			auto rhsElemType = rhsType->components().at(i);
			// Temp variable has the type of LHS by default
			auto tmpVarType = lhsElemType;
			// For reference types in storage, it should be local storage pointer
			auto rhsRef = dynamic_cast<ReferenceType const*>(rhsElemType);
			if (rhsRef && rhsElemType->dataStoredIn(DataLocation::Storage))
				tmpVarType = TypeProvider::withLocation(rhsRef, DataLocation::Storage, true);

			auto rhsElem = rhsTupleExpr ? rhsTupleExpr->components().at(i).get() : nullptr;
			makeAssignInternal(
					AssignParam{lhsElems[i], lhsElemType, lhsElem },
					AssignParam{tmpVars[i]->getRefTo(), tmpVarType, rhsElem },
					Token::Assign, assocNode, context, result);
		}
	}
}

void ASTBoogieUtils::makeStructAssign(AssignParam lhs, AssignParam rhs, ASTNode const* assocNode, BoogieContext& context, AssignResult& result)
{
	auto lhsType = dynamic_cast<StructType const*>(lhs.type);
	auto rhsType = dynamic_cast<StructType const*>(rhs.type);
	solAssert(lhsType, "Expected struct type for LHS");
	solAssert(rhsType, "Expected struct type for RHS");
	auto lhsLoc = lhsType->location();
	auto rhsLoc = rhsType->location();
	// LHS is memory
	if (lhsLoc == DataLocation::Memory)
	{
		// RHS is memory --> reference copy
		if (rhsLoc == DataLocation::Memory)
		{
			makeBasicAssign(lhs, rhs, Token::Assign, assocNode, context, result);
			return;
		}
		// RHS is storage or calldata --> create new, deep copy
		else if (rhsLoc == DataLocation::Storage || rhsLoc == DataLocation::CallData)
		{
			// Create new
			auto varDecl = newStruct(&lhsType->structDefinition(), context);
			result.newDecls.push_back(varDecl);
			result.newStmts.push_back(bg::Stmt::assign(lhs.bgExpr, varDecl->getRefTo()));

			// RHS is local storage: unpack first
			if (rhsLoc == DataLocation::Storage && rhsType->isPointer())
				rhs.bgExpr = unpackLocalPtr(rhs.expr, rhs.bgExpr, context);

			// Make deep copy
			deepCopyStruct(&lhsType->structDefinition(), lhs.bgExpr, rhs.bgExpr,
					lhsLoc, rhsLoc, assocNode, context, result);
			return;
		}
		else
		{
			context.reportError(assocNode, "Assignment from " + dataLocToStr(rhsLoc) + " to memory struct is not supported");
			return;
		}
	}
	// LHS is storage
	else if (lhsLoc == DataLocation::Storage)
	{
		// RHS is storage
		if (rhsLoc == DataLocation::Storage)
		{
			// LHS is storage pointer --> reference copy
			if (lhsType->isPointer())
			{
				// pointer to pointer, simply assign
				if (rhsType->isPointer())
				{
					makeBasicAssign(lhs, rhs, Token::Assign, assocNode, context, result);
					return;
				}
				else // Otherwise pack
				{
					result.newStmts.push_back(bg::Stmt::comment("Packing local storage pointer"));
					auto packed = packToLocalPtr(rhs.expr, rhs.bgExpr, context);
					result.newDecls.push_back(packed.ptr);
					for (auto stmt: packed.stmts)
						result.newStmts.push_back(stmt);
					result.newStmts.push_back(bg::Stmt::assign(lhs.bgExpr, packed.ptr->getRefTo()));
					return;
				}
			}
			// LHS is storage --> deep copy by data types
			else
			{
				// Unpack pointers first
				if (rhsType->isPointer())
					rhs.bgExpr = unpackLocalPtr(rhs.expr, rhs.bgExpr, context);

				makeBasicAssign(lhs, rhs, Token::Assign, assocNode, context, result);
				return;
			}
		}
		// RHS is memory --> deep copy
		else if (rhsLoc == DataLocation::Memory)
		{
			deepCopyStruct(&lhsType->structDefinition(), lhs.bgExpr, rhs.bgExpr,
					lhsLoc, rhsLoc, assocNode, context, result);
			return;
		}
		else
		{
			context.reportError(assocNode, "Assignment from " + dataLocToStr(rhsLoc) + " to storage struct is not supported");
			return;
		}
	}
	context.reportError(assocNode, "Assigning to " + dataLocToStr(lhsLoc) + " struct is not supported");
}

void ASTBoogieUtils::makeArrayAssign(AssignParam lhs, AssignParam rhs, ASTNode const* assocNode, BoogieContext& context, AssignResult& result)
{
	auto lhsType = dynamic_cast<ArrayType const*>(lhs.type);
	auto rhsType = dynamic_cast<ArrayType const*>(rhs.type);
	solAssert(lhsType, "Expected array type for LHS");
	solAssert(rhsType, "Expected array type for RHS");
	if (lhsType->isString())
	{
		makeBasicAssign(lhs, rhs, Token::Assign, assocNode, context, result);
		return;
	}
	if (lhsType->location() != rhsType->location())
	{
		if (lhsType->location() == DataLocation::Memory)
		{
			// Create new
			auto varDecl = newArray(context.toBoogieType(lhsType, assocNode), context);
			result.newDecls.push_back(varDecl);
			result.newStmts.push_back(bg::Stmt::assign(lhs.bgExpr, varDecl->getRefTo()));
			lhs.bgExpr = context.getMemArray(lhs.bgExpr, context.toBoogieType(lhsType->baseType(), assocNode));
		}
		if (rhsType->location() == DataLocation::Memory || rhsType->location() == DataLocation::CallData)
			rhs.bgExpr = context.getMemArray(rhs.bgExpr, context.toBoogieType(rhsType->baseType(), assocNode));
	}
	makeBasicAssign(lhs, rhs, Token::Assign, assocNode, context, result);
}

void ASTBoogieUtils::makeBasicAssign(AssignParam lhs, AssignParam rhs, langutil::Token op, ASTNode const* assocNode, BoogieContext& context, AssignResult& result)
{
	// Bit-precise mode
	if (context.isBvEncoding() && isBitPreciseType(lhs.type))
		// Check for implicit conversion
		rhs.bgExpr = checkImplicitBvConversion(rhs.bgExpr, rhs.type, lhs.type, context);

	ExprWithCC rhsResult;
	// Check for additional arithmetic needed
	if (op == Token::Assign)
	{
		rhsResult.expr = rhs.bgExpr; // rhs already contains the result
	}
	else
	{
		// Transform rhs based on the operator, e.g., a += b becomes a := (a + b)
		unsigned bits = ASTBoogieUtils::getBits(lhs.type);
		bool isSigned = ASTBoogieUtils::isSigned(lhs.type);
		rhsResult = ASTBoogieUtils::encodeArithBinaryOp(context, assocNode, op, lhs.bgExpr, rhs.bgExpr, bits, isSigned);
	}
	if (context.overflow() && rhsResult.cc)
		result.ocs.push_back(rhsResult.cc);

	// First lift conditionals (due to local pointers)
	lhs.bgExpr = liftCond(lhs.bgExpr);
	// Then check if sum ghost variables need to be updated
	for (auto stmt: checkForSums(lhs.bgExpr, rhsResult.expr, context))
		result.newStmts.push_back(stmt);

	result.newStmts.push_back(bg::Stmt::assign(lhs.bgExpr, rhsResult.expr));
}

boogie::Expr::Ref ASTBoogieUtils::liftCond(boogie::Expr::Ref expr)
{
	if (auto selExpr = dynamic_pointer_cast<bg::SelExpr const>(expr))
	{
		// First recurse
		selExpr = dynamic_pointer_cast<bg::SelExpr const>(selExpr->replaceBase(liftCond(selExpr->getBase())));
		solAssert(selExpr, "");
		// Then check if lifting is needed
		if (auto baseCond = dynamic_pointer_cast<bg::CondExpr const>(selExpr->getBase()))
		{
			return bg::Expr::cond(baseCond->getCond(),
					liftCond(selExpr->replaceBase(baseCond->getThen())),
					liftCond(selExpr->replaceBase(baseCond->getElse())));
		}
		return selExpr;
	}
	return expr;
}

list<bg::Stmt::Ref> ASTBoogieUtils::checkForSums(bg::Expr::Ref lhs, bg::Expr::Ref rhs, BoogieContext& context)
{
	if (auto condExpr = dynamic_pointer_cast<bg::CondExpr const>(lhs))
	{
		vector<bg::Stmt::Ref> thenSums;
		for (auto stmt: checkForSums(condExpr->getThen(), rhs, context))
			thenSums.push_back(stmt);
		vector<bg::Stmt::Ref> elseSums;
		for (auto stmt: checkForSums(condExpr->getElse(), rhs, context))
			elseSums.push_back(stmt);

		if (thenSums.empty() && elseSums.empty())
			return {};
		else
			return {bg::Stmt::ifelse(condExpr->getCond(), bg::Block::block("", thenSums), bg::Block::block("", elseSums))};
	}
	return context.updateSumVars(lhs, rhs);
}

void ASTBoogieUtils::deepCopyStruct(StructDefinition const* structDef,
		bg::Expr::Ref lhsBase, bg::Expr::Ref rhsBase, DataLocation lhsLoc, DataLocation rhsLoc,
		ASTNode const* assocNode, BoogieContext& context, AssignResult& result)
{
	result.newStmts.push_back(bg::Stmt::comment("Deep copy struct " + structDef->name()));
	// Loop through each member
	for (auto member: structDef->members())
	{
		// Get expressions for accessing members
		bg::Expr::Ref lhsSel = nullptr;
		if (lhsLoc == DataLocation::Storage)
			lhsSel = bg::Expr::dtsel(lhsBase, context.mapDeclName(*member),
					context.getStructConstructor(structDef),
					dynamic_pointer_cast<bg::DataTypeDecl>(context.getStructType(structDef, lhsLoc)));
		else
			lhsSel = bg::Expr::arrsel(bg::Expr::id(context.mapDeclName(*member)), lhsBase);

		bg::Expr::Ref rhsSel = nullptr;
		if (rhsLoc == DataLocation::Storage)
			rhsSel = bg::Expr::dtsel(rhsBase, context.mapDeclName(*member),
					context.getStructConstructor(structDef),
					dynamic_pointer_cast<bg::DataTypeDecl>(context.getStructType(structDef, rhsLoc)));
		else
			rhsSel = bg::Expr::arrsel(bg::Expr::id(context.mapDeclName(*member)), rhsBase);

		auto memberType = member->annotation().type;
		auto memberTypeCat = memberType->category();
		// For nested structs do recursion
		if (memberTypeCat == Type::Category::Struct)
		{
			auto memberStructType = dynamic_cast<StructType const*>(memberType);
			// Deep copy into memory creates new
			if (lhsLoc == DataLocation::Memory)
			{
				// Create new
				auto varDecl = ASTBoogieUtils::newStruct(&memberStructType->structDefinition(), context);
				result.newDecls.push_back(varDecl);
				// Update member to point to new
				makeBasicAssign(
						AssignParam{lhsSel, memberType, nullptr},
						AssignParam{varDecl->getRefTo(), memberType, nullptr},
						Token::Assign, assocNode, context, result);
			}
			// Do the deep copy
			deepCopyStruct(&memberStructType->structDefinition(), lhsSel, rhsSel, lhsLoc, rhsLoc, assocNode, context, result);
		}
		else if (memberTypeCat == Type::Category::Mapping)
		{
			// Mappings are simply skipped
		}
		else if (memberTypeCat == Type::Category::Array)
		{
			auto arrType = dynamic_cast<ArrayType const*>(memberType);
			if (arrType->isString())
				makeBasicAssign(
						AssignParam{lhsSel, arrType, nullptr},
						AssignParam{rhsSel, arrType, nullptr},
						Token::Assign, assocNode, context, result);
			else
			{
				if (lhsLoc == DataLocation::Memory)
				{
					// Create new
					auto varDecl = ASTBoogieUtils::newArray(
							context.toBoogieType(TypeProvider::withLocation(arrType, DataLocation::Memory, false), assocNode),
							context);
					result.newDecls.push_back(varDecl);
					// Update member to point to new
					makeBasicAssign(
							AssignParam{lhsSel, memberType, nullptr},
							AssignParam{varDecl->getRefTo(), memberType, nullptr},
							Token::Assign, assocNode, context, result);
				}
				if (rhsLoc == DataLocation::Memory || rhsLoc == DataLocation::CallData)
					rhsSel = context.getMemArray(rhsSel, context.toBoogieType(arrType->baseType(), assocNode));

				makeBasicAssign(
						AssignParam{lhsSel, memberType, nullptr},
						AssignParam{rhsSel, memberType, nullptr},
						Token::Assign, assocNode, context, result);
			}
		}
		// For other types make the copy by updating the LHS with RHS
		else
		{
			makeBasicAssign(
					AssignParam{lhsSel, memberType, nullptr},
					AssignParam{rhsSel, memberType, nullptr},
					Token::Assign, assocNode, context, result);
		}
	}
}

ASTBoogieUtils::PackResult ASTBoogieUtils::packToLocalPtr(Expression const* expr, bg::Expr::Ref bgExpr, BoogieContext& context)
{
	PackResult result {nullptr, {}};
	auto structType = dynamic_cast<StructType const*>(expr->annotation().type);
	if (!structType)
	{
		context.reportError(expr, "Expected struct type");
		return PackResult{context.freshTempVar(context.errType()), {}};
	}
	packInternal(expr, bgExpr, structType, context, result);
	if (result.ptr)
		return result;

	context.reportError(expr, "Unsupported expression for packing into local pointer");
	return PackResult{context.freshTempVar(context.errType()), {}};
}

void ASTBoogieUtils::packInternal(Expression const* expr, bg::Expr::Ref bgExpr, StructType const* structType, BoogieContext& context, PackResult& result)
{
	// Packs an expression (path to storage) into a local pointer as an array
	// The general idea is to associate each state variable and member with an index
	// so that the path can be encoded as an integer array.
	//
	// Example:
	// contract C {
	//   struct T { int z; }
	//   struct S {
	//     T t1;     --> 0
	//     T[] ts;   --> 1 + array index
	//   }
	//
	//   T t1;       --> 0
	//   S s1;       --> 1
	//   S[] ss;     --> 2 + array index
	// }
	//
	// t1 becomes [0]
	// s1.t1 becomes [1, 0]
	// ss[5].ts[3] becomes [2, 5, 1, 3]

	// Function calls return pointers, no need to pack, just copy the return value
	if (dynamic_cast<FunctionCall const*>(expr))
	{
		auto ptr = context.freshTempVar(context.localPtrType());
		result.ptr = ptr;
		result.stmts.push_back(bg::Stmt::assign(ptr->getRefTo(), bgExpr));
		return;
	}
	// Identifier: search for matching state variable in the contract
	if (auto idExpr = dynamic_cast<Identifier const*>(expr))
	{
		auto ptr = context.freshTempVar(context.localPtrType());
		// Collect all variables from all contracts that can see the struct
		vector<VariableDeclaration const*> vars;
		for (auto contr: context.stats().contractsForStruct(&structType->structDefinition()))
		{
			auto subVars = ASTNode::filteredNodes<VariableDeclaration>(contr->subNodes());
			vars.insert(vars.end(), subVars.begin(), subVars.end());
		}
		for (unsigned i = 0; i < vars.size(); ++i)
		{
			// If found, put its index into the packed array
			if (vars[i] == idExpr->annotation().referencedDeclaration)
			{
				// Must be the first element in the packed array
				solAssert(!result.ptr, "Reassignment of packed pointer");
				solAssert(result.stmts.empty(), "Non-empty packing statements");
				result.ptr = ptr;
				result.stmts.push_back(bg::Stmt::assign(
						bg::Expr::arrsel(ptr->getRefTo(), context.intLit(0, 256)),
						context.intLit(i, 256)));
				return;
			}
		}
	}
	// Member access: process base recursively, then find matching member
	else if (auto memAccExpr = dynamic_cast<MemberAccess const*>(expr))
	{
		auto bgMemAccExpr = dynamic_pointer_cast<bg::DtSelExpr const>(bgExpr);
		solAssert(bgMemAccExpr, "Expected Boogie member access expression");
		packInternal(&memAccExpr->expression(), bgMemAccExpr->getBase(), structType, context, result);
		solAssert(result.ptr, "Empty pointer from subexpression");
		auto eStructType = dynamic_cast<StructType const*>(memAccExpr->expression().annotation().type);
		solAssert(eStructType, "Trying to pack member of non-struct expression");
		auto members = eStructType->structDefinition().members();
		for (unsigned i = 0; i < members.size(); ++i)
		{
			// If matching member found, put index in next position of the packed array
			if (members[i].get() == memAccExpr->annotation().referencedDeclaration)
			{
				unsigned nextPos = result.stmts.size();
				result.stmts.push_back(
						bg::Stmt::assign(
								bg::Expr::arrsel(result.ptr->getRefTo(), context.intLit(nextPos, 256)),
								context.intLit(i, 256)));
				return;
			}
		}
	}
	// Arrays and mappings: process base recursively, then store index expression in next position
	// in the packed array
	else if (auto idxExpr = dynamic_cast<IndexAccess const*>(expr))
	{
		auto bgIdxAccExpr = dynamic_pointer_cast<bg::ArrSelExpr const>(bgExpr);
		solAssert(bgIdxAccExpr, "Expected Boogie index access expression");
		bg::Expr::Ref base = bgIdxAccExpr->getBase();

		// Base has to be extracted in two steps for arrays, because an array is
		// actually a datatype with the actual array and its length
		if (idxExpr->baseExpression().annotation().type->category() == Type::Category::Array)
		{
			auto bgDtSelExpr = dynamic_pointer_cast<bg::DtSelExpr const>(bgIdxAccExpr->getBase());
			solAssert(bgIdxAccExpr, "Expected Boogie member access expression below indexer");
			base = bgDtSelExpr->getBase();
		}

		// Report error for unsupported index types
		auto idxType = idxExpr->indexExpression()->annotation().type->category();
		if (idxType == Type::Category::StringLiteral || idxType == Type::Category::Array)
		{
			context.reportError(idxExpr->indexExpression(), "Unsupported type for index in local pointer");
			return;
		}

		packInternal(&idxExpr->baseExpression(), base, structType, context, result);
		solAssert(result.ptr, "Empty pointer from subexpression");
		unsigned nextPos = result.stmts.size();
		result.stmts.push_back(bg::Stmt::assign(
				bg::Expr::arrsel(result.ptr->getRefTo(), context.intLit(nextPos, 256)),
				bgIdxAccExpr->getIdx()));
		return;
	}

	context.reportError(expr, "Unsupported expression for packing into local pointer");
}

bg::Expr::Ref ASTBoogieUtils::unpackLocalPtr(Expression const* ptrExpr, boogie::Expr::Ref ptrBgExpr, BoogieContext& context)
{
	auto result = unpackInternal(ptrExpr, ptrBgExpr, context.currentContract(), 0, nullptr, context);
	if (!result)
		context.reportError(ptrExpr, "Nothing to unpack, perhaps there are no instances of the type");
	return result;
}

bg::Expr::Ref ASTBoogieUtils::unpackInternal(Expression const* ptrExpr, boogie::Expr::Ref ptrBgExpr, Declaration const* decl, int depth, bg::Expr::Ref base, BoogieContext& context)
{
	// Unpacks a local storage pointer represented as an array of integers
	// into a conditional expression. The general idea is the opposite of packing:
	// go through each state variable (recursively for complex types) and associate
	// a conditional expression. For the example in pack, unpacking an array [arr]
	// would be like the following:
	// ite(arr[0] == 0, t1,
	// ite(arr[0] == 1,
	//   ite(arr[1] == 0, s1.t1, s1.ts[arr[2]], ... )))

	// Contract: go through state vars and create conditional expression recursively
	if (dynamic_cast<ContractDefinition const*>(decl))
	{
		auto structType = dynamic_cast<StructType const*>(ptrExpr->annotation().type);
		solAssert(structType, "Expected struct type when unpacking");
		// Collect all variables from all contracts that can see the struct
		vector<VariableDeclaration const*> vars;
		for (auto contr: context.stats().contractsForStruct(&structType->structDefinition()))
		{
			auto subVars = ASTNode::filteredNodes<VariableDeclaration>(contr->subNodes());
			vars.insert(vars.end(), subVars.begin(), subVars.end());
		}

		bg::Expr::Ref unpackedExpr = nullptr;
		for (unsigned i = 0; i < vars.size(); ++i)
		{
			auto subExpr = unpackInternal(ptrExpr, ptrBgExpr, vars[i], depth+1,
					bg::Expr::arrsel(bg::Expr::id(context.mapDeclName(*vars[i])), context.boogieThis()->getRefTo()), context);
			if (subExpr)
			{
				if (!unpackedExpr)
					unpackedExpr = subExpr;
				else
					unpackedExpr = bg::Expr::cond(
							bg::Expr::eq(bg::Expr::arrsel(ptrBgExpr, context.intLit(depth, 256)), context.intLit(i, 256)),
							subExpr, unpackedExpr);
			}
		}
		if (!unpackedExpr)
			context.reportError(ptrExpr, "Nothing to unpack, perhaps there are no instances of the type");
		return unpackedExpr;
	}
	// Variable (state var or struct member)
	else if (auto varDecl = dynamic_cast<VariableDeclaration const*>(decl))
	{
		auto targetTp = dynamic_cast<StructType const*>(ptrExpr->annotation().type);
		auto declTp = varDecl->type();

		// Get rid of arrays and mappings by indexing into them
		while (declTp->category() == Type::Category::Array || declTp->category() == Type::Category::Mapping)
		{
			if (auto arrType = dynamic_cast<ArrayType const*>(declTp))
			{
				auto bgType = context.toBoogieType(arrType->baseType(), ptrExpr);
				context.toBoogieType(arrType, ptrExpr); // TODO: this makes sure that the array types are declared even if the array itself is declared later
				base = bg::Expr::arrsel(
						context.getInnerArray(base, context.toBoogieType(arrType->baseType(), ptrExpr)),
						bg::Expr::arrsel(ptrBgExpr, context.intLit(depth, 256)));
				declTp = arrType->baseType();
			}
			else if (auto mapType = dynamic_cast<MappingType const*>(declTp))
			{
				base = bg::Expr::arrsel(base, bg::Expr::arrsel(ptrBgExpr, context.intLit(depth, 256)));
				declTp = mapType->valueType();
			}
			else
				solAssert(false, "Expected array or mapping type");
			depth++;
		}

		auto declStructTp = dynamic_cast<StructType const*>(declTp);

		// Found a variable with a matching type, just return
		if (targetTp && declStructTp && targetTp->structDefinition() == declStructTp->structDefinition())
		{
			return base;
		}
		// Otherwise if it is a struct, go through members and recurse
		if (declStructTp)
		{
			auto members = declStructTp->structDefinition().members();
			bg::Expr::Ref expr = nullptr;
			for (unsigned i = 0; i < members.size(); ++i)
			{
				auto baseForSub = bg::Expr::dtsel(base,
						context.mapDeclName(*members[i]),
						context.getStructConstructor(&declStructTp->structDefinition()),
						dynamic_pointer_cast<bg::DataTypeDecl>(context.getStructType(&declStructTp->structDefinition(), DataLocation::Storage)));
				auto subExpr = unpackInternal(ptrExpr, ptrBgExpr, members[i].get(), depth+1, baseForSub, context);
				if (subExpr)
				{
					if (!expr)
						expr = subExpr;
					else
						expr = bg::Expr::cond(
								bg::Expr::eq(bg::Expr::arrsel(ptrBgExpr, context.intLit(depth, 256)), context.intLit(i, 256)),
								subExpr, expr);
				}
			}
			return expr;
		}
	}
	return nullptr;
}

}
}
