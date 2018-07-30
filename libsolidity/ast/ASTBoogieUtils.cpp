#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/AST.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>
#include <libsolidity/parsing/Scanner.h>

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
// no constant required for 'throw' because it is a separate statement

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

smack::ProcDecl* ASTBoogieUtils::createTransferProc()
{
	// Parameters: this, msg.sender, msg.value, amount
	list<smack::Binding> transferParams;
	transferParams.push_back(make_pair(BOOGIE_THIS, BOOGIE_ADDRESS_TYPE));
	transferParams.push_back(make_pair(BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE));
	transferParams.push_back(make_pair(BOOGIE_MSG_VALUE, "int"));
	transferParams.push_back(make_pair("amount", "int"));

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
					smack::Expr::plus(this_bal, amount)
			)));
	// balance[msg.sender] -= amount
	transferImpl->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(
					smack::Expr::id(BOOGIE_BALANCE),
					smack::Expr::id(BOOGIE_MSG_SENDER),
					smack::Expr::minus(sender_bal, amount)
			)));
	transferImpl->addStmt(smack::Stmt::comment("TODO: call fallback, exception handling"));
	list<smack::Block*> transferBlocks;
	transferBlocks.push_back(transferImpl);
	smack::ProcDecl* transfer = smack::Decl::procedure(BOOGIE_TRANSFER,
			transferParams, list<smack::Binding>(), list<smack::Decl*>(), transferBlocks);

	// Precondition: there is enough ether to transfer
	transfer->getRequires().push_back(smack::Specification::spec(smack::Expr::gte(sender_bal, amount)));
	// Postcondition: if sender and receiver is different ether gets transferred, otherwise nothing happens
	transfer->getEnsures().push_back(smack::Specification::spec(smack::Expr::cond(
			smack::Expr::neq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER)),
			smack::Expr::and_(
					smack::Expr::eq(sender_bal, smack::Expr::minus(smack::Expr::old(sender_bal), amount)),
					smack::Expr::eq(this_bal, smack::Expr::plus(smack::Expr::old(this_bal), amount))),
			smack::Expr::and_(
					smack::Expr::eq(sender_bal, smack::Expr::old(sender_bal)),
					smack::Expr::eq(this_bal, smack::Expr::old(this_bal))))));

	return transfer;
}

smack::ProcDecl* ASTBoogieUtils::createCallProc()
{
	// Parameters: this, msg.sender, msg.value
	list<smack::Binding> callParams;
	callParams.push_back(make_pair(BOOGIE_THIS, BOOGIE_ADDRESS_TYPE));
	callParams.push_back(make_pair(BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE));
	callParams.push_back(make_pair(BOOGIE_MSG_VALUE, "int"));

	// Return value
	list<smack::Binding> callReturns;
	callReturns.push_back(make_pair("__result", "bool"));

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
					smack::Expr::plus(this_bal, msg_val)
			)));
	thenBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(true)));
	// Unsuccessful transfer
	smack::Block* elseBlock = smack::Block::block();
	elseBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(false)));
	// Nondeterministic choice between success and failure
	smack::Block* callBlock = smack::Block::block();
	callBlock->addStmt(smack::Stmt::comment("TODO: call fallback"));
	callBlock->addStmt(smack::Stmt::ifelse(smack::Expr::id("*"), thenBlock, elseBlock));

	list<smack::Block*> callBlocks;
	callBlocks.push_back(callBlock);
	smack::ProcDecl* callProc = smack::Decl::procedure(BOOGIE_CALL, callParams, callReturns, list<smack::Decl*>(), callBlocks);
	// Postcondition: if result is false nothing happens
	callProc->getEnsures().push_back(smack::Specification::spec(smack::Expr::or_(result,
			smack::Expr::eq(smack::Expr::id(BOOGIE_BALANCE), smack::Expr::old(smack::Expr::id(BOOGIE_BALANCE))))));
	return callProc;
}

