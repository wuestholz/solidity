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
const string ASTBoogieUtils::SOLIDITY_MSG = "msg";
const string ASTBoogieUtils::SOLIDITY_SENDER = "sender";
const string ASTBoogieUtils::BOOGIE_MSG_SENDER = "__msg_sender";
const string ASTBoogieUtils::SOLIDITY_TRANSFER = "transfer";
const string ASTBoogieUtils::BOOGIE_TRANSFER = "__transfer";
const string ASTBoogieUtils::SOLIDITY_THIS = "this";
const string ASTBoogieUtils::BOOGIE_THIS = "__this";
const string ASTBoogieUtils::SOLIDITY_ASSERT = "assert";
const string ASTBoogieUtils::VERIFIER_MAIN = "__verifier_main";
const string ASTBoogieUtils::BOOGIE_CONSTRUCTOR = "__constructor";

smack::ProcDecl* ASTBoogieUtils::createTransferProc()
{
	// Parameters: this, msg.sender, amount
	list<smack::Binding> transferParams;
	transferParams.push_back(make_pair(BOOGIE_THIS, BOOGIE_ADDRESS_TYPE));
	transferParams.push_back(make_pair(BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE));
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

string ASTBoogieUtils::mapDeclName(Declaration const& decl)
{
	// Check for special names
	if (decl.name() == VERIFIER_MAIN) return "main";
	if (decl.name() == SOLIDITY_ASSERT) return "assert";
	if (decl.name() == SOLIDITY_THIS) return BOOGIE_THIS;

	// ID is important to append, since (1) even fully qualified names can be
	// same for state variables and local variables in functions, (2) return
	// variables might have no name (whereas Boogie requires a name)
	return decl.name() + "#" + to_string(decl.id());
}

string ASTBoogieUtils::mapType(TypePointer tp, ASTNode const& _associatedNode)
{
	// TODO: option for bit precise types
	string tpStr = tp->toString();
	if (tpStr == SOLIDITY_ADDRESS_TYPE) return BOOGIE_ADDRESS_TYPE;
	if (tpStr == "bool") return "bool";
	for (int i = 8; i <= 256; ++i)
	{
		if (tpStr == "int" + to_string(i)) return "int";
		if (tpStr == "uint" + to_string(i)) return "int";
	}
	if (boost::algorithm::starts_with(tpStr, "contract ")) return BOOGIE_ADDRESS_TYPE;

	BOOST_THROW_EXCEPTION(CompilerError() <<
			errinfo_comment("Unsupported type: " + tpStr) <<
			errinfo_sourceLocation(_associatedNode.location()));
	return "";
}



}
}
