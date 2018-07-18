#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/AST.h>
#include <boost/algorithm/string/predicate.hpp>

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
const string ASTBoogieUtils::BOOGIE_STRING_TYPE = "string_t";
const string ASTBoogieUtils::ERR_TYPE = "__ERROR_UNSUPPORTED_TYPE";

smack::ProcDecl* ASTBoogieUtils::createTransferProc()
{
	// Parameters: this, msg.sender, msg.value, amount
	list<smack::Binding> transferParams;
	transferParams.push_back(make_pair(BOOGIE_THIS, BOOGIE_ADDRESS_TYPE));
	transferParams.push_back(make_pair(BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE));
	transferParams.push_back(make_pair(ASTBoogieUtils::BOOGIE_MSG_VALUE, "int"));
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
	transfer->getRequires().push_back(smack::Expr::gte(sender_bal, amount));
	// Postcondition: if sender and receiver is different (ether gets transferred)
	transfer->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::neq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER)),
					smack::Expr::eq(sender_bal, smack::Expr::minus(smack::Expr::old(sender_bal), amount))));
	transfer->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::neq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER)),
					smack::Expr::eq(this_bal, smack::Expr::plus(smack::Expr::old(this_bal), amount))));
	// Postcondition: if sender and receiver is the same (nothing happens)
	transfer->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::eq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER)),
					smack::Expr::eq(sender_bal, smack::Expr::old(sender_bal))));
	transfer->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::eq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER)),
					smack::Expr::eq(this_bal, smack::Expr::old(this_bal))));

	return transfer;
}

smack::ProcDecl* ASTBoogieUtils::createCallProc()
{
	// Parameters: this, msg.sender, msg.value
	list<smack::Binding> callParams;
	callParams.push_back(make_pair(BOOGIE_THIS, BOOGIE_ADDRESS_TYPE));
	callParams.push_back(make_pair(BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE));
	callParams.push_back(make_pair(ASTBoogieUtils::BOOGIE_MSG_VALUE, "int"));

	// Return value
	list<smack::Binding> callReturns;
	callReturns.push_back(make_pair("__result", "bool"));

	// Body
	smack::Block* callImpl = smack::Block::block();
	callImpl->addStmt(smack::Stmt::comment("TODO: model something nondeterministic here"));
	list<smack::Block*> callBlocks;
	callBlocks.push_back(callImpl);
	smack::ProcDecl* callProc = smack::Decl::procedure(BOOGIE_CALL,
			callParams, callReturns, list<smack::Decl*>(), callBlocks);

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
	sendProc->getRequires().push_back(smack::Expr::gte(sender_bal, amount));
	// Postcondition: if result is true and sender/receiver is different (ether gets transferred)
	sendProc->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::and_(result, smack::Expr::neq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER))),
					smack::Expr::eq(sender_bal, smack::Expr::minus(smack::Expr::old(sender_bal), amount))));
	sendProc->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::and_(result, smack::Expr::neq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER))),
					smack::Expr::eq(this_bal, smack::Expr::plus(smack::Expr::old(this_bal), amount))));
	// Postcondition: if result is false or sender/receiver is the same (nothing happens)
	sendProc->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::or_(smack::Expr::not_(result), smack::Expr::eq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER))),
					smack::Expr::eq(sender_bal, smack::Expr::old(sender_bal))));
	sendProc->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::or_(smack::Expr::not_(result), smack::Expr::eq(smack::Expr::id(BOOGIE_THIS), smack::Expr::id(BOOGIE_MSG_SENDER))),
					smack::Expr::eq(this_bal, smack::Expr::old(this_bal))));

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

	// ID is important to append, since (1) even fully qualified names can be
	// same for state variables and local variables in functions, (2) return
	// variables might have no name (whereas Boogie requires a name)
	return decl.name() + "#" + to_string(decl.id());
}

string ASTBoogieUtils::mapType(TypePointer tp, ASTNode const& _associatedNode, ErrorReporter& errorReporter)
{
	// TODO: option for bit precise types
	string tpStr = tp->toString();
	if (tpStr == SOLIDITY_ADDRESS_TYPE) return BOOGIE_ADDRESS_TYPE;
	if (tpStr == "string storage ref") return BOOGIE_STRING_TYPE;
	if (tpStr == "string memory") return BOOGIE_STRING_TYPE;
	if (tpStr == "bool") return "bool";
	if (boost::algorithm::starts_with(tpStr, "int_const ")) return "int";
	for (int i = 8; i <= 256; ++i)
	{
		if (tpStr == "int" + to_string(i)) return "int";
		if (tpStr == "uint" + to_string(i)) return "int";
	}
	if (boost::algorithm::starts_with(tpStr, "contract ")) return BOOGIE_ADDRESS_TYPE;

	if (tp->category() == Type::Category::Array)
	{
		auto arrType = dynamic_cast<ArrayType const*>(&*tp);
		return "[int]" + mapType(arrType->baseType(), _associatedNode, errorReporter);
	}

	if (tp->category() == Type::Category::Mapping)
	{
		auto mappingType = dynamic_cast<MappingType const*>(&*tp);
		return "[" + mapType(mappingType->keyType(), _associatedNode, errorReporter) + "]"
				+ mapType(mappingType->valueType(), _associatedNode, errorReporter);
	}

	if (tp->category() == Type::Category::FixedBytes)
	{
		auto fbType = dynamic_cast<FixedBytesType const*>(&*tp);
		if (fbType->numBytes() == 1) return "int";
		else return "[int]int";
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



}
}