smack::ProcDecl* ASTBoogieUtils::createSendProc()
{
	// Parameters: this, msg.sender, msg.value, amount
	list<smack::Binding> sendParams;
	sendParams.push_back(make_pair(BOOGIE_THIS, BOOGIE_ADDRESS_TYPE));
	sendParams.push_back(make_pair(BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE));
	sendParams.push_back(make_pair(ASTBoogieUtils::BOOGIE_MSG_VALUE, "int"));
	sendParams.push_back(make_pair("amount", "int"));

	// Return value
	list<smack::Binding> sendReturns;
	sendReturns.push_back(make_pair("__result", "bool"));

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
					smack::Expr::plus(this_bal, amount)
			)));
	// balance[msg.sender] -= amount
	thenBlock->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(
					smack::Expr::id(BOOGIE_BALANCE),
					smack::Expr::id(BOOGIE_MSG_SENDER),
					smack::Expr::minus(sender_bal, amount)
			)));
	thenBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(true)));
	// Unsuccessful transfer
	smack::Block* elseBlock = smack::Block::block();
	elseBlock->addStmt(smack::Stmt::assign(result, smack::Expr::lit(false)));
	// Nondeterministic choice between success and failure
	smack::Block* transferBlock = smack::Block::block();
	transferBlock->addStmt(smack::Stmt::comment("TODO: call fallback"));
	transferBlock->addStmt(smack::Stmt::ifelse(smack::Expr::id("*"), thenBlock, elseBlock));

	list<smack::Block*> transferBlocks;
	transferBlocks.push_back(transferBlock);
	smack::ProcDecl* sendProc = smack::Decl::procedure(BOOGIE_SEND,
			sendParams, sendReturns, list<smack::Decl*>(), transferBlocks);

	// Precondition: there is enough ether to transfer
	sendProc->getRequires().push_back(smack::Specification::spec(smack::Expr::gte(sender_bal, amount)));
	// Postcondition: if result is true and sender/receiver is different ether gets transferred
	// otherwise nothing happens
	sendProc->getEnsures().push_back(smack::Specification::spec(smack::Expr::cond(
			smack::Expr::and_(result, smack::Expr::neq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER))),
			smack::Expr::and_(
					smack::Expr::eq(sender_bal, smack::Expr::minus(smack::Expr::old(sender_bal), amount)),
					smack::Expr::eq(this_bal, smack::Expr::plus(smack::Expr::old(this_bal), amount))),
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

string ASTBoogieUtils::mapType(TypePointer tp, ASTNode const& _associatedNode, ErrorReporter& errorReporter, bool bitPrecise)
{
	string tpStr = tp->toString();
	if (tpStr == SOLIDITY_ADDRESS_TYPE) return BOOGIE_ADDRESS_TYPE;
	if (tpStr == "string storage ref") return BOOGIE_STRING_TYPE;
	if (tpStr == "string memory") return BOOGIE_STRING_TYPE;
	if (tpStr == "bool") return "bool";
	// For literals we return 'int_const' even in bit-precise mode, they will be
	// converted to appropriate type when assigned to something
	if (boost::algorithm::starts_with(tpStr, "int_const ")) return "int_const";
	for (int i = 8; i <= 256; ++i)
	{
		if (tpStr == "int" + to_string(i)) return bitPrecise ? "bv" + to_string(i) : "int";
		if (tpStr == "uint" + to_string(i)) return bitPrecise ? "bv" + to_string(i) : "int";
	}
	if (boost::algorithm::starts_with(tpStr, "contract ")) return BOOGIE_ADDRESS_TYPE;

	if (tp->category() == Type::Category::Array)
	{
		auto arrType = dynamic_cast<ArrayType const*>(&*tp);
		return "[" + string(bitPrecise ? "bv256" : "int") + "]" + mapType(arrType->baseType(), _associatedNode, errorReporter, bitPrecise);
	}

	if (tp->category() == Type::Category::Mapping)
	{
		auto mappingType = dynamic_cast<MappingType const*>(&*tp);
		return "[" + mapType(mappingType->keyType(), _associatedNode, errorReporter, bitPrecise) + "]"
				+ mapType(mappingType->valueType(), _associatedNode, errorReporter, bitPrecise);
	}

	if (tp->category() == Type::Category::FixedBytes)
	{
		auto fbType = dynamic_cast<FixedBytesType const*>(&*tp);
		if (bitPrecise)
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
		errorReporter.error(Error::Type::ParserError, _associatedNode.location(), "Tuples are not supported");
	}
	else
	{
		errorReporter.error(Error::Type::ParserError, _associatedNode.location(), "Unsupported type: '" + tpStr.substr(0, tpStr.find(' ')) + "'");
	}
	return ERR_TYPE;
}


list<const smack::Attr*> ASTBoogieUtils::createLocAttrs(SourceLocation const& loc, std::string const& message, Scanner const& scanner)
{
	list<const smack::Attr*> attrs;
	int srcLine, srcCol;
	tie(srcLine, srcCol) = scanner.translatePositionToLineColumn(loc.start);
	attrs.push_back(smack::Attr::attr("sourceloc", *loc.sourceName, srcLine + 1, srcCol + 1));
	attrs.push_back(smack::Attr::attr("message", message));
	return attrs;
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
	return stoi(typeStr.substr(typeStr.find("t") + 1));
}

bool ASTBoogieUtils::isSigned(TypePointer type)
{
	if (!isBitPreciseType(type)) { return false; }
	string typeStr = type->toString();
	return typeStr[0] == 'i';
}

smack::Expr const* ASTBoogieUtils::checkImplicitBvConversion(smack::Expr const* expr, TypePointer exprType, TypePointer targetType, map<string, smack::FuncDecl*>& bvBuiltin)
{
	if (!targetType || !exprType) { return expr; }

	if (isBitPreciseType(targetType))
	{
		unsigned targetBits = getBits(targetType);
		if (auto exprLit = dynamic_cast<smack::IntLit const*>(expr))
		{
			if (exprLit->getVal() < 0)
			{
				string fullName = "bv" + to_string(targetBits) + "neg";
				// TODO: check if exists
				bvBuiltin[fullName] = smack::Decl::function(
								fullName, {make_pair("", "bv"+to_string(targetBits))}, "bv"+to_string(targetBits), nullptr,
								{smack::Attr::attr("bvbuiltin", "bvneg")});
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
			if (targetBits < exprBits) { BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Implicit conversion to smaller type")); }

			if (!exprSigned) // Unsigned can be converted to larger (signed or unsigned) with zero extension
			{
				string fullName = "bvzeroext" + to_string(exprBits) + "to" + to_string(targetBits);
				// TODO: check if exists
				bvBuiltin[fullName] = smack::Decl::function(
						fullName, {make_pair("", "bv"+to_string(exprBits))}, "bv"+to_string(targetBits), nullptr,
						{smack::Attr::attr("bvbuiltin", "zero_extend " + to_string(targetBits - exprBits))});
				return smack::Expr::fn(fullName, expr);
			}
			else if (targetSigned) // Signed can only be converted to signed with sign extension
			{
				string fullName = "bvsignext" + to_string(exprBits) + "to" + to_string(targetBits);
				bvBuiltin[fullName] = smack::Decl::function(
						fullName, {make_pair("", "bv"+to_string(exprBits))}, "bv"+to_string(targetBits), nullptr,
						{smack::Attr::attr("bvbuiltin", "sign_extend " + to_string(targetBits - exprBits))});
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


}
}
